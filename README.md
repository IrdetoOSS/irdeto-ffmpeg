# irdeto-ffmpeg
Based on FFmpeg 4.1 (https://github.com/FFmpeg/FFmpeg/tree/n4.1)

## License and legal considerations
The Irdeto FFmpeg project is based on FFmpeg sources and follows FFmpeg License and Legal Considerations (https://www.ffmpeg.org/legal.html)

The building of the project depends on some third-party components licensed in their own way:
- x264 licensing: https://x264.org/licensing/
- x265 licensing: https://www.x265.org/x265-licensing-faq/
- AOM licensing: https://aomedia.org/about/legal/
- SRT licensing: https://github.com/Haivision/srt/blob/master/LICENSE

## Patches / changes
- Patches in the wrapper of the x264 encoder (libx264.c)
- Patches in the wrapper of the x265 encoder (libx265.c)
- Patches in the wrapper of the libaom AV1 encoder
- Patches in the wrapper of the libaom AV1 decoder
- Patches in the wrapper of the SRT library (libsrt.c)
- Patches in h264 decoder to export necessary metadata from the decoder
- Patches in h265 decoder to export necessary metadata from the decoder
- Patches in MP4/MOV demux and remux
- Patches in MXF demuxer
- Unified interface irdeto-xps-export to exchange metadata between decoder and encoder
- Few helper video filters

## Build requirements
To build this project, folowing tools and libraries are reqired:

### RedHat-family distributions

    git
    gcc
    gcc-c++
    nasm
    tcl
    cmake
    make
    patch
    pkgconfig
    check-devel
    openjpeg2-devel
    openssl-devel
    libxml2-devel

### Debian-family distributions

    git
    gcc
    g++
    nasm
    tcl
    cmake
    make
    patch
    pkg-config
    check
    libopenjp2-7-dev
    libssl-dev
    libxml2-dev

## Build
To build this project, please run:

<!---
    Submodules update command:
    git submodule update --init --recursive
--->

    git clone --recurse-submodules https://github.com/IrdetoOSS/irdeto-ffmpeg.git
    cd irdeto-ffmpeg
    mkdir build
    cd build
    cmake ..
    make
    make install

## Unit tests
To execute unit tests, please run:

    make test

## Video filters

### irdeto_owl_emb
To list all options of the video filter, please run:

    irdeto-ffmpeg --help filter=irdeto_owl_emb

Optional environment variables:

    IR_WM_LIB         - Watermarking plugin library
    IR_WM_PLUGIN_PATH - Watermarking plugin path
    IR_LICENSE        - License file
