# Grok 20.3.7 for iipsrv

This repository includes an alternative Grok-based JPEG 2000 backend for
iipsrv. The integration was verified against Grok commit
`c9c1223c1e7bd7be579ea60e8e083ee244a78f58` (`20.3.7`).

The Visual Studio project files list `GrokImage.cc` and `GrokImage.h`, but the
implementation is excluded from default Windows builds because the projects
do not yet configure a Grok Windows SDK include path or import library.

## Building Grok

```sh
git clone --recursive https://github.com/GrokImageCompression/grok.git
cd grok
git checkout c9c1223c1e7bd7be579ea60e8e083ee244a78f58
git submodule update --init --recursive

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/grok
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

- building and linking iipsrv against Grok 20.3.7;
- automatic detection through `libgrokj2k.pc`;
- manual detection through `--with-grok=/opt/grok`;
- CVT region decoding of a multi-tile JP2 with TLM/PLT;
- JTL tile decoding;
- a virtual pyramid level for a JP2 with only one native resolution.
