#include "pool_str.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void pool_str_init(struct pool_str *ctx, size_t m)
{
    if (m == 0)
    {
        m = BUFSIZ;
    }
    ctx->p = (char *)malloc(m);
    ctx->m = m;
    ctx->n = 0;
}

void pool_str_grow(struct pool_str *ctx, size_t n)
{
    do
    {
        ctx->m <<= 1;
    } while (ctx->n + n > ctx->m);
    ctx->p = (char *)realloc(ctx->p, ctx->m);
}

void pool_str_exit(struct pool_str *ctx)
{
    free(ctx->p);
    ctx->p = 0;
    ctx->m = 0;
    ctx->n = 0;
}

char *pool_str_puts(struct pool_str *ctx, char const *str)
{
    return pool_str_putn(ctx, str, str ? strlen(str) : 0);
}

char *pool_str_putn(struct pool_str *ctx, void const *ptr, size_t num)
{
    char *str_p;
    size_t str_n = num + 1;
    if (ctx->n + str_n > ctx->m)
    {
        pool_str_grow(ctx, str_n);
    }
    str_p = ctx->p + ctx->n;
    memcpy(str_p, ptr, num);
    ctx->n += str_n;
    str_p[num] = 0;
    return str_p;
}

char *pool_str_putv(struct pool_str *ctx, char const *fmt, va_list va)
{
    va_list ap;
    char *str_p;
    size_t str_n;
    size_t mem, num;
    va_copy(ap, va);
    str_p = ctx->p + ctx->n;
    mem = ctx->m - ctx->n;
    num = (size_t)vsnprintf(str_p, mem, fmt, ap);
    va_end(ap);
    str_n = num + 1;
    if (str_n > mem)
    {
        pool_str_grow(ctx, str_n);
        ctx->p = (char *)realloc(ctx->p, ctx->m);
        va_copy(ap, va);
        str_p = ctx->p + ctx->n;
        mem = ctx->m - ctx->n;
        num = (size_t)vsnprintf(str_p, mem, fmt, ap);
        va_end(ap);
    }
    ctx->n += str_n;
    return str_p;
}

char *pool_str_putf(struct pool_str *ctx, char const *fmt, ...)
{
    va_list va;
    char *str_p;
    va_start(va, fmt);
    str_p = pool_str_putv(ctx, fmt, va);
    va_end(va);
    return str_p;
}

char *pool_str_undo(struct pool_str *ctx)
{
    char *p = ctx->p + ctx->n;
    if (--p >= ctx->p && *p == 0)
    {
        while (--p >= ctx->p && *p)
        {
        }
    }
    ctx->n = (size_t)(++p - ctx->p);
    return p;
}

void pool_str_drop(struct pool_str *ctx, char const *p)
{
    char *end = ctx->p + ctx->n;
    if (p >= ctx->p && p < end)
    {
        char *str_p = ctx->p + (p - ctx->p);
        size_t str_n = strlen(str_p) + 1;
        char *start = str_p + str_n;
        memmove(str_p, start, (size_t)(end - start));
        ctx->n -= str_n;
    }
}
