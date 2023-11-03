#ifndef LOG_VERSION
#define LOG_VERSION 0x20231103

#include <stddef.h>
#include <stdarg.h>

#if defined(LOG_USE_TIME)
#include <time.h>
#endif /* LOG_USE_TIME */
typedef struct log_t
{
    void *data;
#if defined(LOG_USE_TIME)
    struct tm const *time;
#if defined(LOG_USE_USEC)
    unsigned long usec;
#endif /* LOG_USE_MSEC */
#if defined(LOG_USE_MSEC)
    unsigned long msec;
#endif /* LOG_USE_USEC */
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_THREAD)
    unsigned long tid;
#endif /* LOG_USE_THREAD */
    char const *file;
    unsigned int line;
    int level;
} log_t;

#define LOG_ISOK(isok, level, lvl) int isok(int level, int lvl)
#define LOG_LOCK(lock, data, flag) void lock(void *data, int flag)
#define LOG_IMPL(impl, ctx, fmt, ap) void impl(log_t const *ctx, char const *fmt, va_list ap)
#if defined(__GNUC__) || defined(__clang__)
#define LOG_IMPL_DEF(impl, ctx, fmt, ap) LOG_IMPL(impl, ctx, fmt, ap) __attribute__((__format__(__printf__, 2, 0)))
#else /* !__GNUC__ */
#define LOG_IMPL_DEF(impl, ctx, fmt, ap) LOG_IMPL(impl, ctx, fmt, ap)
#endif /* __GNUC__ */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

LOG_ISOK(log_islt, level, lvl);
LOG_ISOK(log_isgt, level, lvl);
LOG_ISOK(log_isle, level, lvl);
LOG_ISOK(log_isge, level, lvl);
LOG_ISOK(log_iseq, level, lvl);
LOG_ISOK(log_isne, level, lvl);

int log_join(LOG_ISOK((*isok), , ), int level, LOG_IMPL((*impl), , , ), void *data);
void log_impl(char const *file, unsigned int line, int level, char const *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 4, 5)))
#endif /* __GNUC__ */
    ;
#define log_log(level, ...) log_impl(__FILE__, __LINE__, level, __VA_ARGS__)

#if defined(LOG_USE_LOCK)
void log_lock(LOG_LOCK((*lock), , ), void *data);
#endif /* LOG_USE_LOCK */

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* log.h */
