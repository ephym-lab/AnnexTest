#include <stdio.h>
#include <stdlib.h>
#include <math.h>

long pthFactor(long n, long p) {
    long *small = malloc(64 * sizeof(long));
    long count = 0, cap = 64;

    long sq = (long)sqrt((double)n);

    for (long d = 1; d <= sq; d++) {
        if (n % d != 0) continue;
        if (count == cap) {
            cap *= 2;
            small = realloc(small, cap * sizeof(long));
        }
        small[count++] = d;
    }

    /* factors pair as (d, n/d); perfect squares have one unpaired middle */
    int perfect = (sq * sq == n);
    long total = 2 * count - perfect;

    if (p > total) {
        free(small);
        return 0;
    }

    long result;
    if (p <= count) {
        result = small[p - 1];
    } else {
        long idx = p - count - 1;
        result = n / small[count - 1 - perfect - idx];
    }

    free(small);
    return result;
}

int main(void) {
    printf("%ld\n", pthFactor(10, 3));  // 5
    printf("%ld\n", pthFactor(10, 5));  // 0
    printf("%ld\n", pthFactor(36, 5));  // 6
    return 0;
}
