// Examples of how to call salient timing functions

#define _POSIX_C_SOURCE 199309
#define MILLION 1000000
#define HUNDRED_MILLION 100000000
#define BILLION 1000000000
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

typedef enum
{
    million,
    hundred_million = 100000000,
    billion
} cycles;

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

    return (double)seconds + (double)ns / (double)BILLION;
}

// compute the total seconds for getrusage
static inline double get_seconds_r(long int start_sec, long int end_sec, long int start_usec, long int end_usec)
{
    long int seconds = end_sec - start_sec;
    long int us = end_usec - start_usec;

    return (double)seconds + (double)us / (double)MILLION;
}

// calculate time to run empty loop x times
static inline double get_emptyloop_time(cycles c)
{
    int x;
    if (c == million)
    {
        x = MILLION;
    }
    else if (c == hundred_million)
    {
        x = HUNDRED_MILLION;
    }
    else if (c == billion)
    {
        x = BILLION;
    }

    struct timespec start, finish;

    clock_gettime(CLOCK_REALTIME, &start);
    for (int i = 0; i < x; i++)
    {
        // Empty Loop
    }
    clock_gettime(CLOCK_REALTIME, &finish);

    return get_seconds(start.tv_sec, finish.tv_sec, start.tv_nsec, finish.tv_nsec);
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

    double seconds = get_seconds_r(start.ru_utime.tv_sec, finish.ru_utime.tv_sec, start.ru_utime.tv_usec, finish.ru_utime.tv_usec) - get_emptyloop_time(hundred_million);
    t->utime = seconds;
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
    printf("avg user time for %s: %e seconds\n", fname, avg_utime);
    printf("avg system time for %s: %e seconds\n", fname, avg_stime);
}

// test time to run sempost over 10 trials
static inline void test_sempost(result *times)
{
    double loop_time = get_emptyloop_time(hundred_million);
    for (int i = 0; i < 10; i++)
    {
        times[i].utime = 0.0;
        times[i].stime = 0.0;
        get_sempost_time(&times[i], loop_time);
        // printf("trial %d: total user time: %e seconds\n", i, times[i].utime);
        // printf("trial %d: total system time: %e seconds\n", i, times[i].stime);
    }
    calc_ttime(times, 10, hundred_million, "sem_post");
}
int main()
{
    result sempost_time[10];
    test_sempost(sempost_time);

    // how to invoke mutex locks
    pthread_mutex_t onelock;
    pthread_mutex_init(&onelock, NULL);
    pthread_mutex_lock(&onelock);
    pthread_mutex_unlock(&onelock);

    // how to open a file
    int fd = open("/etc/motd", O_RDONLY);
    // did it work?
    if (fd < 0)
        perror("open");
    close(fd);

    // how to allocate memory
    void *p;
    p = sbrk(8024);
    // did it work?
    if (p == NULL)
        perror("sbrk");

    // how to read user and system time
    struct rusage buf;
    getrusage(RUSAGE_SELF, &buf);
    double u = (double)buf.ru_utime.tv_sec + ((double)buf.ru_utime.tv_usec) / 1e6;
    double s = (double)buf.ru_stime.tv_sec + ((double)buf.ru_stime.tv_usec) / 1e6;
    printf("user=%e system=%e\n", u, s);

    printf("seconds per tick = %e\n", seconds_per_tick());
}