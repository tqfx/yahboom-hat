#ifndef POOL_STR
#define POOL_STR

#include <stddef.h>
#include <stdarg.h>

// clang-format off
#define POOL_STR_INIT {NULL, 0, 0}
// clang-format on

struct pool_str
{
    char *p;
    size_t m;
    size_t n;
};

static inline char *pool_str_bufp(struct pool_str const *ctx)
{
    return ctx->p + ctx->n;
}

static inline size_t pool_str_bufn(struct pool_str const *ctx)
{
    return ctx->m - ctx->n;
}

static inline char *pool_str_done(struct pool_str *ctx, size_t n)
{
    char *p = ctx->p + ctx->n;
    ctx->n += n + 1;
    p[n] = 0;
    return p;
}

static inline int pool_str_have(struct pool_str *ctx, char const *p)
{
    return p >= ctx->p && p < ctx->p + ctx->n;
}

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

void pool_str_init(struct pool_str *ctx, size_t m);
void pool_str_grow(struct pool_str *ctx, size_t n);
void pool_str_exit(struct pool_str *ctx);

char *pool_str_puts(struct pool_str *ctx, char const *str);
char *pool_str_putn(struct pool_str *ctx, void const *ptr, size_t num);
char *pool_str_putv(struct pool_str *ctx, char const *fmt, va_list va)
#if defined(__GNUC__)
    __attribute__((__format__(__printf__, 2, 0)))
#endif /* __GNUC__ */
    ;
char *pool_str_putf(struct pool_str *ctx, char const *fmt, ...)
#if defined(__GNUC__)
    __attribute__((__format__(__printf__, 2, 3)))
#endif /* __GNUC__ */
    ;
char *pool_str_undo(struct pool_str *ctx);

void pool_str_drop(struct pool_str *ctx, char const *p);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* pool_str.h */
