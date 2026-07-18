/* fractal.c
 *
 * central registry and dispatch functions for base fractals.
 * connects render loops to correct mathematical kernels.
 */

#include "fractal.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffalo.h"
#include "celtic.h"
#include "tricorn.h"

#define MAX_REGISTERED_FRACTALS 16
static FractalDefinition registry[MAX_REGISTERED_FRACTALS];
static int registry_count = 0;
static pthread_once_t registry_once = PTHREAD_ONCE_INIT;

void register_fractal(const FractalDefinition* def) {
    if (registry_count >= MAX_REGISTERED_FRACTALS) {
        fprintf(stderr, "warning: fractal registry is full\n");
        return;
    }
    registry[registry_count++] = *def;
}

const FractalDefinition* get_fractal_by_mode(RenderMode mode) {
    pthread_once(&registry_once, init_fractal_registry);
    for (int i = 0; i < registry_count; i++) {
        if (registry[i].mode == mode) {
            return &registry[i];
        }
    }
    return NULL;
}

#define DEF_SCALAR_NJ(name, fn)                                               \
    static double name##_scalar_wrap(complex_t c, complex_t julia_c, int n) { \
        (void)julia_c;                                                        \
        return fn(c, n);                                                      \
    }

#ifdef __AVX2__
#define DEF_AVX2_NJ(name, fn)                                                             \
    static void name##_avx2_wrap(__m256d re, __m256d im, complex_t j, int n, double* r) { \
        (void)j;                                                                          \
        fn(re, im, n, r);                                                                 \
    }
#else
#define DEF_AVX2_NJ(name, fn)
#endif

#ifdef __AVX512F__
#define DEF_AVX512_NJ(name, fn)                                                             \
    static void name##_avx512_wrap(__m512d re, __m512d im, complex_t j, int n, double* r) { \
        (void)j;                                                                            \
        fn(re, im, n, r);                                                                   \
    }
#else
#define DEF_AVX512_NJ(name, fn)
#endif

#ifdef __wasm_simd128__
#define DEF_WASM_NJ(name, fn)                                                           \
    static void name##_wasm_wrap(v128_t re, v128_t im, complex_t j, int n, double* r) { \
        (void)j;                                                                        \
        fn(re, im, n, r);                                                               \
    }
#else
#define DEF_WASM_NJ(name, fn)
#endif

#ifdef __ARM_NEON
#define DEF_NEON_NJ(name, fn)                                                                     \
    static void name##_neon_wrap(float64x2_t re, float64x2_t im, complex_t j, int n, double* r) { \
        (void)j;                                                                                  \
        fn(re, im, n, r);                                                                         \
    }
#else
#define DEF_NEON_NJ(name, fn)
#endif

#ifdef USE_SIMD_F128
#define DEF_F128_NJ(name, fn)                                                                \
    static double name##_f128_wrap(simd_f128 re, simd_f128 im, simd_f128 jre, simd_f128 jim, \
                                   int n) {                                                  \
        (void)jre;                                                                           \
        (void)jim;                                                                           \
        return fn(re, im, n);                                                                \
    }
#ifdef __AVX2__
#define DEF_F128X4_NJ(name, fn)                                                     \
    static void name##_f128x4_wrap(simd_f128x4 re, simd_f128x4 im, simd_f128x4 jre, \
                                   simd_f128x4 jim, int n, double* r) {             \
        (void)jre;                                                                  \
        (void)jim;                                                                  \
        fn(re, im, n, r);                                                           \
    }
#else
#define DEF_F128X4_NJ(name, fn)
#endif
#else
#define DEF_F128_NJ(name, fn)
#define DEF_F128X4_NJ(name, fn)
#endif

#define DEF_NJ_WRAPPERS(name, sc, a2, a5, ws, ne, f1, f1x4) \
    DEF_SCALAR_NJ(name, sc)                                 \
    DEF_AVX2_NJ(name, a2)                                   \
    DEF_AVX512_NJ(name, a5)                                 \
    DEF_WASM_NJ(name, ws)                                   \
    DEF_NEON_NJ(name, ne)                                   \
    DEF_F128_NJ(name, f1)                                   \
    DEF_F128X4_NJ(name, f1x4)

DEF_NJ_WRAPPERS(mandelbrot, mandelbrot_check, mandelbrot_check_avx2, mandelbrot_check_avx512,
                mandelbrot_check_wasm_simd128, mandelbrot_check_neon, mandelbrot_check_f128,
                mandelbrot_check_f128x4)

DEF_NJ_WRAPPERS(burning_ship, burning_ship_check, burning_ship_check_avx2,
                burning_ship_check_avx512, burning_ship_check_wasm_simd128, burning_ship_check_neon,
                burning_ship_check_f128, burning_ship_check_f128x4)

DEF_NJ_WRAPPERS(tricorn, tricorn_check, tricorn_check_avx2, tricorn_check_avx512,
                tricorn_check_wasm_simd128, tricorn_check_neon, tricorn_check_f128,
                tricorn_check_f128x4)

DEF_NJ_WRAPPERS(celtic, celtic_check, celtic_check_avx2, celtic_check_avx512,
                celtic_check_wasm_simd128, celtic_check_neon, celtic_check_f128,
                celtic_check_f128x4)

DEF_NJ_WRAPPERS(buffalo, buffalo_check, buffalo_check_avx2, buffalo_check_avx512,
                buffalo_check_wasm_simd128, buffalo_check_neon, buffalo_check_f128,
                buffalo_check_f128x4)

/* --- julia wrappers (julia_c is passed through, not ignored) --- */

static double julia_scalar_wrap(complex_t z, complex_t julia_c, int max_iterations) {
    return julia_check(z, julia_c, max_iterations);
}

#ifdef __AVX2__
static void julia_avx2_wrap(__m256d zre, __m256d zim, complex_t julia_c, int max_iterations,
                            double* results) {
    julia_check_avx2(zre, zim, julia_c, max_iterations, results);
}
#endif

#ifdef __AVX512F__
static void julia_avx512_wrap(__m512d zre, __m512d zim, complex_t julia_c, int max_iterations,
                              double* results) {
    julia_check_avx512(zre, zim, julia_c, max_iterations, results);
}
#endif

#ifdef __wasm_simd128__
static void julia_wasm_wrap(v128_t zre, v128_t zim, complex_t julia_c, int max_iterations,
                            double* results) {
    julia_check_wasm_simd128(zre, zim, julia_c, max_iterations, results);
}
#endif

#ifdef __ARM_NEON
static void julia_neon_wrap(float64x2_t zre, float64x2_t zim, complex_t julia_c, int max_iterations,
                            double* results) {
    julia_check_neon(zre, zim, vdupq_n_f64(julia_c.re), vdupq_n_f64(julia_c.im), max_iterations,
                     results);
}
#endif

#ifdef USE_SIMD_F128
static double julia_f128_wrap(simd_f128 zre, simd_f128 zim, simd_f128 julia_cre,
                              simd_f128 julia_cim, int max_iterations) {
    return julia_check_f128(zre, zim, julia_cre, julia_cim, max_iterations);
}

#ifdef __AVX2__
static void julia_f128x4_wrap(simd_f128x4 zre, simd_f128x4 zim, simd_f128x4 julia_cre,
                              simd_f128x4 julia_cim, int max_iterations, double* results) {
    julia_check_f128x4(zre, zim, julia_cre, julia_cim, max_iterations, results);
}
#endif
#endif

/*
 * helper macro to fill in SIMD function pointer fields only when the
 * corresponding feature is compiled in.
 */
#ifdef __AVX2__
#define FD_AVX2(name) .check_avx2 = name##_avx2_wrap,
#else
#define FD_AVX2(name)
#endif
#ifdef __AVX512F__
#define FD_AVX512(name) .check_avx512 = name##_avx512_wrap,
#else
#define FD_AVX512(name)
#endif
#ifdef __wasm_simd128__
#define FD_WASM(name) .check_wasm_simd128 = name##_wasm_wrap,
#else
#define FD_WASM(name)
#endif
#ifdef __ARM_NEON
#define FD_NEON(name) .check_neon = name##_neon_wrap,
#else
#define FD_NEON(name)
#endif
#ifdef USE_SIMD_F128
#define FD_F128(name) .check_scalar_f128 = name##_f128_wrap,
#ifdef __AVX2__
#define FD_F128X4(name) .check_avx2_f128 = name##_f128x4_wrap,
#else
#define FD_F128X4(name)
#endif
#else
#define FD_F128(name)
#define FD_F128X4(name)
#endif

void init_fractal_registry(void) {
    // 1. mandelbrot
    FractalDefinition mandelbrot_def = {.mode = RENDER_MANDELBROT,
                                        .name = "mandelbrot",
                                        .display_name = "Mandelbrot",
                                        .explorer_title = "Mandelbrot Explorer",
                                        .default_center_re = -0.5,
                                        .default_center_im = 0.0,
                                        .default_zoom = 3.0,
                                        .check_scalar = mandelbrot_scalar_wrap,
                                        FD_AVX2(mandelbrot) FD_AVX512(mandelbrot)
                                            FD_WASM(mandelbrot) FD_NEON(mandelbrot)
                                                FD_F128(mandelbrot) FD_F128X4(mandelbrot)};
    register_fractal(&mandelbrot_def);

    // 2. julia
    FractalDefinition julia_def = {.mode = RENDER_JULIA,
                                   .name = "julia",
                                   .display_name = "Julia",
                                   .explorer_title = "Julia Explorer",
                                   .default_center_re = 0.0,
                                   .default_center_im = 0.0,
                                   .default_zoom = 3.0,
                                   .check_scalar = julia_scalar_wrap,
                                   FD_AVX2(julia) FD_AVX512(julia) FD_WASM(julia) FD_NEON(julia)
                                       FD_F128(julia) FD_F128X4(julia)};
    register_fractal(&julia_def);

    // 3. burning ship
    FractalDefinition burning_ship_def = {.mode = RENDER_BURNING_SHIP,
                                          .name = "burning_ship",
                                          .display_name = "Burning Ship",
                                          .explorer_title = "Burning Ship Explorer",
                                          .default_center_re = -0.4,
                                          .default_center_im = -0.6,
                                          .default_zoom = 3.5,
                                          .check_scalar = burning_ship_scalar_wrap,
                                          FD_AVX2(burning_ship) FD_AVX512(burning_ship)
                                              FD_WASM(burning_ship) FD_NEON(burning_ship)
                                                  FD_F128(burning_ship) FD_F128X4(burning_ship)};
    register_fractal(&burning_ship_def);

    // 4. tricorn
    FractalDefinition tricorn_def = {.mode = RENDER_TRICORN,
                                     .name = "tricorn",
                                     .display_name = "Tricorn",
                                     .explorer_title = "Tricorn Explorer",
                                     .default_center_re = -0.1,
                                     .default_center_im = 0.0,
                                     .default_zoom = 3.5,
                                     .check_scalar = tricorn_scalar_wrap,
                                     FD_AVX2(tricorn) FD_AVX512(tricorn) FD_WASM(tricorn)
                                         FD_NEON(tricorn) FD_F128(tricorn) FD_F128X4(tricorn)};
    register_fractal(&tricorn_def);

    // 5. celtic
    FractalDefinition celtic_def = {.mode = RENDER_CELTIC,
                                    .name = "celtic",
                                    .display_name = "Celtic",
                                    .explorer_title = "Celtic Explorer",
                                    .default_center_re = -0.5,
                                    .default_center_im = 0.0,
                                    .default_zoom = 3.5,
                                    .check_scalar = celtic_scalar_wrap,
                                    FD_AVX2(celtic) FD_AVX512(celtic) FD_WASM(celtic)
                                        FD_NEON(celtic) FD_F128(celtic) FD_F128X4(celtic)};
    register_fractal(&celtic_def);

    // 6. buffalo
    FractalDefinition buffalo_def = {.mode = RENDER_BUFFALO,
                                     .name = "buffalo",
                                     .display_name = "Buffalo",
                                     .explorer_title = "Buffalo Explorer",
                                     .default_center_re = -0.4,
                                     .default_center_im = 0.0,
                                     .default_zoom = 4.0,
                                     .check_scalar = buffalo_scalar_wrap,
                                     FD_AVX2(buffalo) FD_AVX512(buffalo) FD_WASM(buffalo)
                                         FD_NEON(buffalo) FD_F128(buffalo) FD_F128X4(buffalo)};
    register_fractal(&buffalo_def);
}

int get_fractal_registry_count(void) {
    pthread_once(&registry_once, init_fractal_registry);
    return registry_count;
}

const FractalDefinition* get_fractal_by_index(int idx) {
    pthread_once(&registry_once, init_fractal_registry);
    if (idx >= 0 && idx < registry_count) {
        return &registry[idx];
    }
    return NULL;
}
