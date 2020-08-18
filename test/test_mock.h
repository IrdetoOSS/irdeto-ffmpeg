#ifndef TEST_MOCK_H
#define TEST_MOCK_H
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

void av_log(void* avcl, int level, const char *fmt, ...);

void* av_mallocz(size_t x);
void av_free(void* x);

int av_reallocp( void *ptr, size_t new_size );
#endif /*!TEST_MOCK_H*/
