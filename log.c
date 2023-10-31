#include "log.h"

static struct log_root
{
    struct log_node *head;
    struct log_node *tail;
    void (*lock)(void *, int);
    void *data;
#if defined(LOG_USE_TIME)
    struct tm tm;
    time_t time;
#endif /* LOG_USE_TIME */
} log = {
    NULL,
    (struct log_node *)&log.head,
    NULL,
    NULL,
#if defined(LOG_USE_TIME)
    {0},
    0,
#endif /* LOG_USE_TIME */
};

int log_islt(unsigned int level, unsigned int lvl) { return level < lvl; }
int log_isgt(unsigned int level, unsigned int lvl) { return level > lvl; }
int log_isle(unsigned int level, unsigned int lvl) { return level <= lvl; }
int log_isge(unsigned int level, unsigned int lvl) { return level >= lvl; }
int log_iseq(unsigned int level, unsigned int lvl) { return level == lvl; }
int log_isne(unsigned int level, unsigned int lvl) { return level != lvl; }

void log_lock(void (*lock)(void *, int), void *data)
{
    log.lock = lock;
    log.data = data;
}

void log_init(struct log_node *_log, int (*isok)(unsigned int, unsigned int), unsigned int level,
              void (*impl)(struct log_node const *, char const *, va_list ap), void *data)
{
    _log->isok = isok;
    _log->level = level;
    _log->impl = impl;
    _log->data = data;
}

void log_join(struct log_node *_log)
{
    if (log.lock)
    {
        log.lock(log.data, 1);
    }
#if defined(LOG_USE_TIME)
    _log->time = &log.tm;
#endif /* LOG_USE_TIME */
    _log->next = NULL;
    log.tail->next = _log;
    log.tail = _log;
    if (log.lock)
    {
        log.lock(log.data, 0);
    }
}

void log_drop(struct log_node *_log)
{
    if (log.lock)
    {
        log.lock(log.data, 1);
    }
    for (struct log_node *cur = (struct log_node *)&log.head; cur->next; cur = cur->next)
    {
        if (cur->next == _log)
        {
            cur->next = _log->next;
            if (!cur->next)
            {
                log.tail = cur;
            }
            break;
        }
    }
    if (log.lock)
    {
        log.lock(log.data, 0);
    }
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

void log_impl(unsigned int level, char const *file, unsigned int line, char const *fmt, ...)
{
#if defined(LOG_USE_TIME)
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
    struct timeval time;
#else /* !LOG_USE_USEC */
    time_t sec;
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_USEC)
    unsigned long usec;
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_MSEC)
    unsigned long msec;
#endif /* LOG_USE_MSEC */
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_THREAD)
    unsigned long tid;
#endif /* LOG_USE_THREAD */
    if (log.lock)
    {
        log.lock(log.data, 1);
    }
#if defined(LOG_USE_TIME)
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
    gettimeofday(&time, NULL);
    if (log.time != time.tv_sec)
#else /* !LOG_USE_USEC */
    sec = time(NULL);
    if (log.time != sec)
#endif /* LOG_USE_USEC */
    {
#if defined(LOG_USE_USEC) || \
    defined(LOG_USE_MSEC)
        log.time = time.tv_sec;
#else /* !LOG_USE_USEC */
        log.time = sec;
#endif /* LOG_USE_USEC */
#if defined(_WIN32)
        _tzset();
        localtime_s(&log.tm, &log.time);
#else /* !_WIN32 */
        tzset();
        localtime_r(&log.time, &log.tm);
#endif /* _WIN32 */
    }
#if defined(LOG_USE_USEC)
    usec = (unsigned long)(time.tv_usec);
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_MSEC)
    msec = (unsigned long)(time.tv_usec / 1000);
#endif /* LOG_USE_MSEC */
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_THREAD)
#if defined(_WIN32)
    tid = GetCurrentThreadId();
#elif defined(__linux__)
    tid = (unsigned long)syscall(SYS_gettid);
#elif defined(__APPLE__)
#if 1 /* macos(10.6), ios(3.2) */
    {
        uint64_t tid64;
        pthread_threadid_np(NULL, &tid64);
        tid = (unsigned long)tid64;
    }
#else /* <macos(10.6), ios(3.2) */
    tid = (unsigned long)syscall(SYS_thread_selfid);
#endif /* macos(10.6), ios(3.2) */
#elif defined(__FreeBSD__)
    thr_self((unsigned long *)&tid);
#elif defined(__OpenBSD__)
    tid = (unsigned long)getthrid();
#elif defined(__NetBSD__)
    tid = (unsigned long)_lwp_self();
#else /* !_WIN32 */
    tid = pthread_self();
#endif /* _WIN32 */
#endif /* LOG_USE_THREAD */
    for (struct log_node *cur = log.head; cur; cur = cur->next)
    {
        unsigned int lvl = cur->level;
        if (cur->isok(level, lvl))
        {
            va_list ap;
            cur->level = level;
            cur->file = file;
            cur->line = line;
#if defined(LOG_USE_TIME)
#if defined(LOG_USE_USEC)
            cur->usec = usec;
#endif /* LOG_USE_USEC */
#if defined(LOG_USE_MSEC)
            cur->msec = msec;
#endif /* LOG_USE_MSEC */
#endif /* LOG_USE_TIME */
#if defined(LOG_USE_THREAD)
            cur->tid = tid;
#endif /* LOG_USE_THREAD */
            va_start(ap, fmt);
            cur->impl(cur, fmt, ap);
            va_end(ap);
            cur->level = lvl;
        }
    }
    if (log.lock)
    {
        log.lock(log.data, 0);
    }
}
