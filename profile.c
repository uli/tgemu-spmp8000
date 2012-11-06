#include <stdint.h>
#include <string.h>
#include <stdio.h>

struct funcall {
    void *this_fn;
    void *caller;
    int count;
    uint64_t start;
    uint64_t total;
};

struct funcall calls[65536];

static inline int hash(void *this_fn, void *caller)
{
    uint32_t t = (uint32_t)this_fn;
    uint32_t c = (uint32_t)caller;
    return (t >> 16) | (t & 0xffff) | (c >> 16) | (c & 0xffff);
}

void __cyg_profile_func_enter(void *this_fn, void *caller)
{
#if 1
    struct funcall *f = &calls[hash(this_fn, caller)];
    if (f->this_fn != this_fn) {
        f->this_fn = this_fn;
        f->caller = caller;
        f->count = 0;
        f->total = 0;
    }
    f->count++;
    f->start = libgame_utime();
#endif
}

void __cyg_profile_func_exit(void *this_fn, void *caller)
{
#if 1
    struct funcall *f = &calls[hash(this_fn, caller)];
    f->total += libgame_utime() - f->start;
#endif
}

void init_profile(void)
{
    memset(calls, 0, sizeof(calls));
}

void dump_profile(void)
{
    FILE *fp = fopen("profile.txt", "w");
    int i;
    for (i = 0; i < 65536; i++) {
        if (calls[i].this_fn) {
            fprintf(fp, "%u %d %p %p\n", (uint32_t)calls[i].total, calls[i].count, calls[i].this_fn, calls[i].caller);
        }
    }
    fclose(fp);
}
