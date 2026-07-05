#ifndef FRACTAL_H
#define FRACTAL_H

#include "core_math.h"

// holds dynamic fractal iteration kernels for pluggable dispatch.
// contains both standard 64-bit and high-precision 128-bit function pointers.
typedef struct {
    RenderMode mode;
    const char* name;
    const char* display_name;
    const char* explorer_title;
    double default_center_re;
    double default_center_im;
    double default_zoom;

    // standard 64-bit precision kernels
    double (*check_scalar)(complex_t c, complex_t julia_c, int max_iterations);
#ifdef __AVX2__
    void (*check_avx2)(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations,
                       double* results);
#endif
#ifdef __AVX512F__
    void (*check_avx512)(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations,
                         double* results);
#endif
#ifdef __wasm_simd128__
    void (*check_wasm_simd128)(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations,
                               double* results);
#endif
#ifdef __ARM_NEON
    void (*check_neon)(float64x2_t cre, float64x2_t cim, complex_t julia_c, int max_iterations,
                       double* results);
#endif

    // high-precision 128-bit kernels
#ifdef USE_SIMD_F128
    double (*check_scalar_f128)(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre,
                                simd_f128 julia_cim, int max_iterations);
#ifdef __AVX2__
    void (*check_avx2_f128)(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre,
                            simd_f128x4 julia_cim, int max_iterations, double* results);
#endif
#endif
} FractalDefinition;

// dynamic registry interface functions
void register_fractal(const FractalDefinition* def);
const FractalDefinition* get_fractal_by_mode(RenderMode mode);
void init_fractal_registry(void);

#endif
