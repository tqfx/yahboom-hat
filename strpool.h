#ifndef STRPOOL_VERSION
#define STRPOOL_VERSION 0x20231029

#include <stddef.h>
#include <stdarg.h>

// clang-format off
#define STRPOOL_INIT {{NULL, 0, 0}, strpool_compare, NULL, 0, 0}
// clang-format on
#define STRPOOL_INIT_MIN 0x10

struct strpool
{
    struct
    {
        char **ptr;
        size_t mem;
        size_t num;
    } pool;
    int (*cmp)(void const *, void const *);
    char **ptr;
    size_t mem;
    size_t num;
};

#define strpool_pool_foreach(ctx, cur) \
    for (char **cur = (ctx)->pool.ptr, **cur##_ = (ctx)->pool.ptr + (ctx)->pool.num; cur < cur##_; ++cur)
#define strpool_pool_foreach_reverse(ctx, cur) \
    for (char **cur = (ctx)->pool.ptr + (ctx)->pool.num; cur-- > (ctx)->pool.ptr;)
#define strpool_foreach(ctx, cur) \
    for (char **cur = (ctx)->ptr, **cur##_ = (ctx)->ptr + (ctx)->num; cur < cur##_; ++cur)
#define strpool_foreach_reverse(ctx, cur) \
    for (char **cur = (ctx)->ptr + (ctx)->num; cur-- > (ctx)->ptr;)

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

size_t strpool_size(void const *block);

void strpool_pool_init(struct strpool *ctx, size_t m);
char *strpool_pool_load(struct strpool *ctx, size_t num);
void strpool_pool_dump(struct strpool *ctx, char *str);
void strpool_pool_exit(struct strpool *ctx);

int strpool_compare(void const *lhs, void const *rhs);
int strpool_reverse(void const *lhs, void const *rhs);

void strpool_init(struct strpool *ctx, int (*cmp)(void const *, void const *), size_t m);
char *const *strpool_load(struct strpool *ctx, char *str);
void strpool_dump(struct strpool *ctx, void const *ptr);
void strpool_exit(struct strpool *ctx);

void strpool_sort(struct strpool const *ctx);
char *const *strpool_find(struct strpool const *ctx, char const *str);

char *const *strpool_puts(struct strpool *ctx, char const *str);
char *const *strpool_putn(struct strpool *ctx, void const *ptr, size_t num);
char *const *strpool_putv(struct strpool *ctx, char const *fmt, va_list va)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 2, 0)))
#endif
    ;
char *const *strpool_putf(struct strpool *ctx, char const *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 2, 3)))
#endif /* __GNUC__ */
    ;

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* strpool.h */
