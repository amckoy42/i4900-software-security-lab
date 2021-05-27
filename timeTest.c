#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(){
    int size = 1000000;
    srand (time(NULL));
    int a[size];
    int n = rand() % size;

    clock_t start, end;
    double elapsed_time;

    start = clock();
    a[n] = 10;
    sleep(1);
    end = clock();
    elapsed_time = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("%s%d\n", "Random number ", n);
    printf("%s%f\n", "Elapsed time: ", elapsed_time);

}