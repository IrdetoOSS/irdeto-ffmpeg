#include "test_mock.h"
#include "libavformat/avio.h"

void *av_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size + !size);
}

void av_log(void* avcl, int level, const char *fmt, ...)
{}

void* av_mallocz(size_t x)
{
    void* res = malloc(x);
    memset(res, 0, x);
    return res;
}


int av_reallocp(void *ptr, size_t size)
{
    void *val;

    if (!size) {
        av_freep(ptr);
        return 0;
    }

    memcpy(&val, ptr, sizeof(val));
    val = av_realloc(val, size);

    if (!val) {
        av_freep(ptr);
        return -1;
    }

    memcpy(ptr, &val, sizeof(val));
    return 0;
}

void av_free(void* x)
{
    free(x);
}

void av_freep(void* x)
{
    void *val;

    memcpy(&val, x, sizeof(val));
    //memcpy(x, &(void *){ NULL }, sizeof(val));
    av_free(val);
}

int avio_open_dyn_buf(AVIOContext **s)
{
    return 0;
}


void avio_w8(AVIOContext *s, int b)
{
}

void avio_write(AVIOContext *s, const unsigned char *buf, int size)
{
    memcpy(s->buffer, buf, size);
    s->buffer += size;
}

void avio_wl64(AVIOContext *s, uint64_t val)
{
}

void avio_wb64(AVIOContext *s, uint64_t val)
{
}

void avio_wl32(AVIOContext *s, unsigned int val)
{
}

void avio_wb32(AVIOContext *s, unsigned int val)
{
    uint32_t b0,b1,b2,b3;
    uint32_t res;

    b0 = (val & 0x000000ff) << 24u;
    b1 = (val & 0x0000ff00) << 8u;
    b2 = (val & 0x00ff0000) >> 8u;
    b3 = (val & 0xff000000) >> 24u;

    res = b0 | b1 | b2 | b3;
    avio_write(s, &res, 4);
}

void avio_wl24(AVIOContext *s, unsigned int val)
{
}

void avio_wb24(AVIOContext *s, unsigned int val)
{
}

void avio_wl16(AVIOContext *s, unsigned int val)
{
}

void avio_wb16(AVIOContext *s, unsigned int val)
{
}
int avio_close_dyn_buf(AVIOContext *s, uint8_t **pbuffer)
{return 0;}

uint32_t av_get_random_seed()
{return 0;}
int64_t avio_seek(AVIOContext *s, int64_t offset, int whence)
{return 0;}
