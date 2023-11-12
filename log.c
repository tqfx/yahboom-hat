#include "log.h"

struct log_node
{
    LOG_ISOK((*isok), , );
    LOG_IMPL((*impl), , , );
    void *data;
    int level;
    int flags;
};

static struct log_head
{
#if defined(LOG_USE_LOCK)
    LOG_LOCK((*lock), , );
    void *data;
#endif /* LOG_USE_LOCK */
#if defined(LOG_USE_TIME)
    time_t time;
    struct tm tm;
#endif /* LOG_USE_TIME */
#if !defined LOG_NODE || (LOG_NODE + 0 < 1)
#undef LOG_NODE
#define LOG_NODE 1
#endif /* LOG_NODE */
    struct log_node node[LOG_NODE];
} log_ = {
#if defined(LOG_USE_LOCK)
    NULL,
    NULL,
#endif /* LOG_USE_LOCK */
#if defined(LOG_USE_TIME)
    0,
    {0},
#endif /* LOG_USE_TIME */
    {{0}},
};

int log_join(LOG_ISOK((*isok), , ), int level, LOG_IMPL((*impl), , , ), void *data)
{
    int ok = ~0;
    for (unsigned int i = 0; i < LOG_NODE; ++i)
    {
        struct log_node *log = (struct log_node *)(log_.node + i);
        if (!log->flags)
        {
            log->isok = isok;
            log->impl = impl;
            log->data = data;
            log->level = level;
            log->flags = 1;
            ok = 0;
            break;
        }
    }
    return ok;
}

#if defined(_WIN32)
#include <Windows.h>
#if defined(LOG_USE_TIME)
#define timeval TimeVal
struct timeval
{
    time_t tv_sec;
    time_t tv_usec;
};
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
#define gettimeofday GetTimeOfDay
static inline int gettimeofday(struct timeval *tv, void *tz)
{
    FILETIME t;
    ULARGE_INTEGER x;
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    GetSystemTimePreciseAsFileTime(&t);
#else /* !_WIN32_WINNT */
    GetSystemTimeAsFileTime(&t);
#endif /* _WIN32_WINNT */
    x.LowPart = t.dwLowDateTime;
    x.HighPart = t.dwHighDateTime;
#define FILETIME_EPOCH 116444736000000000ULL
    tv->tv_usec = (x.QuadPart - FILETIME_EPOCH) / 10;
    tv->tv_sec = tv->tv_usec / 1000000;
    tv->tv_usec %= 1000000;
    (void)(tz);
    return 0;
}
#endif /* LOG_USE_USEC */
#endif /* LOG_USE_TIME */
#else /* !_WIN32 */
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
#include <sys/time.h>
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_THREAD)
#if defined(__FreeBSD__)
#include <sys/thr.h>
#elif defined(__NetBSD__)
#include <sys/lwp.h>
#else /* !__FreeBSD__ */
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>
#endif /* __FreeBSD__ */
#endif /* LOG_USE_THREAD */
#endif /* _WIN32 */

void log_impl(char const *file, unsigned int line, int level, char const *fmt, ...)
{
    log_t ctx;
#if defined(LOG_USE_TIME)
    struct timeval tv;
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_LOCK)
    if (log_.lock)
    {
        log_.lock(log_.data, 1);
    }
#endif /* LOG_USE_LOCK */
    if (log_.node->flags)
    {
#if defined(LOG_USE_TIME)
        ctx.time = &log_.tm;
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
        gettimeofday(&tv, NULL);
#else /* !LOG_USE_USEC */
        tv.tv_sec = time(NULL);
#endif /* LOG_USE_USEC */
        if (log_.time != tv.tv_sec)
        {
            log_.time = tv.tv_sec;
#if defined(_WIN32)
            _tzset();
            localtime_s(&log_.tm, &log_.time);
#else /* !_WIN32 */
            tzset();
            localtime_r(&log_.time, &log_.tm);
#endif /* _WIN32 */
        }
#if defined(LOG_USE_USEC)
        ctx.usec = (unsigned long)(tv.tv_usec);
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_MSEC)
        ctx.msec = (unsigned long)(tv.tv_usec / 1000);
#endif /* LOG_USE_MSEC */
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_THREAD)
#if defined(_WIN32)
        ctx.tid = GetCurrentThreadId();
#elif defined(__linux__)
        ctx.tid = (unsigned long)syscall(SYS_gettid);
#elif defined(__APPLE__)
#if 1 /* macos(10.6), ios(3.2) */
        {
            uint64_t tid64;
            pthread_threadid_np(NULL, &tid64);
            ctx.tid = (unsigned long)tid64;
        }
#else /* !macos(10.6), ios(3.2) */
        ctx.tid = (unsigned long)syscall(SYS_thread_selfid);
#endif /* macos(10.6), ios(3.2) */
#elif defined(__FreeBSD__)
        thr_self((long *)&ctx.tid);
#elif defined(__OpenBSD__)
        ctx.tid = (unsigned long)getthrid();
#elif defined(__NetBSD__)
        ctx.tid = (unsigned long)_lwp_self();
#else /* !_WIN32 */
        ctx.tid = pthread_self();
#endif /* _WIN32 */
#endif /* LOG_USE_THREAD */
        ctx.file = file;
        ctx.line = line;
        ctx.level = level;
    }
    for (unsigned int i = 0; i < LOG_NODE && log_.node[i].flags; ++i)
    {
        struct log_node *log = (log_.node + i);
        if (log->isok(level, log->level))
        {
            va_list ap;
            va_start(ap, fmt);
            ctx.data = log->data;
            log->impl(&ctx, fmt, ap);
            va_end(ap);
        }
    }
#if defined(LOG_USE_LOCK)
    if (log_.lock)
    {
        log_.lock(log_.data, 0);
    }
#endif /* LOG_USE_LOCK */
}

#if defined(LOG_USE_LOCK)
void log_lock(LOG_LOCK((*lock), , ), void *data)
{
    log_.lock = lock;
    log_.data = data;
}
#endif /* LOG_USE_LOCK */
