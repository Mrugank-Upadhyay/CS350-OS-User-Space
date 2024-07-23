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

int main(int argc, char **argv)
{
    long random_seed = get_micros();
    srand((unsigned)random_seed);

    // if any arguments are missing or if hi < lo or if n < 1
    if (argc < 3 || (atoi(argv[2]) > atoi(argv[3])) || atoi(argv[1]) < 1)
    {
        return 1;
    }

    FILE *fp;

    fp = fopen("log.txt", "w");
    int n = atoi(argv[1]);
    int lo = atoi(argv[2]);
    int hi = atoi(argv[3]);

    fputs(strcat(argv[1], "\n"), fp);

    // if hi == lo
    if (hi == lo)
    {
        char *str = strcat(argv[2], "\n");
        for (int i = 0; i < n; i++)
        {
            // printf("%s", str);
            fputs(str, fp);
        }
    }

    // Otherwise if hi > lo
    for (int i = 0; i < n; i++){
        __uint64_t random;
        random = rand();
        random = random << 32;
        random = random + rand();
        random = lo + random % (hi - lo + 1);
        __int64_t random_signed = random;
        int rand = (int)random_signed;
        char str [sizeof(__int64_t)*8+1];
        sprintf(str, "%d\n", rand);
        fputs(str, fp);
    }

    fclose(fp);

    return 0;
}