#ifndef STRPOOL_H
#define STRPOOL_H 0x20231016

#include <stddef.h>
#include <stdarg.h>

// clang-format off
#define STRPOOL_INIT {NULL, 0, 0}
// clang-format on

struct strpool
{
    char *p;
    size_t m;
    size_t n;
};

static inline char *strpool_addr(struct strpool const *ctx) { return ctx->p; }

static inline size_t strpool_size(struct strpool const *ctx) { return ctx->m; }

static inline size_t strpool_used(struct strpool const *ctx) { return ctx->n; }

static inline char *strpool_bufp(struct strpool const *ctx) { return ctx->p + ctx->n; }

static inline size_t strpool_bufn(struct strpool const *ctx) { return ctx->m - ctx->n; }

static inline int strpool_have(struct strpool const *ctx, char const *p)
{
    return p >= ctx->p && p < ctx->p + ctx->n;
}

static inline char *strpool_done(struct strpool *ctx, size_t n)
{
    char *p = ctx->p + ctx->n;
    ctx->n += n + 1;
    p[n] = 0;
    return p;
}

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

void strpool_init(struct strpool *ctx, size_t m);
void strpool_grow(struct strpool *ctx, size_t n);
void strpool_exit(struct strpool *ctx);

char *strpool_puts(struct strpool *ctx, char const *str);
char *strpool_putn(struct strpool *ctx, void const *ptr, size_t num);
char *strpool_putv(struct strpool *ctx, char const *fmt, va_list va)
#if defined(__GNUC__)
    __attribute__((__format__(__printf__, 2, 0)))
#endif /* __GNUC__ */
    ;
char *strpool_putf(struct strpool *ctx, char const *fmt, ...)
#if defined(__GNUC__)
    __attribute__((__format__(__printf__, 2, 3)))
#endif /* __GNUC__ */
    ;
char *strpool_undo(struct strpool *ctx);

void strpool_drop(struct strpool *ctx, char const *p);

char *strpool_head(struct strpool const *ctx);
char *strpool_next(struct strpool const *ctx, char *cur);
#define strpool_foreach(ctx, cur) \
    for (char *cur = strpool_head(ctx); cur; cur = strpool_next(ctx, cur))

char *strpool_tail(struct strpool const *ctx);
char *strpool_prev(struct strpool const *ctx, char *cur);
#define strpool_foreach_reverse(ctx, cur) \
    for (char *cur = strpool_tail(ctx); cur; cur = strpool_prev(ctx, cur))

char *strpool_find(struct strpool *ctx, char const *p);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* strpool.h */
