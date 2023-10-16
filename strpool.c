#if defined(_MSC_VER)
#if !defined _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS /* NOLINT */
#endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */
#include "strpool.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void strpool_init(struct strpool *ctx, size_t m)
{
    ctx->m = m ? m : BUFSIZ;
    ctx->p = (char *)malloc(ctx->m);
    ctx->n = 0;
}

void strpool_grow(struct strpool *ctx, size_t n)
{
    do
    {
        --ctx->m;
        ctx->m |= ctx->m >> 0x01;
        ctx->m |= ctx->m >> 0x02;
        ctx->m |= ctx->m >> 0x04;
        ctx->m |= ctx->m >> 0x08;
        ctx->m |= ctx->m >> 0x10;
#if INT_MAX > 0x7FFF
        ctx->m |= ctx->m >> 0x20;
#endif /* INT_MAX */
        ++ctx->m;
        ctx->m += (ctx->m == 0);
    } while (ctx->n + n > ctx->m);
    ctx->p = (char *)realloc(ctx->p, ctx->m);
}

void strpool_exit(struct strpool *ctx)
{
    free(ctx->p);
    ctx->p = 0;
    ctx->m = 0;
    ctx->n = 0;
}

char *strpool_puts(struct strpool *ctx, char const *str)
{
    return strpool_putn(ctx, str, str ? strlen(str) : 0);
}

char *strpool_putn(struct strpool *ctx, void const *ptr, size_t num)
{
    char *str_p;
    size_t str_n = num + 1;
    if (ctx->n + str_n > ctx->m)
    {
        strpool_grow(ctx, str_n);
    }
    str_p = ctx->p + ctx->n;
    memcpy(str_p, ptr, num);
    ctx->n += str_n;
    str_p[num] = 0;
    return str_p;
}

char *strpool_putv(struct strpool *ctx, char const *fmt, va_list va)
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
        strpool_grow(ctx, str_n);
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

char *strpool_putf(struct strpool *ctx, char const *fmt, ...)
{
    va_list va;
    char *str_p;
    va_start(va, fmt);
    str_p = strpool_putv(ctx, fmt, va);
    va_end(va);
    return str_p;
}

char *strpool_undo(struct strpool *ctx)
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

void strpool_drop(struct strpool *ctx, char const *p)
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

char *strpool_head(struct strpool const *ctx)
{
    return ctx->n ? ctx->p : NULL;
}

char *strpool_next(struct strpool const *ctx, char *cur)
{
    if (cur)
    {
        cur += strlen(cur) + 1;
        if (cur < ctx->p + ctx->n)
        {
            return cur;
        }
    }
    return NULL;
}

char *strpool_tail(struct strpool const *ctx)
{
    if (ctx->n)
    {
        char *p = ctx->p + ctx->n - 1;
        while (--p >= ctx->p && *p)
        {
        }
        return ++p;
    }
    return NULL;
}

char *strpool_prev(struct strpool const *ctx, char *cur)
{
    if (cur && --cur >= ctx->p)
    {
        while (--cur >= ctx->p && *cur)
        {
        }
        return ++cur;
    }
    return NULL;
}

char *strpool_find(struct strpool *ctx, char const *p)
{
    char *res = NULL;
    for (char *cur = ctx->n ? ctx->p : NULL; cur;)
    {
        if (strcmp(p, cur) == 0)
        {
            res = cur;
            break;
        }
        cur += strlen(cur) + 1;
        if (cur >= ctx->p + ctx->n)
        {
            cur = NULL;
        }
    }
    return res;
}
