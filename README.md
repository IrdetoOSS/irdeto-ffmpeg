# irdeto-ffmpeg
Based on FFmpeg 4.1 (https://github.com/FFmpeg/FFmpeg/tree/n4.1)

## Patch list
- Patches in the wrapper of the x264 encoder (libx264.c)
- Patches in the wrapper of the x265 encoder (libx265.c)
- Patches in h264 decoder to export necessary metadata from the decoder
- Patches in h265 decoder to export necessary metadata from the decoder
- Patches in MP4/MOV demux and remux
- Patches in MXF demuxer
- Unified interface irdeto-xps-export to facilitate the export of metadata from the decoder to a separate encoder
- Few helper video filters

## Build requirements
To build this project, folowing tools and libraries are reqired:

    git
    gcc-c++
    nasm
    cmake
    make
    patch
    check-devel
    openjpeg2-devel
    openssl-devel
    libxml2-devel

## Build
To build this project, please run:
<!---
    Submodules update command:
    git submodule update --init --recursive
--->

    git clone --recurse-submodules https://github.com/IrdetoOSS/irdeto-ffmpeg.git
    cd irdeto-ffmpeg
    mkdir build
    cd build/
    cmake3 ..
    make install

## Unit test
To execute unit tests, please run:

    make test
