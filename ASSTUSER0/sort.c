#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static long get_micros(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec;
}

int compare(const void *x, const void *y) {
    int a = *((int*)x);
    int b = *((int*)y);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int main(int argc, char **argv) {

    FILE *fp_log = fopen("log.txt", "r");
    FILE *fp_sorted = fopen("sorted.txt", "w");

    if (!fp_log) {
        fprintf(stderr, "FILE DOES NOT EXIST\n");
        return 1;
    }

    // each line will be of max 256 bytes
    const unsigned MAX_LENGTH = 256;
    char line[MAX_LENGTH];

    // get total number of integers in the file
    char *n_str = fgets(line, MAX_LENGTH, fp_log);
    int n = atoi(n_str);
    fputs(n_str,fp_sorted);

    // create array from log.txt
    int arr[n];
    for (int i = 0; i < n; i++) {
        int elem = atoi(fgets(line, MAX_LENGTH, fp_log));
        arr[i] = elem;
    }

    long time_start = get_micros();
    //printf("%ld\n", time_start);
    qsort(arr, n, sizeof(int), compare);
    long time_end = get_micros();
    //printf("%ld\n", time_end);
    long duration = time_end - time_start;

    printf("%ld\n", duration);
    
    for (int i = 0; i < n; i++) {
        char str [sizeof(int)*8+1];
        sprintf(str, "%d\n", arr[i]);
        fputs(str, fp_sorted);
    }

    fclose(fp_log);
    fclose(fp_sorted);
    return 0;

}