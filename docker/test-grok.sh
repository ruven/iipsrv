#!/usr/bin/env bash
set -euo pipefail

# /repo is mounted read-only; all generated build/test artifacts stay in /work.
mkdir -p /work/source /work/results
tar -C /repo --exclude=.git -cf - . | tar -C /work/source -xf -
find /work/source -type f -exec sed -i 's/\r$//' {} +
cd /work/source
trap 'tail -n 50 /work/results/*.log 2>/dev/null || true' ERR

autoreconf -fi > /work/results/autoreconf.log 2>&1
CXXFLAGS='-O2 -g -std=c++11 -Wall -Wextra -Wpedantic' \
  ./configure --disable-openjpeg > /work/results/configure.log 2>&1
grep -F 'true (Grok)' /work/results/configure.log
make -j3 > /work/results/build.log 2>&1
make check > /work/results/check.log 2>&1
[[ "$(pkg-config --modversion libgrokj2k)" == '20.4.19' ]]

python3 docker/grok-test-data.py generate /work/results
for name in sample color gray16; do
  extension=pnm
  if [[ "$name" == gray16 ]]; then extension=pgm; fi
  grk_compress -i "/work/results/$name.$extension" \
    -o "/work/results/$name.jp2" -t 256,256 -n 6 -L -X -H 2 \
    > "/work/results/encode-$name.log" 2>&1
done
grk_compress -i /work/results/virtual.pnm -o /work/results/virtual.jp2 \
  -n 1 -H 2 > /work/results/encode-virtual.log 2>&1

start_server() {
  local threads=$1
  local address="127.0.0.1:$((19000 + threads))"
  GROK_THREADS="$threads" VERBOSITY=5 LOGFILE="/work/results/server-$threads.log" \
    ./src/iipsrv.fcgi --bind "$address" \
    > "/work/results/server-$threads.stderr.log" 2>&1 &
  local server_pid=$!
  for attempt in {1..50}; do
    if grep -q 'Initialisation Complete' "/work/results/server-$threads.log" 2>/dev/null; then
      return
    fi
    kill -0 "$server_pid" || return 1
    sleep 0.1
  done
  return 1
}

request() {
  local name=$1 query=$2 threads=${3:-2}
  env REQUEST_METHOD=GET QUERY_STRING="$query" \
    SCRIPT_NAME=/iipsrv.fcgi SCRIPT_FILENAME=/iipsrv.fcgi \
    SERVER_SOFTWARE=grok-docker-test \
    SERVER_NAME=localhost SERVER_PORT=80 SERVER_PROTOCOL=HTTP/1.1 \
    GATEWAY_INTERFACE=CGI/1.1 REMOTE_ADDR=127.0.0.1 \
    timeout 30 cgi-fcgi -bind -connect "127.0.0.1:$((19000 + threads))" \
    > "/work/results/$name.response"
}

start_server 2
request cvt 'FIF=/work/results/sample.jp2&WID=333&QLT=100&CVT=JPEG'
request jtl 'FIF=/work/results/sample.jp2&JTL=2,5'
request virtual 'FIF=/work/results/virtual.jp2&JTL=0,0'
request color 'FIF=/work/results/color.jp2&WID=300&QLT=100&CVT=JPEG'
request edge 'FIF=/work/results/color.jp2&JTL=1,3'
request gray16 'FIF=/work/results/gray16.jp2&WID=200&CVT=JPEG'
request contrast 'FIF=/work/results/color.jp2&WID=300&CNT=ST:98&CVT=JPEG'
start_server 1
request threads1 'FIF=/work/results/color.jp2&WID=128&CVT=JPEG' 1
start_server 0
request threads0 'FIF=/work/results/sample.jp2&JTL=2,5' 0

pids=()
for number in {0..11}; do
  request "concurrent-$number" 'FIF=/work/results/color.jp2&WID=160&CVT=JPEG' &
  pids+=("$!")
done
for pid in "${pids[@]}"; do wait "$pid"; done
request missing 'FIF=/work/results/does-not-exist.jp2&CVT=JPEG'
request recovery 'FIF=/work/results/color.jp2&WID=100&CVT=JPEG'

python3 docker/grok-test-data.py validate /work/results | tee /work/results/report.json
for threads in 0 1 2; do
  grep -F 'Setting up JPEG2000 support via Grok 20.4.19' "/work/results/server-$threads.log"
done

# Verify both the explicit prefix and the original OpenJPEG backend.
make distclean > /work/results/distclean.log 2>&1
PKG_CONFIG_LIBDIR=/nonexistent ./configure --with-grok=/opt/grok \
  --disable-openjpeg > /work/results/prefix-configure.log 2>&1
grep -F 'true (Grok)' /work/results/prefix-configure.log
make -j3 > /work/results/prefix-build.log 2>&1
make distclean > /work/results/distclean-prefix.log 2>&1
./configure --disable-grok > /work/results/openjpeg-configure.log 2>&1
grep -F 'true (OpenJPEG)' /work/results/openjpeg-configure.log
make -j3 > /work/results/openjpeg-build.log 2>&1
make check > /work/results/openjpeg-check.log 2>&1
echo 'Grok 20.4.19 E2E, explicit-prefix build and OpenJPEG fallback: PASS'
