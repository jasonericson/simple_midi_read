#include <sys/time.h>

#define START_TIMER()\
{\
    struct timeval start, end;\
    double elapsed_time;\
    gettimeofday(&start, 0);

#define END_TIMER()\
    gettimeofday(&end, 0);\
    elapsed_time = (end.tv_usec - start.tv_usec) / 1000.0;\
    printf("Time spent: %f milliseconds.\n", elapsed_time);\
}
