/*
 * Cross-platform compatibility header
 * Provides compatibility between GCC/Clang and MSVC
 */

#ifndef COMPAT_H
#define COMPAT_H

#ifdef _MSC_VER
/* MSVC-specific definitions */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

/* MSVC doesn't support computed gotos */
#define COMPUTED_GOTO_SUPPORTED 0

/* String functions */
#define strcasecmp _stricmp
#define strtok_r strtok_s

/* Timer support using QueryPerformanceCounter */
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER freq;
} compat_timer;

static inline void compat_timer_init(compat_timer *t) {
    QueryPerformanceFrequency(&t->freq);
}

static inline void compat_timer_start(compat_timer *t) {
    QueryPerformanceCounter(&t->start);
}

static inline long compat_timer_elapsed_ms(compat_timer *t) {
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    return (long)((end.QuadPart - t->start.QuadPart) * 1000 / t->freq.QuadPart);
}

#else
/* GCC/Clang definitions */

#include <sys/time.h>
#include <strings.h>

/* GCC and Clang support computed gotos */
#define COMPUTED_GOTO_SUPPORTED 1

/* Timer support using gettimeofday */
typedef struct {
    struct timeval start;
} compat_timer;

static inline void compat_timer_init(compat_timer *t) {
    (void)t; /* No initialization needed */
}

static inline void compat_timer_start(compat_timer *t) {
    gettimeofday(&t->start, NULL);
}

static inline long compat_timer_elapsed_ms(compat_timer *t) {
    struct timeval end;
    gettimeofday(&end, NULL);
    return ((end.tv_sec * 1000000L + end.tv_usec) -
            (t->start.tv_sec * 1000000L + t->start.tv_usec)) / 1000;
}

#endif /* _MSC_VER */

#endif /* COMPAT_H */
