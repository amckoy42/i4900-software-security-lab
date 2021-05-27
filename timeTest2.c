#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int main(){
    int size = 2000000;
    srand (time(NULL));
    int a[size];
    int n = rand() % size;

    struct timeval tval_before, tval_after, tval_result;

    gettimeofday(&tval_before, NULL);
    a[n] = 10;
    sleep(1);
    gettimeofday(&tval_after, NULL);

    timersub(&tval_after, &tval_before, &tval_result);

    printf("%s%d\n", "Random number ", n);
    printf("Time elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);

}