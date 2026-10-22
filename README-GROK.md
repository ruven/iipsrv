# Grok 20.4.19 for iipsrv

This repository includes an alternative Grok-based JPEG 2000 backend for
iipsrv. The Docker test pins the official Grok release `v20.4.19` to commit
`31ae6fa25be3747370b47a65d311e355f2adb33f`.

The Visual Studio project files list `GrokImage.cc` and `GrokImage.h`, but the
implementation is excluded from default Windows builds because the projects
do not yet configure a Grok Windows SDK include path or import library.

## Building Grok

```sh
git clone --branch v20.4.19 --depth 1 https://github.com/GrokImageCompression/grok.git
cd grok
git submodule update --init --recursive

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/grok \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DGRK_BUILD_CORE_SWIG_BINDINGS=OFF
cmake --build build --parallel
sudo cmake --install build
```

## Building iipsrv

From the root of this iipsrv checkout:

```sh
autoreconf -fi
PKG_CONFIG_PATH=/opt/grok/lib/pkgconfig \
  ./configure --disable-openjpeg
make --parallel
```

The configuration summary should contain:

```text
JPEG2000    :  true (Grok)
```

If the `pkg-config` file is unavailable, specify the installation prefix
directly:

```sh
./configure --with-grok=/opt/grok --disable-openjpeg
```

At runtime, the dynamic linker must be able to find `libgrokj2k`. Use the
system `ldconfig` configuration, an RPATH in the image, or for example:

```sh
export LD_LIBRARY_PATH=/opt/grok/lib
```

`GROK_THREADS` controls the number of Grok worker threads. The integration
defaults to `2`; a value of `0` uses all available logical processors.

## Verified scenarios

- building and linking iipsrv against Grok 20.4.19;
- automatic detection through `libgrokj2k.pc`;
- manual detection through `--with-grok=/opt/grok`;
- CVT region decoding of a multi-tile JP2 with TLM/PLT;
- JTL tile decoding;
- a virtual pyramid level for a JP2 with only one native resolution.

## Reproducing the Docker regression tests

The test image builds Grok from the pinned release and includes its command-line
encoder. It also includes Pillow for full JPEG decoding and comparison of RGB
pixel values against the synthetic source. Grok itself requires a C++23 compiler;
iipsrv is compiled with C++11.

From the repository root:

```sh
docker build -f docker/Dockerfile.grok-test -t iipsrv-grok-test:20.4.19 .
docker run --rm -v "${PWD}:/repo:ro" iipsrv-grok-test:20.4.19 \
  bash -c "tr -d '\r' < /repo/docker/test-grok.sh | bash"
```

The script builds the current checkout in `/work/source`, runs `make check`,
creates grayscale, RGB, 16-bit and single-resolution JP2 fixtures, and exercises
real FastCGI CVT/JTL requests, partial edge tiles, `GROK_THREADS=0/1/2`, concurrent
clients, histogram-based contrast stretching and recovery after a missing-file
request. The server uses its native TCP `--bind` mode. The test then verifies a build
using `--with-grok=/opt/grok` without pkg-config discovery, and a separate
OpenJPEG build with `--disable-grok`. No build artifacts are written to the
read-only source checkout.
