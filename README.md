# irdeto-ffmpeg
This README is for the feature by Irdeto patch.

## Patch list
- Patches / additions to the MXF demuxer enabling support of MXF content.
- Patches / additions in the wrapper of the x264 encoder (libx264.c).
- Patches / additions to the built in h264 decoder to export necessary metadata from the decoder.
- Patches / additions in the wrapper of the x265 encoder (libx265.c).
- Patches / additions to the built in h265 decoder to export necessary metadata from the decoder.
- Unified interface irdeto-xps-export to facilitate the export of metadata from the decoder to a separate encoder.
- Patches in MP4/MOV demux to support CENC ISO23001-7 cbcs decryption.
- Patches in MP4/MOV remux to support CENC ISO23001-7 cenc v3 encryption and CENC ISO23001-7 cbcs v3 encryption.
- Few helper video filters

## Common code
- add unit test to the patch, especially the helper function which does not have much dependency of the FFmpeg lib and is fully controlled by the developer regarding the processing logic. Also, unit test should not have memory leak.
- add Irdeto specific flag to switch on/off the patch and the original behavior should not be broken if flag is off.
- avoid tight couple with FFmpeg original code but add patch as different API/functions and files to separate original and patch features.
- write document about the patch API, command, the sandbox and limitation/known issue.


## Build
To build this project, please run:

    cd irdeto-ffmpeg
    mkdir build
    cd build/
    cmake3 ..
    make

## unit test
To run unit test:

    make test


## CENC ISO23001-7 cenc and cbcs scheme support
official ffmpeg only supports CENC ISO23001-7 cenc scheme decryption and v1 encryption. This patch extends the support regarding
cenc and cbcs v3 scheme

### Reference
ISO/IEC 23001-7 Information technology — MPEG systems technologies — Part 7: Common encryption in ISO base media file format files

### CENC ISO23001-7 cbcs decryption
Play encrypted video:

    irdeto-ffplay  -irdeto_cbcs 1 -i {input file encrypted in cbcs v1/v2/v3 scheme} {ffmpeg options} -decryption_key {16 byte key in hex}

For example:

    irdeto-ffplay  -irdeto_cbcs 1 -i tos_cbcs.mp4 -decryption_key 533a583a843436a536fbe2a5821c4b6c


Decrypt and remux encrypted video:

    irdeto-ffmpeg  -irdeto_cbcs 1 -i {input file encrypted in cbcs v1/v2/v3 scheme} {ffmpeg options} \
                   -decryption_key {16 byte key in hex} {output file}

For example:

    irdeto-ffmpeg  -irdeto_cbcs 1 -i tos_cbcs.mp4 -c:v copy -c:a copy -decryption_key 533a583a843436a536fbe2a5821c4b6c \
    tos.ts

### CENC ISO23001-7 cenc v3 encryption
Encrypt and remux clear video

    irdeto-ffmpeg -i {input clear video file} {ffmpeg options} -encryption_scheme irdeto-cenc-aes-ctr-v3 \
                  -encryption_key {16 byte key in hex} -encryption_kid {16 byte key id in hex} \
                  {output file}

For example:

    irdeto-ffmpeg -i tos.ts  -c:v copy -c:a copy -encryption_scheme irdeto-cenc-aes-ctr-v3 \
                  -encryption_key 533a583a843436a536fbe2a5821c4b6c -encryption_kid a7e61c373e219033c21091fa607bf3b8 \
                  tos_cenc_v3.mp4

    irdeto-ffmpeg -i tos.mp4  -c:v copy -c:a copy -encryption_scheme irdeto-cenc-aes-ctr-v3 \
                -encryption_key 533a583a843436a536fbe2a5821c4b6c -encryption_kid a7e61c373e219033c21091fa607bf3b8 \
                tos_cenc_v3.mp4

### CENC ISO23001-7 cbcs v3 encryption
Encrypt and remux clear video

    irdeto-ffmpeg -i {input clear video file} {ffmpeg options} -encryption_scheme irdeto-cbcs-aes-cbc \
                  -encryption_key {16 byte key in hex} -encryption_kid {16 byte key id in hex} \
                  {output file}

For example:

    irdeto-ffmpeg -i tos.ts  -c:v copy -c:a copy -encryption_scheme irdeto-cbcs-aes-cbc \
                  -encryption_key 533a583a843436a536fbe2a5821c4b6c -encryption_kid a7e61c373e219033c21091fa607bf3b8 \
                  tos_cbcs.mp4

    irdeto-ffmpeg -i tos.mp4  -c:v copy -c:a copy -encryption_scheme irdeto-cbcs-aes-cbc \
                -encryption_key 533a583a843436a536fbe2a5821c4b6c -encryption_kid a7e61c373e219033c21091fa607bf3b8 \
                tos_cbcs.mp4

### limitation
    - only support CENC ISO23001-7 AVC codec video NAL subsample encryption, the other codec is full encryption
    - derives all the limition of official ffmpeg regarding demux, remux, etc.
    - based on specs, decryption input and encryption output must be mp4/mov related container format
