#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 32768
#define ITERATIONS 10000

int main(void)
{
    int data[SIZE];

    // Populate array with random values
    for (int i = 0; i < SIZE; ++i)
        data[i] = rand() % 1024;

    // Uncomment to sort the array
    // qsort(data, SIZE, sizeof(int), compare);

    long long sum = 0;

    clock_t start = clock();

    // 327,680,000 total iterations
    for (int i = 0; i < ITERATIONS; ++i)
    {
        for (int j = 0; j < SIZE; ++j)
        {
            // Branch being evaluated by the CPU's BPU
            if (data[j] >= 512)
                sum += data[j];
        }
    }

    clock_t end = clock();

    double duration =
        (double)(end - start) / CLOCKS_PER_SEC;

    printf("Time: %.6f seconds\n", duration);
    printf("Checksum: %lld\n", sum);

    return 0;
}
