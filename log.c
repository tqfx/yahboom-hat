#include "log.h"

#if !defined LOG_NODE_MAX || (LOG_NODE_MAX + 0 < 1)
#undef LOG_NODE_MAX
#define LOG_NODE_MAX 1
#endif /* LOG_NODE_MAX */

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
    struct log_node node[LOG_NODE_MAX];
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

LOG_ISOK(log_islt, level, lvl) { return level < lvl; }
LOG_ISOK(log_isgt, level, lvl) { return level > lvl; }
LOG_ISOK(log_isle, level, lvl) { return level <= lvl; }
LOG_ISOK(log_isge, level, lvl) { return level >= lvl; }
LOG_ISOK(log_iseq, level, lvl) { return level == lvl; }
LOG_ISOK(log_isne, level, lvl) { return level != lvl; }

int log_join(LOG_ISOK((*isok), , ), int level, LOG_IMPL((*impl), , , ), void *data)
{
    int ok = ~0;
    for (unsigned int i = 0; i < LOG_NODE_MAX; ++i)
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
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
struct TimeVal
{
    time_t tv_sec;
    time_t tv_usec;
};
static inline int GetTimeOfDay(struct TimeVal *tv, void *tz)
{
    FILETIME filetime;
    ULARGE_INTEGER x;
    time_t usec;
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    GetSystemTimePreciseAsFileTime(&filetime);
#else /* !_WIN32_WINNT */
    GetSystemTimeAsFileTime(&filetime);
#endif /* _WIN32_WINNT */
    x.LowPart = filetime.dwLowDateTime;
    x.HighPart = filetime.dwHighDateTime;
#define EPOCH_FILE_TIME 116444736000000000ULL
    usec = (x.QuadPart - EPOCH_FILE_TIME) / 10;
    tv->tv_sec = usec / 1000000;
    tv->tv_usec = usec % 1000000;
    (void)(tz);
    return 0;
}
#define gettimeofday GetTimeOfDay
#define timeval TimeVal
#endif /* LOG_USE_USEC */
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
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
    struct timeval time;
#else /* !LOG_USE_USEC */
    time_t sec;
#endif /* LOG_USE_USEC */
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_LOCK)
    if (log_.lock)
    {
        log_.lock(log_.data, 1);
    }
#endif /* LOG_USE_LOCK */
#if defined(LOG_USE_TIME)
    ctx.time = &log_.tm;
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
    gettimeofday(&time, NULL);
    if (log_.time != time.tv_sec)
#else /* !LOG_USE_USEC */
    sec = time(NULL);
    if (log_.time != sec)
#endif /* LOG_USE_USEC */
    {
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
        log_.time = time.tv_sec;
#else /* !LOG_USE_USEC */
        log_.time = sec;
#endif /* LOG_USE_USEC */
#if defined(_WIN32)
        _tzset();
        localtime_s(&log_.tm, &log_.time);
#else /* !_WIN32 */
        tzset();
        localtime_r(&log_.time, &log_.tm);
#endif /* _WIN32 */
    }
#if defined(LOG_USE_USEC)
    ctx.usec = (unsigned long)(time.tv_usec);
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_MSEC)
    ctx.msec = (unsigned long)(time.tv_usec / 1000);
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
#else /* <macos(10.6), ios(3.2) */
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
    for (unsigned int i = 0; i < LOG_NODE_MAX && log_.node[i].flags; ++i)
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
