/*  IIPImage image processing routines: mamespaced, templated, optimized and
    threaded implementations of nearest neighbour, bilinear, bicubic,
    lanczos 2 and lanczos 3 resampling. Called from Transforms.cc.

    Copyright (C) 2026 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/



#include "Transforms_Resample.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <tuple>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#if defined(_MSC_VER)
#define TRANSFORM_RESAMPLE_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define TRANSFORM_RESAMPLE_RESTRICT __restrict__
#else
#define TRANSFORM_RESAMPLE_RESTRICT
#endif

namespace resample {

  // Mirrors PARALLEL_THRESHOLD in Transforms.cc: only thread work this large.
  constexpr long long PARALLEL_THRESHOLD = 65536;

  // Clamp a floating point sample to the range of SampleT. Truncation is used
  // by the bilinear path to preserve its existing 8-bit rounding behaviour;
  // the separable resampler rounds its vertical output as before.
  template <typename SampleT>
  inline SampleT clamp_truncate(float v){
    const float max_val = static_cast<float>(std::numeric_limits<SampleT>::max());
    if( v >= max_val )
      return std::numeric_limits<SampleT>::max();
    if( !(v > 0.0f) )
      return static_cast<SampleT>(0);
    // Truncate via a SampleT cast: for unsigned SampleT the guards above keep
    // the truncated value representable, avoiding the undefined float->int
    // conversion that an intermediate (int) cast would trigger for 32-bit
    // samples above INT_MAX.
    return static_cast<SampleT>(v);
  }


  template <typename SampleT>
  inline SampleT clamp_round(float v){
    const float max_val = static_cast<float>(std::numeric_limits<SampleT>::max());
    if( v >= max_val - 0.5f )
      return std::numeric_limits<SampleT>::max();
    if( !(v > 0.0f) )
      return static_cast<SampleT>(0);
    return static_cast<SampleT>(v + 0.5f);
  }


  // Floating-point samples need no clamping: negative and fractional values are
  // valid, so the value is passed through unchanged.
  template <>
  inline float clamp_truncate<float>(float v){
    return v;
  }


  template <>
  inline float clamp_round<float>(float v){
    return v;
  }


  // Select the source pixel containing the destination pixel center using
  // integer arithmetic. The quotient/remainder form avoids overflowing the
  // intermediate (2 * destination + 1) * input_size.
  inline uint64_t nearest_center_index(uint64_t destination, uint64_t input_size,
                                     uint64_t output_size){
    const uint64_t product = destination * input_size;
    const uint64_t quotient = product / output_size;
    const uint64_t remainder = product % output_size;
    const uint64_t adjustment = (2 * remainder + input_size) / (2 * output_size);
    return std::min(input_size - 1, quotient + adjustment);
  }


  // Kernel callback: returns the filter value at distance x for the given
  // support radius. Only used while building weight tables, never in the hot
  // loops.
  typedef float (*KernelFn)(float x, int support);


  // Precomputed, normalized tap list for one axis (CSR-style layout).
  //
  // starts[x] is the first input index used by output position x. The weights
  // for that position are weights[offsets[x] .. offsets[x+1]-1] and correspond
  // to consecutive input indices starts[x], starts[x]+1, ... Keeping the taps
  // contiguous (including zero weights) turns the inner loops into plain dot
  // products that the compiler can vectorize.
  struct WeightTable {
    std::vector<size_t> offsets; // size out_size + 1
    std::vector<int> starts;     // size out_size
    std::vector<float> weights;
  };


  // RawTile stores byte lengths and capacities in uint32_t. Check the output
  // product before allocating or constructing any resampling tables.
  inline bool checked_output_samples(size_t width, size_t height, size_t channels,
				     size_t sample_bytes, size_t& samples){
    const size_t max_size = std::numeric_limits<size_t>::max();
    if( width != 0 && height > max_size / width )
      return false;
    size_t pixels = width * height;
    if( channels != 0 && pixels > max_size / channels )
      return false;
    samples = pixels * channels;
    if( sample_bytes != 0 && samples > max_size / sample_bytes )
      return false;
    return samples * sample_bytes <= std::numeric_limits<uint32_t>::max();
  }


  inline bool weight_table_sizes_valid(size_t out_size, size_t in_size,
				       int support){
    if( out_size == 0 || in_size == 0 ||
	out_size == std::numeric_limits<size_t>::max() )
      return false;
    // starts stores source indices as int and the builder uses int loop indices.
    if( in_size > static_cast<size_t>(std::numeric_limits<int>::max()) )
      return false;
    const float scale =
      static_cast<float>(in_size) / static_cast<float>(out_size);
    const float radius = static_cast<float>(support) * std::max(1.0f, scale);
    const double tap_bound = 2.0 * std::ceil(static_cast<double>(radius)) + 1.0;
    const size_t max_taps = tap_bound >= static_cast<double>(in_size)
                                ? in_size
                                : static_cast<size_t>(tap_bound);
    const std::vector<size_t> offsets;
    const std::vector<int> starts;
    const std::vector<float> weights;
    return out_size + 1 <= offsets.max_size() && out_size <= starts.max_size() &&
                           out_size <= weights.max_size() &&
                           (max_taps == 0 || out_size <= weights.max_size() / max_taps);
  }

  
  // Build per-output-position weights once, reused across the other axis.
  // Uses pixel-center mapping and stretches the kernel when minifying to
  // avoid aliasing.
  inline WeightTable build_weights(size_t out_size, size_t in_size, int support,
				   KernelFn kernel){
    WeightTable table;
    const float scale =
      static_cast<float>(in_size) / static_cast<float>(out_size);
    const float filter_scale = std::max(1.0f, scale);
    const float radius = static_cast<float>(support) * filter_scale;

    const double tap_bound = 2.0 * std::ceil(static_cast<double>(radius)) + 1.0;
    const size_t max_taps = tap_bound >= static_cast<double>(in_size)
                                ? in_size
                                : static_cast<size_t>(tap_bound);
    if( out_size == std::numeric_limits<size_t>::max() ||
	(max_taps != 0 &&
	 out_size > std::numeric_limits<size_t>::max() / max_taps) )
      throw std::length_error("resampling weight table size overflow");
    const size_t max_weights = out_size * max_taps;
    if( max_weights > table.weights.max_size() ||
	out_size > table.starts.max_size() ||
	out_size + 1 > table.offsets.max_size() )
      throw std::length_error("resampling weight table too large");
    table.offsets.reserve(out_size + 1);
    table.starts.reserve(out_size);
    table.weights.reserve(max_weights);
    table.offsets.push_back(0);

    for( size_t pos = 0; pos < out_size; ++pos ){
      const float center = (static_cast<float>(pos) + 0.5f) * scale - 0.5f;
      int left = static_cast<int>(std::ceil(center - radius));
      int right = static_cast<int>(std::floor(center + radius));
      left = std::max(left, 0);
      right = std::min(right, static_cast<int>(in_size) - 1);

      const size_t start = table.weights.size();
      float sum = 0.0f;
      for( int i = left; i <= right; ++i ){
	const float w =
          kernel((center - static_cast<float>(i)) / filter_scale, support);
	table.weights.push_back(w);
	sum += w;
      }

      if( sum == 0.0f ){
	// Degenerate case: fall back to nearest neighbour
	const int nearest =
          std::min(static_cast<int>(in_size) - 1,
                   std::max(0, static_cast<int>(std::floor(center + 0.5f))));
	table.weights.resize(start);
	table.weights.push_back(1.0f);
	table.starts.push_back(nearest);
      }
      else{
	const float inv = 1.0f / sum;
	for( size_t k = start; k < table.weights.size(); ++k )
	  table.weights[k] *= inv;
	table.starts.push_back(left);
      }
      table.offsets.push_back(table.weights.size());
    }
    return table;
  }


  // Cache weight tables per thread. std::map is node-based, so references
  // returned here stay valid as the cache grows. The cache is bounded because
  // CVT clients can request arbitrary output sizes; without a limit a long-lived
  // server would accumulate one entry per unique request size. Leave room for
  // the two axis tables inserted by a single resize.
  typedef std::tuple<size_t, size_t, int, std::uintptr_t> WeightKey;


  constexpr size_t MAX_WEIGHT_CACHE_ENTRIES = 32;


  inline std::map<WeightKey, WeightTable>& weight_cache(){
    static thread_local std::map<WeightKey, WeightTable> cache;
    return cache;
  }


  inline void trim_weight_cache(){
    std::map<WeightKey, WeightTable>& cache = weight_cache();
    if( cache.size() >= MAX_WEIGHT_CACHE_ENTRIES - 1 )
      cache.clear();
  }


  inline const WeightTable& cached_weights(size_t out_size, size_t in_size,
					   int support, KernelFn kernel){
    std::map<WeightKey, WeightTable>& cache = weight_cache();
    const WeightKey key(out_size, in_size, support,
			reinterpret_cast<std::uintptr_t>(kernel));
    std::map<WeightKey, WeightTable>::iterator it = cache.find(key);
    if( it == cache.end() )
      it = cache.insert(std::make_pair( key, build_weights(out_size, in_size, support, kernel))).first;
    return it->second;
  }


  // Three-channel horizontal pass. Keeping the three accumulators in registers
  // avoids the awkward 3-wide vector case while still reading the interleaved
  // input contiguously.
  template <typename SampleT>
  inline void horizontal_row_rgb(const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow,
				 float* TRANSFORM_RESAMPLE_RESTRICT outrow,
				 size_t new_width, const WeightTable& w){
    for( size_t x = 0; x < new_width; ++x ){
      const size_t begin = w.offsets[x];
      const size_t end = w.offsets[x + 1];
      const SampleT* TRANSFORM_RESAMPLE_RESTRICT p =
        inrow + static_cast<size_t>(w.starts[x]) * 3;
      const float* TRANSFORM_RESAMPLE_RESTRICT wt = w.weights.data() + begin;
      const size_t taps = end - begin;
      float r = 0.0f, g = 0.0f, b = 0.0f;
      for( size_t i = 0; i < taps; ++i ){
	const float f = wt[i];
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT q = p + i * 3;
	r += static_cast<float>(q[0]) * f;
	g += static_cast<float>(q[1]) * f;
	b += static_cast<float>(q[2]) * f;
      }
      float* TRANSFORM_RESAMPLE_RESTRICT o = outrow + x * 3;
      o[0] = r;
      o[1] = g;
      o[2] = b;
    }
  }


  // Generic fixed-channel horizontal pass (2, 4, ... channels). The channel
  // loop is contiguous and vectorizes well for the common 4-channel case.
  template <typename SampleT, int C>
  inline void horizontal_row(const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow,
			     float* TRANSFORM_RESAMPLE_RESTRICT outrow,
			     size_t new_width, const WeightTable& w){
    for( size_t x = 0; x < new_width; ++x ){
      const size_t begin = w.offsets[x];
      const size_t end = w.offsets[x + 1];
      const SampleT* TRANSFORM_RESAMPLE_RESTRICT p =
        inrow + static_cast<size_t>(w.starts[x]) * C;
      const float* TRANSFORM_RESAMPLE_RESTRICT wt = w.weights.data() + begin;
      const size_t taps = end - begin;
      float acc[C] = {};
      for( size_t i = 0; i < taps; ++i ){
	const float f = wt[i];
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT q = p + i * C;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( int k = 0; k < C; ++k )
	  acc[k] += static_cast<float>(q[k]) * f;
      }
      float* TRANSFORM_RESAMPLE_RESTRICT o = outrow + x * C;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
      for( int k = 0; k < C; ++k )
	o[k] = acc[k];
    }
  }


  // Runtime-channel horizontal pass. The accumulator is allocated once per
  // thread/row rather than once per pixel.
  template <typename SampleT>
  inline void
  horizontal_row_generic(const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow,
			 float* TRANSFORM_RESAMPLE_RESTRICT outrow,
			 size_t new_width, size_t channels, const WeightTable& w,
			 float* TRANSFORM_RESAMPLE_RESTRICT acc){
    for( size_t x = 0; x < new_width; ++x ){
      const size_t begin = w.offsets[x];
      const size_t end = w.offsets[x + 1];
      const SampleT* TRANSFORM_RESAMPLE_RESTRICT p =
        inrow + static_cast<size_t>(w.starts[x]) * channels;
      const float* TRANSFORM_RESAMPLE_RESTRICT wt = w.weights.data() + begin;
      const size_t taps = end - begin;

      std::fill(acc, acc + channels, 0.0f);
      for( size_t i = 0; i < taps; ++i ){
	const float f = wt[i];
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT q = p + i * channels;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( size_t k = 0; k < channels; ++k )
	  acc[k] += static_cast<float>(q[k]) * f;
      }
      float* TRANSFORM_RESAMPLE_RESTRICT o = outrow + x * channels;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
      for( size_t k = 0; k < channels; ++k )
	o[k] = acc[k];
    }
  }


  // Horizontal resampling of a single row with on-the-fly deinterleaving. Each
  // input row is converted once into contiguous float planes, so the convolution
  // inner loops are plain float dot products instead of repeatedly converting
  // input samples for every tap. Output is written back interleaved so the
  // vertical pass stays contiguous.
  template <typename SampleT>
  inline void
  horizontal_row_deinterleaved(const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow,
			       float* TRANSFORM_RESAMPLE_RESTRICT outrow,
			       size_t width, size_t new_width, size_t channels,
			       const WeightTable& xweights,
			       float* TRANSFORM_RESAMPLE_RESTRICT planes){
    // Deinterleave and convert this row to planar floats.
    for( size_t c = 0; c < channels; ++c ){
      float* TRANSFORM_RESAMPLE_RESTRICT plane = planes + c * width;
      for( size_t x = 0; x < width; ++x )
	plane[x] = static_cast<float>(inrow[x * channels + c]);
    }

    // Convolve each channel independently.
    for( size_t c = 0; c < channels; ++c ){
      const float* TRANSFORM_RESAMPLE_RESTRICT plane = planes + c * width;
      for( size_t x = 0; x < new_width; ++x ){
	const size_t begin = xweights.offsets[x];
	const size_t end = xweights.offsets[x + 1];
	const float* TRANSFORM_RESAMPLE_RESTRICT p = plane + xweights.starts[x];
	const float* TRANSFORM_RESAMPLE_RESTRICT wt =
          xweights.weights.data() + begin;
	const size_t taps = end - begin;
	float acc = 0.0f;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd reduction(+ : acc)
#endif
	for( size_t i = 0; i < taps; ++i )
	  acc += p[i] * wt[i];
	outrow[x * channels + c] = acc;
      }
    }
  }


  // Horizontal pass variants. The deinterleaved path wins when minifying because
  // each input sample is reused by many taps; the interleaved paths avoid the
  // deinterleave overhead when magnifying and keep the output stores contiguous.
  enum HorizontalVariant {
    HORIZONTAL_DEINTERLEAVED,
    HORIZONTAL_RGB,
    HORIZONTAL_C2,
    HORIZONTAL_C4,
    HORIZONTAL_GENERIC
  };


  inline HorizontalVariant
  choose_horizontal_variant(size_t width, size_t new_width, size_t channels){
    if( channels == 1 || width > new_width )
      return HORIZONTAL_DEINTERLEAVED;
    switch( channels ){
      case 2:
	return HORIZONTAL_C2;
      case 3:
	return HORIZONTAL_RGB;
      case 4:
	return HORIZONTAL_C4;
      default:
	return HORIZONTAL_GENERIC;
    }
  }


  // Scratch floats needed by a horizontal row variant, excluding the output row.
  inline size_t horizontal_scratch_size(HorizontalVariant variant, size_t width,
					size_t channels){
    if( variant == HORIZONTAL_DEINTERLEAVED )
      return width * channels;
    if( variant == HORIZONTAL_GENERIC )
      return channels;
    return 0;
  }


  template <typename SampleT>
  inline void horizontal_row_dispatch(
				      const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow,
				      float* TRANSFORM_RESAMPLE_RESTRICT outrow, size_t width, size_t new_width,
				      size_t channels, const WeightTable& xweights,
				      float* TRANSFORM_RESAMPLE_RESTRICT scratch, HorizontalVariant variant){
    switch( variant ){
    case HORIZONTAL_DEINTERLEAVED:
      horizontal_row_deinterleaved<SampleT>(inrow, outrow, width, new_width,
					    channels, xweights, scratch);
      break;
    case HORIZONTAL_RGB:
      horizontal_row_rgb<SampleT>(inrow, outrow, new_width, xweights);
      break;
    case HORIZONTAL_C2:
      horizontal_row<SampleT, 2>(inrow, outrow, new_width, xweights);
      break;
    case HORIZONTAL_C4:
      horizontal_row<SampleT, 4>(inrow, outrow, new_width, xweights);
      break;
    default:
      horizontal_row_generic<SampleT>(inrow, outrow, new_width, channels,
				      xweights, scratch);
      break;
    }
  }


  // Largest number of vertical taps used by any output row.
  inline size_t max_taps(const WeightTable& w){
    size_t m = 1;
    for( size_t y = 0; y + 1 < w.offsets.size(); ++y ){
      const size_t taps = w.offsets[y + 1] - w.offsets[y];
      if( taps > m )
	m = taps;
    }
    return m;
  }


  // Choose how many threads to use for a row-chunked pass. Each thread
  // recomputes up to taps_max rows of halo at its chunk boundary; that overhead
  // is bounded by the parallel-work threshold, so use the full team but never
  // more threads than there are rows to produce.
  inline size_t row_chunk_threads(size_t rows, bool parallel){
    if( !parallel )
      return 1;
    size_t limit = 1;
#if defined(_OPENMP)
    limit = static_cast<size_t>(omp_get_max_threads());
#endif
    return std::min(limit, std::max<size_t>(1, rows));
  }


  // Separable resampling with a per-thread ring of horizontally resampled rows.
  //
  // Each thread owns a contiguous range of output rows. It keeps the last
  // taps_max horizontally filtered rows in a circular buffer and computes only
  // the rows that the next output row actually needs. This bounds the
  // intermediate to O(taps_max * new_width) instead of O(height * new_width) and
  // keeps it cache resident, so the vertical pass does not re-read DRAM. The tap
  // order and accumulation order match the previous full-frame implementation
  // exactly.
  template <typename SampleT>
  void resample_ring(const SampleT* input, size_t width, size_t height,
		     size_t channels, SampleT* output, size_t new_width,
		     size_t new_height, const WeightTable& xweights,
		     const WeightTable& yweights, HorizontalVariant variant,
		     size_t taps_max, bool parallel){
    const size_t row_len = new_width * channels;
    const size_t scratch_len = horizontal_scratch_size(variant, width, channels);
    // Either pass can dominate: minification does more horizontal work, while
    // magnification does more vertical work. Parallelize if either is large.
    const long long input_work =
      static_cast<long long>(height) * static_cast<long long>(row_len);
    const long long output_work =
      static_cast<long long>(new_height) * static_cast<long long>(row_len);
    const bool use_parallel =
      parallel && std::max(input_work, output_work) > PARALLEL_THRESHOLD;
    const size_t threads = row_chunk_threads(new_height, use_parallel);

    std::atomic<bool> alloc_failed(false);

#if defined(_OPENMP)
#pragma omp parallel if( use_parallel ) num_threads(threads)
#endif
    {
      size_t team = 1;
      size_t thread = 0;
#if defined(_OPENMP)
      team = static_cast<size_t>(omp_get_num_threads());
      thread = static_cast<size_t>(omp_get_thread_num());
#endif
      const size_t chunk = (new_height + team - 1) / team;
      const size_t y_begin = std::min(new_height, thread * chunk);
      const size_t y_end = std::min(new_height, y_begin + chunk);

      if( y_begin < y_end ){
	// Every element is written before it is read, so avoid
	// value-initializing these buffers.
	std::unique_ptr<float[]> ring( new (std::nothrow) float[taps_max * row_len] );
	std::unique_ptr<float[]> acc( new (std::nothrow) float[row_len] );
	std::unique_ptr<float[]> scratch( new (std::nothrow) float[scratch_len ? scratch_len : 1] );

	if( !ring || !acc || !scratch ){
	  alloc_failed.store(true, std::memory_order_relaxed);
	}
	else{
	  float* TRANSFORM_RESAMPLE_RESTRICT ring_ptr = ring.get();
	  float* TRANSFORM_RESAMPLE_RESTRICT acc_ptr = acc.get();
	  float* TRANSFORM_RESAMPLE_RESTRICT scratch_ptr = scratch.get();

	  size_t next_row = static_cast<size_t>(yweights.starts[y_begin]);

	  for( size_t y = y_begin; y < y_end; ++y ){
	    if( alloc_failed.load(std::memory_order_relaxed) ){
	      break;
	    }

	    const size_t begin = yweights.offsets[y];
	    const size_t end = yweights.offsets[y + 1];
	    SampleT* TRANSFORM_RESAMPLE_RESTRICT dst = output + y * row_len;

	    if( begin == end ){
	      std::fill(dst, dst + row_len, static_cast<SampleT>(0));
	      continue;
	    }

	    const size_t taps = end - begin;
	    const size_t start = static_cast<size_t>(yweights.starts[y]);

	    // starts[] is non-decreasing, so rows before start are no
	    // longer needed: drop them instead of filtering them.
	    if( next_row < start ){
	      next_row = start;
	    }

	    const size_t needed_end = start + taps;
	    for( size_t r = next_row; r < needed_end; ++r ){
	      const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow =
                input + r * width * channels;
	      float* TRANSFORM_RESAMPLE_RESTRICT ringrow =
                ring_ptr + (r % taps_max) * row_len;
	      horizontal_row_dispatch<SampleT>(inrow, ringrow, width, new_width,
					       channels, xweights, scratch_ptr,
					       variant);
	    }
	    if( needed_end > next_row ){
	      next_row = needed_end;
	    }

	    const float* TRANSFORM_RESAMPLE_RESTRICT wt =
              yweights.weights.data() + begin;
	    const float* TRANSFORM_RESAMPLE_RESTRICT in =
              ring_ptr + (start % taps_max) * row_len;
	    const float w0 = wt[0];
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	    for( size_t n = 0; n < row_len; ++n ){
	      acc_ptr[n] = in[n] * w0;
	    }

	    for( size_t i = 1; i < taps; ++i ){
	      const float* TRANSFORM_RESAMPLE_RESTRICT in2 =
                ring_ptr + ((start + i) % taps_max) * row_len;
	      const float wi = wt[i];
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	      for( size_t n = 0; n < row_len; ++n )
		acc_ptr[n] += in2[n] * wi;
	    }

#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	    for( size_t n = 0; n < row_len; ++n )
	      dst[n] = clamp_round<SampleT>(acc_ptr[n]);
	  }
	}
      }
    }

    if( alloc_failed.load(std::memory_order_relaxed) )
      throw std::bad_alloc();
  }

  
  // Horizontal-only resampling: the vertical axis is unchanged, so the filtered
  // rows can be written straight to the output with no intermediate buffer.
  template <typename SampleT>
  void resample_horizontal_only(const SampleT* input, size_t width, size_t height,
				size_t channels, SampleT* output,
				size_t new_width, const WeightTable& xweights,
				HorizontalVariant variant, bool parallel){
    const size_t in_row_len = width * channels;
    const size_t out_row_len = new_width * channels;
    const size_t scratch_len = horizontal_scratch_size(variant, width, channels);
    const bool use_parallel =
      parallel && static_cast<long long>(height) * static_cast<long long>(out_row_len) > PARALLEL_THRESHOLD;

    std::atomic<bool> alloc_failed(false);

#if defined(_OPENMP)
#pragma omp parallel if( use_parallel )
#endif
    {
      std::unique_ptr<float[]> row(new (std::nothrow) float[out_row_len]);
      std::unique_ptr<float[]> scratch( new (std::nothrow) float[scratch_len ? scratch_len : 1] );

      if( !row || !scratch ){
	alloc_failed.store(true, std::memory_order_relaxed);
      }

      float* TRANSFORM_RESAMPLE_RESTRICT row_ptr = row.get();
      float* TRANSFORM_RESAMPLE_RESTRICT scratch_ptr = scratch.get();
#if defined(_OPENMP)
#pragma omp for schedule(static)
#endif
      for( long long y = 0; y < static_cast<long long>(height); ++y ){
	if( alloc_failed.load(std::memory_order_relaxed) ) continue;

	const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow =
          input + static_cast<size_t>(y) * in_row_len;
	horizontal_row_dispatch<SampleT>(inrow, row_ptr, width, new_width,
					 channels, xweights, scratch_ptr,
					 variant);
	SampleT* TRANSFORM_RESAMPLE_RESTRICT dst =
          output + static_cast<size_t>(y) * out_row_len;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( size_t n = 0; n < out_row_len; ++n ){
	  dst[n] = clamp_round<SampleT>(row_ptr[n]);
	}
      }
    }

    if( alloc_failed.load(std::memory_order_relaxed) ){
      throw std::bad_alloc();
    }
  }


  // Vertical-only resampling: the horizontal axis is unchanged, so each output
  // row is accumulated directly from the input rows with no intermediate buffer.
  template <typename SampleT>
  void resample_vertical_only(const SampleT* input, size_t width, size_t channels,
			      SampleT* output, size_t new_height,
			      const WeightTable& yweights, bool parallel){
    const size_t row_len = width * channels;
    const bool use_parallel =
      parallel &&
      static_cast<long long>(new_height) * static_cast<long long>(row_len) >
      PARALLEL_THRESHOLD;

    std::atomic<bool> alloc_failed(false);

#if defined(_OPENMP)
#pragma omp parallel if( use_parallel )
#endif
    {
      std::unique_ptr<float[]> acc(new (std::nothrow) float[row_len]);

      if( !acc ){
	alloc_failed.store(true, std::memory_order_relaxed);
      }

      float* TRANSFORM_RESAMPLE_RESTRICT acc_ptr = acc.get();
#if defined(_OPENMP)
#pragma omp for schedule(static)
#endif
      for( long long yy = 0; yy < static_cast<long long>(new_height); ++yy ){
	if( alloc_failed.load(std::memory_order_relaxed) ) continue;

	const size_t y = static_cast<size_t>(yy);
	const size_t begin = yweights.offsets[y];
	const size_t end = yweights.offsets[y + 1];
	SampleT* TRANSFORM_RESAMPLE_RESTRICT dst = output + y * row_len;

	if( begin == end ){
	  std::fill(dst, dst + row_len, static_cast<SampleT>(0));
	  continue;
	}

	const size_t taps = end - begin;
	const size_t start = static_cast<size_t>(yweights.starts[y]);
	const float* TRANSFORM_RESAMPLE_RESTRICT wt = yweights.weights.data() + begin;
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT in = input + start * row_len;

	const float w0 = wt[0];
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( size_t n = 0; n < row_len; ++n ){
	  acc_ptr[n] = static_cast<float>(in[n]) * w0;
	}

	for( size_t i = 1; i < taps; ++i ){
	  const SampleT* TRANSFORM_RESAMPLE_RESTRICT in2 = in + i * row_len;
	  const float wi = wt[i];
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	  for( size_t n = 0; n < row_len; ++n )
	    acc_ptr[n] += static_cast<float>(in2[n]) * wi;
	}

#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( size_t n = 0; n < row_len; ++n )
	  dst[n] = clamp_round<SampleT>(acc_ptr[n]);
      }
    }

    if( alloc_failed.load(std::memory_order_relaxed) )
      throw std::bad_alloc();
  }


  // Separable resampling of an interleaved 8/16-bit image into a caller-provided
  // output buffer. The caller owns both the input and output buffers.
  template <typename SampleT>
  inline void resample(const SampleT* input, size_t original_width,
		       size_t original_height, size_t channels, SampleT* output,
		       size_t new_width, size_t new_height, int support,
		       KernelFn kernel, bool parallel = true){
    if( original_width == 0 || original_height == 0 || channels == 0 ||
	new_width == 0 || new_height == 0 ) return;

    // No resampling needed.
    if( original_width == new_width && original_height == new_height ){
      const size_t n = original_width * original_height * channels;
      std::memcpy(output, input, n * sizeof(SampleT));
      return;
    }

    trim_weight_cache();

    // An unchanged axis needs no filtering at all. Skipping it also avoids an
    // intermediate buffer for the common "only one dimension changed" request.
    // Querying cached_weights only for the changing axis avoids building and
    // caching an unused identity weight table.
    if( new_width == original_width ){
      const WeightTable& yweights =
        cached_weights(new_height, original_height, support, kernel);
      resample_vertical_only<SampleT>(input, original_width, channels, output,
				      new_height, yweights, parallel);
      return;
    }
    if( new_height == original_height ){
      const WeightTable& xweights =
        cached_weights(new_width, original_width, support, kernel);
      const HorizontalVariant variant =
        choose_horizontal_variant(original_width, new_width, channels);
      resample_horizontal_only<SampleT>(input, original_width, original_height,
					channels, output, new_width, xweights,
					variant, parallel);
      return;
    }

    const WeightTable& xweights = cached_weights(new_width, original_width, support, kernel);
    const WeightTable& yweights = cached_weights(new_height, original_height, support, kernel);

    // Both axes change: stream horizontally filtered rows through a per-thread
    // ring instead of materializing the whole intermediate image.
    const HorizontalVariant variant =
      choose_horizontal_variant(original_width, new_width, channels);
    resample_ring<SampleT>(input, original_width, original_height, channels,
			   output, new_width, new_height, xweights, yweights,
			   variant, max_taps(yweights), parallel);
  }

  namespace {

    // Keys cubic convolution kernel with a = -0.5 (Catmull-Rom), the same filter
    // used by Pillow's "bicubic" resampler. Support radius is 2.
    inline float bicubic_kernel(float x, int){
      x = std::abs(x);
      if( x < 1.0f )
	return ((1.5f * x - 2.5f) * x * x + 1.0f);
      if( x < 2.0f )
	return (((-0.5f * x + 2.5f) * x - 4.0f) * x + 2.0f);
      return 0.0f;
    }


    constexpr float PI = 3.14159265358979323846f;

    // Normalized Lanczos kernel: sinc(x) * sinc(x / a) for |x| < a.
    // The support parameter is the kernel order (2 or 3).
    inline float lanczos_kernel(float x, int support){
      x = std::abs(x);
      if( x < 1e-6f )
	return 1.0f;
      if( x >= static_cast<float>(support) )
	return 0.0f;
      const float px = PI * x;
      return static_cast<float>(support) * std::sin(px) *
	std::sin(px / static_cast<float>(support)) / (px * px);
    }


    template <typename SampleT>
    void replace_tile_data(RawTile& in, SampleT* output, unsigned int width,
			   unsigned int height, size_t channels){
      // Free the old buffer through RawTile's type-aware deallocation so that
      // the delete matches the original allocation regardless of sample type.
      if( in.memoryManaged ){
	in.deallocate(in.data);
      }

      in.data = output;
      in.width = width;
      in.height = height;
      in.dataLength =
	(uint32_t)((uint64_t)width * height * channels * sizeof(SampleT));
      in.capacity = in.dataLength;
      in.memoryManaged = 1;
    }


    // Nearest-neighbour resampling using pixel-center mapping. The copy is
    // templated so that 8- and 16-bit samples are read and written at the correct
    // width. Unlike the old in-place path, the output is always a separate buffer,
    // which keeps the parallel loop safe for minification as well as magnification.
    template <typename SampleT>
    void resize_nearest_typed(RawTile& in, unsigned int resampled_width,
			      unsigned int resampled_height){
      const size_t channels = in.channels;
      const size_t width = in.width;
      const size_t height = in.height;
      const SampleT* input = (const SampleT*)in.data;

      // Nothing to do if the dimensions are unchanged.
      if( width == resampled_width && height == resampled_height ) return;

      size_t new_size = 0;
      if( !checked_output_samples(resampled_width, resampled_height, channels,
				  sizeof(SampleT), new_size) )
	return;
      std::unique_ptr<SampleT[]> output(new SampleT[new_size]);
      SampleT* out_ptr = output.get();

      // Precompute the input column offsets once for all rows using pixel-center
      // mapping.
      std::vector<size_t> xmap(resampled_width);
      for( unsigned int i = 0; i < resampled_width; ++i ){
	const uint64_t nearest = nearest_center_index(i, width, resampled_width);
	xmap[i] = static_cast<size_t>(nearest) * channels;
      }

#if defined(_OPENMP)
#pragma omp parallel for if( static_cast<long long>(resampled_width) * static_cast<long long>(resampled_height) > PARALLEL_THRESHOLD ) schedule(static)
#endif
      for( long long j = 0; j < static_cast<long long>(resampled_height); ++j ){
	const uint64_t nearest_y = nearest_center_index(static_cast<uint64_t>(j),
							height, resampled_height);
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT inrow =
	  input + static_cast<size_t>(nearest_y) * width * channels;
	SampleT* TRANSFORM_RESAMPLE_RESTRICT outrow =
	  out_ptr + static_cast<size_t>(j) * resampled_width * channels;

	for( unsigned int i = 0; i < resampled_width; ++i ){
	  const SampleT* TRANSFORM_RESAMPLE_RESTRICT src = inrow + xmap[i];
	  SampleT* TRANSFORM_RESAMPLE_RESTRICT dst = outrow + (size_t)i * channels;
	  for( size_t k = 0; k < channels; ++k ){
	    dst[k] = src[k];
	  }
	}
      }

      replace_tile_data<SampleT>(in, output.release(), resampled_width,
				 resampled_height, channels);
    }


    template <typename SampleT>
    void resize_with_kernel_typed(RawTile& in, unsigned int resampled_width,
				  unsigned int resampled_height, int support,
				  KernelFn kernel){
      const size_t channels = in.channels;
      size_t new_size = 0;
      if( !checked_output_samples(resampled_width, resampled_height, channels,
				  sizeof(SampleT), new_size) ) return;

      std::unique_ptr<SampleT[]> output(new SampleT[new_size]);

      resample<SampleT>((const SampleT*)in.data, in.width, in.height, channels,
			output.get(), resampled_width, resampled_height, support,
			kernel, true);

      replace_tile_data<SampleT>(in, output.release(), resampled_width,
				 resampled_height, channels);
    }


    void resize_with_kernel(RawTile& in, unsigned int resampled_width,
			    unsigned int resampled_height, int support,
			    KernelFn kernel){

      if( !in.data ) return;
      if( in.channels == 0 || in.width == 0 || in.height == 0 ) return;
      if( resampled_width == 0 || resampled_height == 0 ) return;
      if( in.width == resampled_width && in.height == resampled_height ) return;
      if( !weight_table_sizes_valid(resampled_width, in.width, support) ||
	  !weight_table_sizes_valid(resampled_height, in.height, support) ){
	return;
      }

      if( in.bpc == 8 ){
	resize_with_kernel_typed<unsigned char>(in, resampled_width, resampled_height, support, kernel);
      }
      else if( in.bpc == 16 ){
	resize_with_kernel_typed<unsigned short>(in, resampled_width, resampled_height, support, kernel);
      }
      else if( in.bpc == 32 && in.sampleType == SampleType::FIXEDPOINT ){
	resize_with_kernel_typed<unsigned int>(in, resampled_width, resampled_height, support, kernel);
      }
      else if( in.bpc == 32 && in.sampleType == SampleType::FLOATINGPOINT ){
	resize_with_kernel_typed<float>(in, resampled_width, resampled_height, support, kernel);
      }
    }

  } // namespace


  void resize_bicubic( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){
    resize_with_kernel( in, resampled_width, resampled_height, 2, bicubic_kernel );
  }


  void resize_lanczos( RawTile& in, unsigned int resampled_width, unsigned int resampled_height, int support ){
    resize_with_kernel( in, resampled_width, resampled_height, support, lanczos_kernel );
  }


  namespace {

    struct BilinearXMap {
      uint64_t offset0;
      uint64_t offset1;
      float weight0;
      float weight1;
    };


    // Horizontal interpolation of one input row into a float row. The vertical
    // pass below reuses these rows across output rows, so each input row is
    // filtered once per thread instead of once per output row.
    template <typename SampleT, int C>
    inline void
    bilinear_horizontal_row(const SampleT* TRANSFORM_RESAMPLE_RESTRICT row,
			    float* TRANSFORM_RESAMPLE_RESTRICT dst,
			    size_t new_width,
			    const std::vector<BilinearXMap>& xmap){
      for( size_t x = 0; x < new_width; ++x ){
	const BilinearXMap& m = xmap[x];
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT p = row + m.offset0;
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT q = row + m.offset1;
	const float a = m.weight0;
	const float b = m.weight1;
	float* TRANSFORM_RESAMPLE_RESTRICT o = dst + x * C;
#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( int k = 0; k < C; ++k ){
	  o[k] = static_cast<float>(p[k]) * a + static_cast<float>(q[k]) * b;
	}
      }
    }


    template <typename SampleT>
    inline void
    bilinear_horizontal_row_generic(const SampleT* TRANSFORM_RESAMPLE_RESTRICT row,
				    float* TRANSFORM_RESAMPLE_RESTRICT dst,
				    size_t new_width, size_t channels,
				    const std::vector<BilinearXMap>& xmap){
      for( size_t x = 0; x < new_width; ++x ){
	const BilinearXMap& m = xmap[x];
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT p = row + m.offset0;
	const SampleT* TRANSFORM_RESAMPLE_RESTRICT q = row + m.offset1;
	const float a = m.weight0;
	const float b = m.weight1;
	float* TRANSFORM_RESAMPLE_RESTRICT o = dst + x * channels;
	for( size_t k = 0; k < channels; ++k )
	  o[k] = static_cast<float>(p[k]) * a + static_cast<float>(q[k]) * b;
      }
    }

  } // namespace


  template <typename SampleT>
  void resize_bilinear_typed(RawTile& in, unsigned int resampled_width,
			     unsigned int resampled_height){
    const size_t channels = in.channels;
    const size_t width = in.width;
    const size_t height = in.height;
    const SampleT* input = (const SampleT*)in.data;

    // Nothing to do if the dimensions are unchanged.
    if( width == resampled_width && height == resampled_height ) return;

    size_t new_size = 0;
    if( !checked_output_samples(resampled_width, resampled_height, channels,
				sizeof(SampleT), new_size) ){
      return;
    }
    std::unique_ptr<SampleT[]> output(new SampleT[new_size]);
    SampleT* out_ptr = output.get();

    const double xscale = static_cast<double>(width) / resampled_width;
    const double yscale = static_cast<double>(height) / resampled_height;

    std::vector<BilinearXMap> xmap(resampled_width);
    for( unsigned int i = 0; i < resampled_width; i++ ){
      const double iscale = (static_cast<double>(i) + 0.5) * xscale - 0.5;
      const double floor_i = std::floor(iscale);
      const size_t ii0 = floor_i < 0.0 ? 0
	: (floor_i >= static_cast<double>(width - 1)
	   ? width - 1
	   : static_cast<size_t>(floor_i));
      const size_t ii1 = floor_i < 0.0
	? 0
	: (floor_i >= static_cast<double>(width - 1)
	   ? width - 1
	   : static_cast<size_t>(floor_i) + 1);
      const float frac = static_cast<float>(iscale - floor_i);
      BilinearXMap& m = xmap[i];
      m.offset0 = static_cast<uint64_t>(ii0) * channels;
      m.offset1 = static_cast<uint64_t>(ii1) * channels;
      m.weight0 = 1.0f - frac;
      m.weight1 = frac;
    }

    const size_t row_len = (size_t)resampled_width * channels;

    auto horizontal = [&](size_t r, float* dst){
      const SampleT* row = input + r * width * channels;
      switch( channels ){
      case 1:
	bilinear_horizontal_row<SampleT, 1>(row, dst, resampled_width, xmap);
	break;
      case 2:
	bilinear_horizontal_row<SampleT, 2>(row, dst, resampled_width, xmap);
	break;
      case 3:
	bilinear_horizontal_row<SampleT, 3>(row, dst, resampled_width, xmap);
	break;
      case 4:
	bilinear_horizontal_row<SampleT, 4>(row, dst, resampled_width, xmap);
	break;
      default:
	bilinear_horizontal_row_generic<SampleT>(row, dst, resampled_width,
						 channels, xmap);
	break;
      }
    };

    std::atomic<bool> alloc_failed(false);

#if defined(_OPENMP)
#pragma omp parallel if( static_cast<long long>(resampled_width) * static_cast<long long>(resampled_height) > PARALLEL_THRESHOLD )
#endif
    {
      // Two horizontally filtered rows are enough for bilinear: the current
      // input row pair. They are reused by consecutive output rows when
      // magnifying, so each input row is filtered at most once per thread.
      std::unique_ptr<float[]> ring(new (std::nothrow) float[2 * row_len]);
      if( !ring ){
	alloc_failed.store(true, std::memory_order_relaxed);
      }

      size_t have[2] = {SIZE_MAX, SIZE_MAX};

      auto get_row = [&](size_t r) -> float* {
	const size_t slot = r & 1;
	if( have[slot] != r ){
	  horizontal(r, ring.get() + slot * row_len);
	  have[slot] = r;
	}
	return ring.get() + slot * row_len;
      };

#if defined(_OPENMP)
#pragma omp for schedule(static)
#endif
      for( long long j = 0; j < static_cast<long long>(resampled_height); j++ ){
	if( alloc_failed.load(std::memory_order_relaxed) ) continue;

	const double jscale = (static_cast<double>(j) + 0.5) * yscale - 0.5;
	const double floor_j = std::floor(jscale);
	const size_t jj0 = floor_j < 0.0 ? 0
	  : (floor_j >= static_cast<double>(height - 1)
	     ? height - 1
	     : static_cast<size_t>(floor_j));
	const size_t jj1 = floor_j < 0.0 ? 0
	  : (floor_j >= static_cast<double>(height - 1)
	     ? height - 1
	     : static_cast<size_t>(floor_j) + 1);
	const float frac = static_cast<float>(jscale - floor_j);
	const float c = 1.0f - frac;
	const float d = frac;

	const float* TRANSFORM_RESAMPLE_RESTRICT row0 = get_row(jj0);
	const float* TRANSFORM_RESAMPLE_RESTRICT row1 = get_row(jj1);
	SampleT* TRANSFORM_RESAMPLE_RESTRICT out = out_ptr + (size_t)j * row_len;

#if defined(_OPENMP) && _OPENMP >= 201307
#pragma omp simd
#endif
	for( size_t n = 0; n < row_len; ++n )
	  out[n] = clamp_truncate<SampleT>(row0[n] * c + row1[n] * d);
      }
    }

    if( alloc_failed.load(std::memory_order_relaxed) ) throw std::bad_alloc();

    replace_tile_data<SampleT>(in, output.release(), resampled_width,
			       resampled_height, channels);
  }


  void resize_bilinear(RawTile& in, unsigned int resampled_width,
		       unsigned int resampled_height){
    if( !in.data ) return;
    if( in.channels == 0 || in.width == 0 || in.height == 0 ) return;
    if( resampled_width == 0 || resampled_height == 0 ) return;
    if( in.width == resampled_width && in.height == resampled_height ) return;

    if( in.bpc == 8 ){
      resize_bilinear_typed<unsigned char>( in, resampled_width, resampled_height );
    }
    else if( in.bpc == 16 ){
      resize_bilinear_typed<unsigned short>( in, resampled_width, resampled_height );
    }
    else if( in.bpc == 32 && in.sampleType == SampleType::FIXEDPOINT ){
      resize_bilinear_typed<unsigned int>( in, resampled_width, resampled_height );
    }
    else if( in.bpc == 32 && in.sampleType == SampleType::FLOATINGPOINT ){
      resize_bilinear_typed<float>( in, resampled_width, resampled_height );
    }
  }


  void resize_nearest(RawTile& in, unsigned int resampled_width,
		      unsigned int resampled_height){
    if( !in.data ) return;
    if( in.channels == 0 || in.width == 0 || in.height == 0 ) return;
    if( resampled_width == 0 || resampled_height == 0 ) return;
    if( in.width == resampled_width && in.height == resampled_height ) return;

    if( in.bpc == 8 ){
      resize_nearest_typed<unsigned char>( in, resampled_width, resampled_height );
    }
    else if( in.bpc == 16 ){
      resize_nearest_typed<unsigned short>( in, resampled_width, resampled_height );
    }
    else if( in.bpc == 32 && in.sampleType == SampleType::FIXEDPOINT ){
      resize_nearest_typed<unsigned int>( in, resampled_width, resampled_height );
    }
    else if( in.bpc == 32 && in.sampleType == SampleType::FLOATINGPOINT ){
      resize_nearest_typed<float>( in, resampled_width, resampled_height );
    }
  }

} // namespace resample
