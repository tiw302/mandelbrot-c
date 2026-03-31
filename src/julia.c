#include "julia.h"
#include <math.h>

double julia_check(complex_t z, complex_t c) {
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < MAX_ITERATIONS) {
        // z = z^2 + c
        double next_re = z.re * z.re - z.im * z.im + c.re;
        double next_im = 2 * z.re * z.im + c.im;
        z.re = next_re;
        z.im = next_im;

        double mag_sq = z.re * z.re + z.im * z.im;
        if (mag_sq > escape_radius_sq) {
            // smooth coloring math
            return (double)iterations + 2.0 - log2(log(mag_sq));
        }

        iterations++;
    }

    return (double)MAX_ITERATIONS;
}

#ifdef __AVX2__
void julia_check_avx2(const double *re, const double *im, complex_t c, double *results) {
    __m256d cre = _mm256_set1_pd(c.re);
    __m256d cim = _mm256_set1_pd(c.im);
    __m256d zre = _mm256_loadu_pd(re);
    __m256d zim = _mm256_loadu_pd(im);

    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = _mm256_setzero_pd();
    __m256d final_mag_sq = _mm256_setzero_pd();

    for (int i = 0; i < MAX_ITERATIONS; i++) {
        __m256d zre2 = _mm256_mul_pd(zre, zre);
        __m256d zim2 = _mm256_mul_pd(zim, zim);
        __m256d mag_sq = _mm256_add_pd(zre2, zim2);

        __m256d mask = _mm256_cmp_pd(mag_sq, esc_radius_sq, _CMP_GT_OQ);
        __m256d just_escaped = _mm256_andnot_pd(escaped_mask, mask);

        final_mag_sq = _mm256_or_pd(final_mag_sq, _mm256_and_pd(just_escaped, mag_sq));
        escaped_mask = _mm256_or_pd(escaped_mask, mask);

        if (_mm256_movemask_pd(escaped_mask) == 0xF) break;

        __m256d next_re = _mm256_add_pd(_mm256_sub_pd(zre2, zim2), cre);
        __m256d next_im = _mm256_add_pd(_mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_mul_pd(zre, zim)), cim);
        zre = next_re;
        zim = next_im;

        iters = _mm256_add_pd(iters, _mm256_andnot_pd(escaped_mask, _mm256_set1_pd(1.0)));
    }

    double res_iters[4], res_mag_sq[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 4; i++) {
        if (res_iters[i] >= MAX_ITERATIONS - 1) {
            results[i] = (double)MAX_ITERATIONS;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(res_mag_sq[i]));
        }
    }
}
#endif
