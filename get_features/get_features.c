
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void get_cpu_vendor(char vendor[13])
{
    int regs[4];

    __cpuid(regs, 0);

    memcpy(vendor, &regs[1], 4);
    memcpy(vendor + 4, &regs[3], 4);
    memcpy(vendor + 8, &regs[2], 4);

    vendor[12] = '\0';
}

static void print_cpu_features(void)
{
    int regs[4];

    __cpuid(regs, 0);
    int max_leaf = regs[0];

    char vendor[13];
    get_cpu_vendor(vendor);

    printf("CPU vendor: %s\n", vendor);

    if (max_leaf < 1)
        return;

    __cpuidex(regs, 1, 0);

    unsigned int ecx = (unsigned int)regs[2];
    unsigned int edx = (unsigned int)regs[3];

    int osxsave = (ecx & (1u << 27)) != 0;
    int avx_hw = (ecx & (1u << 28)) != 0;

    uint64_t xcr0 = 0;

    if (osxsave)
        xcr0 = _xgetbv(0);

    int avx_os = (xcr0 & 0x6) == 0x6;
    int avx512_os = (xcr0 & 0xE6) == 0xE6;

    printf("SSE:    %s\n",
        (edx & (1u << 25)) ? "Available" : "Unavailable");

    printf("SSE2:   %s\n",
        (edx & (1u << 26)) ? "Available" : "Unavailable");

    printf("AVX:    %s\n",
        (avx_hw && osxsave && avx_os) ? "Available" : "Unavailable");

    if (max_leaf >= 7)
    {
        __cpuidex(regs, 7, 0);

        int avx512_hw =
            ((unsigned int)regs[1] & (1u << 16)) != 0;

        printf("AVX-512: %s\n",
            (avx512_hw && osxsave && avx512_os)
                ? "Available" : "Unavailable");
    }
}

static int pin_cpu(DWORD cpu)
{
    if (cpu >= sizeof(DWORD_PTR) * 8)
        return 0;

    DWORD_PTR mask = ((DWORD_PTR)1) << cpu;

    if (!SetThreadAffinityMask(GetCurrentThread(), mask))
    {
        printf("Affinity error: %lu\n", GetLastError());
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);

    printf("--- CPU INFORMATION ---\n");

    printf("Logical processors: %lu\n",
        (unsigned long)info.dwNumberOfProcessors);

    printf("Current processor: %lu\n",
        (unsigned long)GetCurrentProcessorNumber());

    print_cpu_features();

    if (argc == 3 && strcmp(argv[1], "-c") == 0)
    {
        char *end;
        unsigned long cpu = strtoul(argv[2], &end, 10);

        if (*end != '\0' || end == argv[2] ||
            !pin_cpu((DWORD)cpu))
        {
            printf("Could not pin thread.\n");
            return 1;
        }

        printf("Thread pinned to processor %lu\n", cpu);
    }

    return 0;
}

