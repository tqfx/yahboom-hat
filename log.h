#ifndef LOG_VERSION
#define LOG_VERSION 0x20231031

#include <stddef.h>
#include <stdarg.h>

#if defined(LOG_USE_TIME)
#include <time.h>
#endif /* LOG_USE_TIME */

#if defined(__cplusplus)
#define LOG_INIT(ISOK, LEVEL, IMPL, DATA) \
    log_node(ISOK, LEVEL, IMPL, DATA)
#else /* !__cplusplus */
// clang-format off
#define LOG_INIT(ISOK, LEVEL, IMPL, DATA) \
    {.isok = ISOK, .impl = IMPL, .level = LEVEL, .data = DATA}
// clang-format on
#endif /* __cplusplus */

typedef struct log_node
{
#if defined(__cplusplus)
    log_node(int (*_isok)(unsigned int, unsigned int), unsigned int _level,
             void (*_impl)(log_node const *, char const *, va_list), void *_data)
        : isok(_isok)
        , impl(_impl)
        , level(_level)
        , data(_data)
    {
    }
#endif /* __cplusplus */
    struct log_node *next;
    int (*isok)(unsigned int level, unsigned int lvl);
    void (*impl)(struct log_node const *log, char const *fmt, va_list ap);
#if defined(LOG_USE_TIME)
    struct tm const *time;
#if defined(LOG_USE_USEC)
    unsigned long usec;
#endif /* LOG_USE_MSEC */
#if defined(LOG_USE_MSEC)
    unsigned long msec;
#endif /* LOG_USE_USEC */
#endif /* LOG_USE_TIME */
    unsigned int level;
    unsigned int line;
#if defined(LOG_USE_THREAD)
    unsigned long tid;
#endif /* LOG_USE_THREAD */
    char const *file;
    void *data;
} log_t;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

int log_islt(unsigned int level, unsigned int lvl);
int log_isgt(unsigned int level, unsigned int lvl);
int log_isle(unsigned int level, unsigned int lvl);
int log_isge(unsigned int level, unsigned int lvl);
int log_iseq(unsigned int level, unsigned int lvl);
int log_isne(unsigned int level, unsigned int lvl);
void log_lock(void (*lock)(void *, int), void *data);
void log_init(struct log_node *log, int (*isok)(unsigned int, unsigned int), unsigned int level,
              void (*impl)(struct log_node const *, char const *, va_list), void *data);
void log_join(struct log_node *log);
void log_drop(struct log_node *log);

void log_impl(unsigned int level, char const *file, unsigned int line, char const *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 4, 5)))
#endif
    ;
#define log_log(level, ...) log_impl(level, __FILE__, __LINE__, __VA_ARGS__)

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* log.h */
