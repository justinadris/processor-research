#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// 64-bit Xorshift PRNG to shuffle cache lines without biasing
static unsigned long long rng_state = 88172645463325252ULL;

static unsigned long long xorshift64(void) {
    unsigned long long x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng_state = x;
}

// Build a randomized pointer-chasing chain over 64-byte cache line strides
void build_pointer_chain(size_t* array, size_t size_bytes) {
    size_t line_size = 64; // x86 cache line size
    size_t num_lines = size_bytes / line_size;
    size_t elements_per_line = line_size / sizeof(size_t);

    size_t* indices = (size_t*)malloc(num_lines * sizeof(size_t));
    if (!indices) return;

    for (size_t i = 0; i < num_lines; i++) {
        indices[i] = i;
    }

    // Fisher-Yates shuffle
    for (size_t i = num_lines - 1; i > 0; i--) {
        size_t j = xorshift64() % (i + 1);
        size_t temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    // Link nodes together into a closed circular loop
    for (size_t i = 0; i < num_lines - 1; i++) {
        size_t curr = indices[i] * elements_per_line;
        size_t next = indices[i + 1] * elements_per_line;
        array[curr] = next;
    }
    array[indices[num_lines - 1] * elements_per_line] = indices[0] * elements_per_line;

    free(indices);
}

double measure_latency_ns(size_t size_bytes, size_t total_accesses) {
    size_t* array = (size_t*)malloc(size_bytes);
    if (!array) return -1.0;

    build_pointer_chain(array, size_bytes);

    // Warm up the cache
    volatile size_t ptr = 0;
    size_t num_elements = size_bytes / sizeof(size_t);
    for (size_t i = 0; i < num_elements; i++) {
        ptr = array[ptr];
    }

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    // Latency loop: OoOE engine is blocked waiting for 'ptr'
    for (size_t i = 0; i < total_accesses; i++) {
        ptr = array[ptr];
    }

    QueryPerformanceCounter(&end);

    // Prevent compiler from optimizing out the loop
    if (ptr == 0xDEADBEEF) printf(" ");

    free(array);

    double elapsed_sec = (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart;
    return (elapsed_sec * 1e9) / (double)total_accesses; // Nanoseconds per access
}

int main(void) {
    // Pin the process to Physical Core 0 so thread stays on one set of L1/L2 caches
    SetThreadAffinityMask(GetCurrentThread(), 1);

    printf("===============================================================\n");
    printf(" Windows Cache Latency Benchmark (Intel i5-1135G7)\n");
    printf("===============================================================\n");
    printf("  Working Set Size  |  Access Latency  | Expected Cache Tier  \n");
    printf("---------------------------------------------------------------\n");

    // Sizes to test (in KB)
    size_t test_sizes_kb[] = {
        16, 32, 64,             // Fits in 96KB L1 Data Cache
        128, 256, 512, 1024,   // Fits in 1.25MB L2 Cache
        2048, 4096, 6144,      // Fits in 8MB Shared L3 Cache
        16384, 32768           // Exceeds L3 -> Main RAM (DDR4/LPDDR4x)
    };

    size_t num_tests = sizeof(test_sizes_kb) / sizeof(test_sizes_kb[0]);
    size_t total_accesses = 50000000; // 50 Million hops

    for (size_t i = 0; i < num_tests; i++) {
        size_t size_kb = test_sizes_kb[i];
        size_t size_bytes = size_kb * 1024;

        double latency = measure_latency_ns(size_bytes, total_accesses);

        const char* tier = "L1 Cache";
        if (size_kb > 96 && size_kb <= 1280) tier = "L2 Cache";
        else if (size_kb > 1280 && size_kb <= 8192) tier = "L3 Cache";
        else if (size_kb > 8192) tier = "Main RAM";

        printf("  %6zu KB          |   %6.2f ns     | %s\n", size_kb, latency, tier);
    }

    printf("===============================================================\n");
    return 0;
}