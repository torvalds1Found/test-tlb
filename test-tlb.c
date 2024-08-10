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
#define FREQ 3.9 // CPU frequency in GHz
#define HUGEPAGE (2 * 1024 * 1024) // Hugepage size: 2MB

// Flags for controlling test behavior
static int test_hugepage = 0; // Use hugepages
static int random_list = 0;   // Use a random list

// Function to print an error message and exit
static void die(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

// Global flag to stop the timing test
static volatile int stop = 0;

// Signal handler for alarm to stop the test
void alarm_handler(int sig)
{
    stop = 1;
}

// Calculate the difference in microseconds between two timeval structures
unsigned long usec_diff(struct timeval *a, struct timeval *b)
{
    return (b->tv_sec - a->tv_sec) * 1000000 + (b->tv_usec - a->tv_usec);
}

// Perform a warmup run to ensure timing is accurate
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

// Perform the test and return the average cycle time in nanoseconds
static double do_test(void *map)
{
    unsigned long count = 0, offset = 0, usec;
    struct timeval start, end;
    struct itimerval itval = { .it_interval = { 0, 0 }, .it_value = { 0, 0 } };

    // Perform a warmup run and set timing granularity
    usec = warmup(map) * 5;
    if (usec < 200000) usec = 200000;
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

    // Ensure the compiler doesn't optimize away the offset calculation
    *(volatile unsigned int *)(map + offset);

    // Return cycle time in nanoseconds
    return 1000.0 * usec / count;
}

// Convert a string representing a size to an unsigned long value
static unsigned long get_num(const char *str)
{
    char *end;
    unsigned long val = 0;

    if (!str) return 0;

    val = strtoul(str, &end, 0);
    if (val == ULONG_MAX || val == 0) return 0;

    switch (*end) {
        case 'k': val <<= 10; break;
        case 'M': val <<= 20; break;
        case 'G': val <<= 30; break;
        default: return 0;
    }

    return val;
}

// Randomize the map to create a random access pattern
static void randomize_map(void *map, unsigned long size, unsigned long stride)
{
    unsigned long off;
    unsigned int *lastpos, *rnd;
    int n;

    rnd = calloc(size / stride + 1, sizeof(unsigned int));
    if (!rnd) die("Out of memory");

    // Create a sorted list of offsets
    for (n = 0, off = 0; off < size; n++, off += stride)
        rnd[n] = off;

    // Shuffle the offsets randomly
    for (n = 0, off = 0; off < size; n++, off += stride) {
        unsigned int m = rand() % (size / stride);
        unsigned int tmp = rnd[n];
        rnd[n] = rnd[m];
        rnd[m] = tmp;
    }

    // Create a circular list from the random offsets
    lastpos = map;
    for (n = 0, off = 0; off < size; n++, off += stride) {
        lastpos = map + rnd[n];
        *lastpos = rnd[n + 1];
    }
    *lastpos = rnd[0];

    free(rnd);
}

// Create a memory map with either hugepages or regular pages
static void *create_map(void *map, unsigned long size, unsigned long stride)
{
    unsigned int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    unsigned long off, mapsize;
    unsigned int *lastpos;

    if (test_hugepage) {
        // Allocate enough space for hugepages
        mapsize = size + 2 * HUGEPAGE;
        // Align the map to hugepage boundary
        unsigned long mapstart = ((unsigned long)map + HUGEPAGE - 1) & ~(HUGEPAGE - 1);
        map = mmap((void *)mapstart, mapsize, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (map == MAP_FAILED) die("mmap failed");

        // Optionally, you can use madvise for other purposes if needed
        // madvise(map, mapsize, MADV_DONTNEED);
    } else {
        // Regular pages
        map = mmap(map, size, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (map == MAP_FAILED) die("mmap failed");
        // madvise(map, size, MADV_NOHUGEPAGE); // This may not be necessary
    }

    // Initialize the map with a simple pattern
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
    unsigned long stride, size;
    const char *arg;
    void *map = NULL;
    double cycles = 1e10;

    // Seed the random number generator
    srand(time(NULL));

    // Parse command line arguments
    while ((arg = argv[1]) != NULL) {
        if (*arg != '-') break;
        switch (*++arg) {
            case 'H': test_hugepage = 1; break;
            case 'r': random_list = 1; break;
            default: die("Unknown flag '%s'", arg);
        }
        argv++;
    }

    size = get_num(argv[1]);
    stride = get_num(argv[2]);
    if (stride < 4 || size < stride) die("Bad arguments: test-tlb [-H] <size> <stride>");

    // Perform the test multiple times to get the best result
    for (int i = 0; i < 5; i++) {
        double d;

        map = create_map(map, size, stride);
        if (random_list)
            randomize_map(map, size, stride);

        d = do_test(map);
        if (d < cycles)
            cycles = d;
    }

    // Print the results
    printf("%6.2fns (~%.1f cycles)\n", cycles, cycles * FREQ);
    return EXIT_SUCCESS;
}
