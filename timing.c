// Examples of how to call salient timing functions

#define _POSIX_C_SOURCE 199309
#define MILLION 1000000
#define TEN_MILLION 10000000
#define HUNDRED_MILLION 100000000
#define BILLION 1000000000
#define THOUSAND 1000
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

typedef struct result
{
    double stime;
    double utime;
} result;

// not declared in standard headers
extern void *sbrk(intptr_t);

// get the resolution of the real time clock
static double seconds_per_tick()
{
    struct timespec res;
    clock_getres(CLOCK_REALTIME, &res);
    double resolution = res.tv_sec + (((double)res.tv_nsec) / 1.0e9);
    return resolution;
}

// compute the total seconds for clock_gettime
static inline double get_seconds(time_t start_sec, time_t end_sec, long start_nsec, long end_nsec)
{
    long seconds = end_sec - start_sec;
    long ns = end_nsec - start_nsec;

    if (start_nsec > end_nsec)
    {
        --seconds;
        ns += 1000000000;
    }

    return (double)seconds + (double)ns / 1e9;
}

// compute the total seconds for getrusage
static inline double get_seconds_r(long int start_sec, long int end_sec, long int start_usec, long int end_usec)
{
    long int seconds = end_sec - start_sec;
    long int us = end_usec - start_usec;

    if (start_usec > end_usec)
    {
        --seconds;
        us += 1000000;
    }
    return (double)seconds + (double)us / 1e6;
}

// calculate time to run empty loop x times
static inline double get_emptyloop_time(int x)
{
    struct timespec start, finish;

    clock_gettime(CLOCK_REALTIME, &start);
    for (int i = 0; i < x; i++)
    {
        // Empty Loop
    }
    clock_gettime(CLOCK_REALTIME, &finish);

    return get_seconds(start.tv_sec, finish.tv_sec, start.tv_nsec, finish.tv_nsec);
}

static inline double get_emptyloop_time_r(int x)
{
    struct rusage start, finish;

    getrusage(RUSAGE_SELF, &start);
    for (int i = 0; i < x; i++)
    {
        // Empty Loop
    }
    getrusage(RUSAGE_SELF, &finish);

    return get_seconds_r(start.ru_utime.tv_sec, finish.ru_utime.tv_sec, start.ru_utime.tv_usec, finish.ru_utime.tv_usec);
}

// calculating time for sempost using clock-gettime
static inline void get_sempost_time(result *t, double lt)
{
    struct timespec start, finish;
    // how to manipulate semaphores
    sem_t onesem;
    int success = sem_init(&onesem, 0, 0);
    if (success < 0)
        perror("sem_init");

    clock_gettime(CLOCK_REALTIME, &start);
    for (int i = 0; i < HUNDRED_MILLION; i++)
    {
        success = sem_post(&onesem);
    }
    clock_gettime(CLOCK_REALTIME, &finish);
    if (sem_destroy(&onesem) < 0)
    {
        fprintf(stderr, "Error: %d\n", errno);
        exit(errno);
    }

    double seconds = get_seconds(start.tv_sec, finish.tv_sec, start.tv_nsec, finish.tv_nsec) - lt;
    t->utime = seconds;
}

// calculating time for sempost using getrusage
static inline void get_sempost_usage(result *t)
{
    struct rusage start, finish;
    // how to manipulate semaphores
    sem_t onesem;
    int success = sem_init(&onesem, 0, 0);
    if (success < 0)
        perror("sem_init");

    getrusage(RUSAGE_SELF, &start);
    for (int i = 0; i < HUNDRED_MILLION; i++)
    {
        success = sem_post(&onesem);
    }
    getrusage(RUSAGE_SELF, &finish);
    if (sem_destroy(&onesem) < 0)
    {
        fprintf(stderr, "Error: %d\n", errno);
        exit(errno);
    }

    double utime = get_seconds_r(start.ru_utime.tv_sec, finish.ru_utime.tv_sec, start.ru_utime.tv_usec, finish.ru_utime.tv_usec) - get_emptyloop_time(HUNDRED_MILLION);
    t->utime = utime;
}

static inline void get_pthread_rtime(result *t, double lt)
{
    struct rusage start, finish;

    pthread_mutex_t *locks = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * TEN_MILLION);

    if (locks == NULL)
    {
        printf("malloc of size %d failed \n", TEN_MILLION);
        exit(1);
    }

    for (int i = 0; i < TEN_MILLION; i++)
    {
        pthread_mutex_init(&locks[i], NULL);
    }
    getrusage(RUSAGE_SELF, &start);
    for (int i = 0; i < TEN_MILLION; i++)
    {
        pthread_mutex_lock(&locks[i]);
    }
    getrusage(RUSAGE_SELF, &finish);
    for (int i = 0; i < TEN_MILLION; i++)
    {
        pthread_mutex_unlock(&locks[i]);
    }

    free(locks);
    // time sometimes is zero, should we count that into the average?
    double utime = get_seconds_r(start.ru_utime.tv_sec, finish.ru_utime.tv_sec, start.ru_utime.tv_usec, finish.ru_utime.tv_usec) - lt;
    double stime = get_seconds_r(start.ru_stime.tv_sec, finish.ru_stime.tv_sec, start.ru_stime.tv_usec, finish.ru_stime.tv_usec);

    // printf("total user time for pthread: %e seconds\n", t->utime);
    // printf("total system time for pthread: %e seconds\n", t->stime);
}

static inline void get_open_time(result *times, char *filepath, double lt)
{

    int i, fd;
    struct rusage start, finish;
    int first_fd = open(filepath, O_RDONLY);

    getrusage(RUSAGE_SELF, &start);
    for (i = 0; i < THOUSAND; i++)
    {
        fd = open(filepath, O_RDONLY);
    }
    getrusage(RUSAGE_SELF, &finish);

    for (i = fd; i >= first_fd; i--)
    {
        if (close(i) != 0)
        {
            perror("failed to close() on");
            printf("fd %d\n");
        }
    }

    assert(i == first_fd - 1);
    double utime = get_seconds_r(start.ru_utime.tv_sec, finish.ru_utime.tv_sec, start.ru_utime.tv_usec, finish.ru_utime.tv_usec) - lt;
    double stime = get_seconds_r(start.ru_stime.tv_sec, finish.ru_stime.tv_sec, start.ru_stime.tv_usec, finish.ru_stime.tv_usec);
    if (utime < 0)
    {
        times->utime = 0.0;
    }
    else
    {
        times->utime = utime;
    }

    times->stime = stime;
    // printf("total user time for open: %e seconds\n", times->utime);
    // printf("total system time for open: %e seconds\n", times->stime);
}

static inline void get_sbrk_time(result *times, int size, double lt)
{
    struct rusage start, finish;
    void *p;
    getrusage(RUSAGE_SELF, &start);
    for (int i = 0; i < MILLION; i++)
    {
        p = sbrk(size);
    }
    getrusage(RUSAGE_SELF, &finish);

    double utime = get_seconds_r(start.ru_utime.tv_sec, finish.ru_utime.tv_sec, start.ru_utime.tv_usec, finish.ru_utime.tv_usec) - lt;
    double stime = get_seconds_r(start.ru_stime.tv_sec, finish.ru_stime.tv_sec, start.ru_stime.tv_usec, finish.ru_stime.tv_usec);
    times->utime = utime;
    times->stime = stime;
}
// caculate the time for a single function call using clock_gettime
static inline void calc_ttime(result *times, int size, int c, char *fname)
{

    double sum_utime = 0.0;
    double avg_utime = 0.0;
    double sum_stime = 0.0;
    double avg_stime = 0.0;
    for (int i = 0; i < size; i++)
    {
        sum_utime = sum_utime + times[i].utime;
        sum_stime = sum_stime + times[i].stime;
    }

    avg_utime = sum_utime / (double)size / (double)c;
    avg_stime = sum_stime / (double)size / (double)c;
    printf("%s: ", fname);
    printf("user= %e ", avg_utime);
    printf("system= %e \n", avg_stime);
}

// test time to run sempost over 10 trials
static inline void test_sempost(result *times)
{
    double loop_time = get_emptyloop_time(HUNDRED_MILLION);
    for (int i = 0; i < 10; i++)
    {
        times[i].utime = 0.0;
        times[i].stime = 0.0;
        get_sempost_time(&times[i], loop_time);
    }
    calc_ttime(times, 10, HUNDRED_MILLION, "sem_post");
}

static inline void test_pthread(result *times)
{
    double loop_time = get_emptyloop_time_r(TEN_MILLION);
    for (int i = 0; i < 10; i++)
    {
        times[i].utime = 0.0;
        times[i].stime = 0.0;
        get_pthread_rtime(&times[i], loop_time);
    }

    calc_ttime(times, 10, TEN_MILLION, "pthread_mutex_lock");
}

static inline void make_dirs()
{
    system("mkdir test");
    system("mkdir test/2");
    system("mkdir test/2/3");
    system("mkdir test/2/3/4");
    system("mkdir test/2/3/4/5");
    system("mkdir test/2/3/4/5/6");
    system("mkdir test/2/3/4/5/6/7");
    system("mkdir test/2/3/4/5/6/7/8");
    system("mkdir test/2/3/4/5/6/7/8/9");
    system("mkdir test/2/3/4/5/6/7/8/9/10");
    system("touch test/small_file");

    system("touch test/medium_file");
    system("touch test/2/3/4/5/medium_file");
    system("touch test/2/3/4/5/6/7/8/9/10/medium_file");

    system("touch test/large_file");

    system("echo 'hello world, goodbye world.' >> test/small_file");

    int size = 1000;
    for (int i = 0; i < size; i++)
    {
        system("echo 'hello world, goodbye world.' >> test/medium_file");
    }

    for (int i = 0; i < size; i++)
    {
        system("echo 'hello world, goodbye world.' >> test/2/3/4/5/medium_file");
    }

    for (int i = 0; i < size; i++)
    {
        system("echo 'hello world, goodbye world.' >> test/2/3/4/5/6/7/8/9/10/medium_file");
    }

    size = 10000;
    for (int i = 0; i < size; i++)
    {
        system("echo 'hello world, goodbye world.' >> test/large_file");
    }
}

static inline void remove_dirs()
{
    printf("\ndeleting all test files created\n");
    system("rm -rf test");
}

static inline void test_open(result *times)
{
    make_dirs();

    char *filepath = "test/small_file";
    double loop_time = get_emptyloop_time_r(THOUSAND);

    printf("small file in a subdirectory of depth 1\n");
    get_open_time(times, filepath, loop_time);
    calc_ttime(times, 1, THOUSAND, "open");

    filepath = "test/medium_file";
    printf("\nmedium file in a subdirectory of depth 1\n");
    get_open_time(times, filepath, loop_time);
    calc_ttime(times, 1, THOUSAND, "open");

    filepath = "test/large_file";
    printf("\nlarge file in a subdirectory of depth 1\n");
    get_open_time(times, filepath, loop_time);
    calc_ttime(times, 1, THOUSAND, "open");

    filepath = "test/2/3/4/5/medium_file";
    printf("\nmedium file in a subdirectory of depth 5\n");
    get_open_time(times, filepath, loop_time);
    calc_ttime(times, 1, THOUSAND, "open");

    filepath = "test/2/3/4/5/6/7/8/9/10/medium_file";
    printf("\nmedium file in a subdirectory of depth 10\n");
    get_open_time(times, filepath, loop_time);
    calc_ttime(times, 1, THOUSAND, "open");

    remove_dirs();
}

static inline void test_sbrk(result *times)
{
    double loop_time = get_emptyloop_time_r(MILLION);

    get_sbrk_time(times, 8024, loop_time);
    calc_ttime(times, 1, MILLION, "sbrk");
}

int main()
{
    result sempost_time[10];
    result pthread_time[10];
    result open_time;
    test_sempost(sempost_time);
    test_pthread(pthread_time);
    test_open(&open_time);

    // how to allocate memory
    void *p;
    p = sbrk(8024);
    // did it work?
    if (p == NULL)
        perror("sbrk");
}