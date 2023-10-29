#ifndef LOG_VERSION
#define LOG_VERSION 0x20231029

#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#define LOG_INIT(isok, level, exec, data)            \
    {                                                \
        NULL, isok, exec, NULL, level, 0, NULL, data \
    }

struct log_node
{
    struct log_node *next;
    int (*isok)(unsigned int, unsigned int);
    void (*exec)(struct log_node const *, char const *, va_list ap);
    struct tm const *time;
    unsigned int level;
    unsigned int line;
    char const *file;
    void *data;
};

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
void log_init(struct log_node *node, int (*isok)(unsigned int, unsigned int), unsigned int level,
              void (*exec)(struct log_node const *, char const *, va_list ap), void *data);
void log_join(struct log_node *node);
void log_drop(struct log_node *node);

void log_exec(unsigned int level, char const *file, unsigned int line, char const *fmt, ...)
#if defined(__GNUC__)
    __attribute__((__format__(__printf__, 4, 5)))
#endif /* __GNUC__ */
    ;

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* log.h */
