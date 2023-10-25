#define STRPOOL_LOOP
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

#if defined(_WIN32)
// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/msize
#include <malloc.h>
#define STRPOOL_MSIZE(x) _msize(x)
#elif defined(__APPLE__)
// https://www.unix.com/man-page/osx/3/malloc_size
#include <malloc/malloc.h>
#define STRPOOL_MSIZE(x) malloc_size(x)
#elif defined(__linux__)
// https://linux.die.net/man/3/malloc_usable_size
#include <malloc.h>
#define STRPOOL_MSIZE(x) malloc_usable_size(x)
#else
#error "oops, I don't know this system!"
#endif
size_t strpool_size(void const *block)
{
    union
    {
        void const *in;
        void *out;
    } u = {block};
    return STRPOOL_MSIZE(u.out);
}

#if INT_MAX > 0x7FFF
#define STRPOOL_ROUNDUP(x)  \
    do                      \
    {                       \
        --(x);              \
        (x) |= (x) >> 0x01; \
        (x) |= (x) >> 0x02; \
        (x) |= (x) >> 0x04; \
        (x) |= (x) >> 0x08; \
        (x) |= (x) >> 0x10; \
        (x) |= (x) >> 0x20; \
        ++(x);              \
    } while (0)
#else /* !INT_MAX */
#define STRPOOL_ROUNDUP(x)  \
    do                      \
    {                       \
        --(x);              \
        (x) |= (x) >> 0x01; \
        (x) |= (x) >> 0x02; \
        (x) |= (x) >> 0x04; \
        (x) |= (x) >> 0x08; \
        (x) |= (x) >> 0x10; \
        ++(x);              \
    } while (0)
#endif /* INT_MAX */

void strpool_pool_init(struct strpool *ctx, size_t m)
{
    if (m)
    {
        STRPOOL_ROUNDUP(m);
    }
    else
    {
        m = STRPOOL_INIT_MIN;
    }
#if defined(STRPOOL_LOOP)
    ctx->pool.ptr = (char **)malloc(sizeof(char *) * m);
#else /* !STRPOOL_LOOP */
    ctx->pool.ptr = NULL;
#endif /* STRPOOL_LOOP */
    ctx->pool.mem = 0;
    ctx->pool.num = 0;
}

char *strpool_pool_load(struct strpool *ctx, size_t num)
{
    num = ((num ? num : 1) + (sizeof(size_t) - 1)) & ~(sizeof(size_t) - 1);
#if defined(STRPOOL_LOOP)
    char **cur = ctx->pool.ptr, **tail = ctx->pool.ptr + ctx->pool.num;
    for (; cur < tail && STRPOOL_MSIZE(*cur) < num; ++cur)
    {
    }
    if (cur == tail)
    {
        return (char *)malloc(num);
    }
    char *res = *cur, **head = cur + 1;
    if (head < tail)
    {
        memmove(cur, head, (size_t)((char *)tail - (char *)head));
    }
    --ctx->pool.num;
    return res;
#else /* !STRPOOL_LOOP */
    (void)ctx;
    return (char *)malloc(num);
#endif /* STRPOOL_LOOP */
}

void strpool_pool_dump(struct strpool *ctx, char *str)
{
#if defined(STRPOOL_LOOP)
    size_t n = STRPOOL_MSIZE(str);
    if (ctx->pool.num >= ctx->pool.mem)
    {
        ctx->pool.mem = ctx->pool.mem ? (ctx->pool.mem << 1) : STRPOOL_INIT_MIN;
        STRPOOL_ROUNDUP(ctx->pool.mem);
        ctx->pool.ptr = (char **)realloc(ctx->pool.ptr, sizeof(char *) * ctx->pool.mem);
    }
    char **cur = ctx->pool.ptr + ctx->pool.num, **tail = cur;
    while (cur > ctx->pool.ptr)
    {
        if (STRPOOL_MSIZE(*--cur) <= n)
        {
            ++cur;
            break;
        }
    }
    if (cur < tail)
    {
        memmove(cur + 1, cur, (size_t)((char *)tail - (char *)cur));
    }
    ++ctx->pool.num;
    *cur = str;
#else /* !STRPOOL_LOOP */
    (void)ctx;
    free(str);
#endif /* STRPOOL_LOOP */
}

void strpool_pool_exit(struct strpool *ctx)
{
#if defined(STRPOOL_LOOP)
    while (ctx->pool.num)
    {
        free(ctx->pool.ptr[--ctx->pool.num]);
    }
    free(ctx->pool.ptr);
    ctx->pool.ptr = 0;
    ctx->pool.mem = 0;
#else /* !STRPOOL_LOOP */
    ctx->pool.ptr = NULL;
    ctx->pool.mem = 0;
    ctx->pool.num = 0;
#endif /* STRPOOL_LOOP */
}

int strpool_compare(void const *lhs, void const *rhs)
{
    return strcmp((char const *)lhs, *(char *const *)rhs);
}

int strpool_reverse(void const *lhs, void const *rhs)
{
    return strcmp(*(char *const *)rhs, (char const *)lhs);
}

void strpool_init(struct strpool *ctx, int (*cmp)(void const *, void const *), size_t m)
{
    strpool_pool_init(ctx, m);
    if (m)
    {
        STRPOOL_ROUNDUP(m);
    }
    else
    {
        m = STRPOOL_INIT_MIN;
    }
    ctx->cmp = cmp ? cmp : strpool_compare;
    ctx->ptr = (char **)malloc(sizeof(char *) * m);
    ctx->mem = m;
    ctx->num = 0;
}

char *const *strpool_load(struct strpool *ctx, char *str)
{
    if (ctx->num >= ctx->mem)
    {
        if (ctx->mem)
        {
            STRPOOL_ROUNDUP(ctx->mem);
            ctx->mem <<= 1;
        }
        else
        {
            ctx->mem = STRPOOL_INIT_MIN;
        }
        ctx->ptr = (char **)realloc(ctx->ptr, sizeof(char *) * ctx->mem);
    }
    char **cur = ctx->ptr + ctx->num, **tail = cur;
    while (cur > ctx->ptr)
    {
        if (ctx->cmp(str, --cur) >= 0)
        {
            ++cur;
            break;
        }
    }
    if (cur < tail)
    {
        memmove(cur + 1, cur, (size_t)((char *)tail - (char *)cur));
    }
    ++ctx->num;
    *cur = str;
    return cur;
}

void strpool_dump(struct strpool *ctx, void const *ptr)
{
    union
    {
        void const *in;
        char **out;
    } u = {ptr};
    char **str = u.out;
    char **tail = ctx->ptr + ctx->num;
    if (str >= ctx->ptr && str < tail)
    {
        char **head = str + 1;
        strpool_pool_dump(ctx, *str);
        if (head < tail)
        {
            memmove(str, head, (size_t)((char *)tail - (char *)head));
        }
        --ctx->num;
    }
}

void strpool_exit(struct strpool *ctx)
{
    strpool_pool_exit(ctx);
    while (ctx->num)
    {
        free(ctx->ptr[--ctx->num]);
    }
    free(ctx->ptr);
    ctx->ptr = 0;
    ctx->mem = 0;
}

void strpool_sort(struct strpool const *ctx)
{
    qsort(ctx->ptr, ctx->num, sizeof(char *), ctx->cmp);
}

char *const *strpool_find(struct strpool const *ctx, char const *str)
{
    return (char *const *)bsearch(str, ctx->ptr, ctx->num, sizeof(char *), ctx->cmp);
}

char *const *strpool_puts(struct strpool *ctx, char const *str)
{
    return strpool_putn(ctx, str, str ? strlen(str) : 0);
}

char *const *strpool_putn(struct strpool *ctx, void const *ptr, size_t num)
{
    char *str = strpool_pool_load(ctx, num + 1);
    memcpy(str, ptr, num);
    str[num] = 0;
    return strpool_load(ctx, str);
}

char *const *strpool_putv(struct strpool *ctx, char const *fmt, va_list va)
{
    int num;
    char *str;
    va_list ap;
    va_copy(ap, va);
    num = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    str = strpool_pool_load(ctx, (size_t)(num + 1));
    va_copy(ap, va);
    num = vsnprintf(str, (size_t)(num + 1), fmt, ap);
    va_end(ap);
    return strpool_load(ctx, str);
}

char *const *strpool_putf(struct strpool *ctx, char const *fmt, ...)
{
    va_list va;
    char *const *str;
    va_start(va, fmt);
    str = strpool_putv(ctx, fmt, va);
    va_end(va);
    return str;
}
