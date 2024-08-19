#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <time.h>

#define PAGE_SIZE 4096
#define FREQ 3.9

static int test_hugepage = 0;
static int random_list = 0;

static void die(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fputc('\n', stderr);
    exit(1);
}

static volatile int stop = 0;

void alarm_handler(int sig)
{
    stop = 1;
}

unsigned long usec_diff(const struct timeval *a, const struct timeval *b)
{
    return (b->tv_sec - a->tv_sec) * 1000000 + (b->tv_usec - a->tv_usec);
}

static unsigned long warmup(void *map)
{
    unsigned int offset = 0;
    struct timeval start, end;

    gettimeofday(&start, NULL);
    do {
        offset = *(volatile unsigned int *)(map + offset);
    } while (offset);
    gettimeofday(&end, NULL);
    return usec_diff(&start, &end);
}

static double do_test(void *map)
{
    unsigned long count = 0, offset = 0, usec;
    struct timeval start, end;
    struct itimerval itval = { { 0, 0 }, { 0, 0 } };

    usec = warmup(map) * 5;
    if (usec < 200000)
        usec = 200000;
    itval.it_value.tv_sec = usec / 1000000;
    itval.it_value.tv_usec = usec % 1000000;

    stop = 0;
    signal(SIGALRM, alarm_handler);
    setitimer(ITIMER_REAL, &itval, NULL);

    gettimeofday(&start, NULL);
    do {
        count++;
        offset = *(unsigned int *)(map + offset);
    } while (!stop);
    gettimeofday(&end, NULL);
    usec = usec_diff(&start, &end);

    *(volatile unsigned int *)(map + offset);

    return 1000 * (double)usec / count;
}

static unsigned long get_num(const char *str)
{
    if (!str || !*str) {
        die("Invalid input: NULL or empty string");
    }

    char *end;
    unsigned long val = strtoul(str, &end, 0);

    if (end == str || val == 0 || val == ULONG_MAX) {
        die("Invalid number: %s", str);
    }

    while (*end) {
        switch (*end++) {
            case 'k': case 'K': val <<= 10; break;
            case 'M': case 'm': val <<= 20; break;
            case 'G': case 'g': val <<= 30; break;
            default: die("Invalid suffix: %c", *(end-1));
        }
    }

    return val;
}

static void randomize_map(void *map, unsigned long size, unsigned long stride)
{
    unsigned long off;
    unsigned int *lastpos, *rnd;
    int n;

    rnd = calloc(size / stride + 1, sizeof(unsigned int));
    if (!rnd)
        die("Out of memory");

    for (n = 0, off = 0; off < size; n++, off += stride)
        rnd[n] = off;

    for (n = 0, off = 0; off < size; n++, off += stride) {
        unsigned int m = (unsigned long)random() % (size / stride);
        unsigned int tmp = rnd[n];
        rnd[n] = rnd[m];
        rnd[m] = tmp;
    }

    lastpos = map;
    for (n = 0, off = 0; off < size; n++, off += stride) {
        lastpos = map + rnd[n];
        *lastpos = rnd[n + 1];
    }
    *lastpos = rnd[0];

    free(rnd);
}

#define HUGEPAGE (2 * 1024 * 1024)

static void *create_map(void *map, unsigned long size, unsigned long stride)
{
    unsigned int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    unsigned long off, mapsize;
    unsigned int *lastpos;

    if (map) {
        if (test_hugepage)
            return map;
        flags |= MAP_FIXED;
    }

    mapsize = size;
    if (test_hugepage)
        mapsize += 2 * HUGEPAGE;

    map = mmap(map, mapsize, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (map == MAP_FAILED)
        die("mmap failed: %s", strerror(errno));

    if (test_hugepage) {
        unsigned long mapstart = (unsigned long)map;
        mapstart = (mapstart + HUGEPAGE - 1) & ~(HUGEPAGE - 1);
        map = (void *)mapstart;

        mapsize = (size + HUGEPAGE - 1) & ~(HUGEPAGE - 1);
        madvise(map, mapsize, MADV_HUGEPAGE);
    } else {
        madvise(map, mapsize, MADV_NOHUGEPAGE);
    }

    lastpos = map;
    for (off = 0; off < size; off += stride) {
        lastpos = map + off;
        *lastpos = off + stride;
    }
    *lastpos = 0;

    return map;
}

int main(int argc, char **argv)
{
    if (argc < 3)
        die("Usage: test-tlb [-H] [-r] <size> <stride>");

    unsigned long stride, size;
    const char *arg;
    void *map = NULL;
    double cycles = 1e10;

    srandom(time(NULL));

    while ((arg = argv[1]) != NULL) {
        if (*arg != '-')
            break;
        for (;;) {
            switch (*++arg) {
                case 0:
                    break;
                case 'H':
                    test_hugepage = 1;
                    continue;
                case 'r':
                    random_list = 1;
                    continue;
                default:
                    die("Unknown flag '%c'", *arg);
            }
            break;
        }
        argv++;
    }

    size = get_num(argv[1]);
    stride = get_num(argv[2]);
    if (stride < 4 || size < stride)
        die("Invalid arguments: test-tlb [-H] [-r] <size> <stride>");

    for (int i = 0; i < 5; i++) {
        double d;

        map = create_map(map, size, stride);
        if (random_list)
            randomize_map(map, size, stride);

        d = do_test(map);
        if (d < cycles)
            cycles = d;
    }

    printf("%6.2fns (~%.1f cycles)\n", cycles, cycles * FREQ);
    return 0;
}
