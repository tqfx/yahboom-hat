#if defined(_MSC_VER)
#if !defined _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS /* NOLINT */
#endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */
#include "log.h"

static struct
{
    struct tm const *tm;
    struct log_node *head;
    struct log_node *tail;
    void (*lock)(void *, int);
    void *data;
} log = {
    NULL,
    NULL,
    (struct log_node *)&log.head,
    NULL,
    NULL,
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

void log_init(struct log_node *node, int (*isok)(unsigned int, unsigned int), unsigned int level,
              void (*exec)(struct log_node const *, char const *, va_list ap), void *data)
{
    node->isok = isok;
    node->level = level;
    node->exec = exec;
    node->data = data;
}

void log_join(struct log_node *node)
{
    if (log.lock)
    {
        log.lock(log.data, 1);
    }
    if (!log.tm)
    {
        time_t t = time(NULL);
        log.tm = localtime(&t);
    }
    node->time = log.tm;
    node->next = NULL;
    log.tail->next = node;
    log.tail = node;
    if (log.lock)
    {
        log.lock(log.data, 0);
    }
}

void log_drop(struct log_node *node)
{
    struct log_node *cur;
    if (log.lock)
    {
        log.lock(log.data, 1);
    }
    for (cur = (struct log_node *)&log.head; cur->next; cur = cur->next)
    {
        if (cur->next == node)
        {
            cur->next = node->next;
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

void log_exec(unsigned int level, char const *file, unsigned int line, char const *fmt, ...)
{
    va_list ap;
    struct log_node *cur;
    if (log.lock)
    {
        log.lock(log.data, 1);
    }
    for (cur = log.head; cur; cur = cur->next)
    {
        unsigned int lvl = cur->level;
        if (cur->isok(level, lvl))
        {
            cur->level = level;
            cur->file = file;
            cur->line = line;
            va_start(ap, fmt);
            cur->exec(cur, fmt, ap);
            va_end(ap);
            cur->level = lvl;
        }
    }
    if (log.lock)
    {
        log.lock(log.data, 0);
    }
}
