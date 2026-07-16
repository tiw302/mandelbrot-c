/* fractal.c
 *
 * central registry and dispatch functions for base fractals.
 * connects render loops to correct mathematical kernels.
 */

#include "fractal.h"
#include "tricorn.h"
#include "celtic.h"
#include "buffalo.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

// centralized registry for all fractal math kernels
// ensures pluggable registration of new fractals

#define MAX_REGISTERED_FRACTALS 16
static FractalDefinition registry[MAX_REGISTERED_FRACTALS];
static int registry_count = 0;
static pthread_once_t registry_once = PTHREAD_ONCE_INIT;

// registers a new fractal structure in the central registry
void register_fractal(const FractalDefinition* def) {
    if (registry_count >= MAX_REGISTERED_FRACTALS) {
        fprintf(stderr, "warning: fractal registry is full\n");
        return;
    }
    registry[registry_count++] = *def;
}

// retrieves a registered fractal definition by render mode
const FractalDefinition* get_fractal_by_mode(RenderMode mode) {
    pthread_once(&registry_once, init_fractal_registry);
    for (int i = 0; i < registry_count; i++) {
        if (registry[i].mode == mode) {
            return &registry[i];
        }
    }
    return NULL;
}

// mandelbrot wrappers
static double mandelbrot_scalar_wrap(complex_t c, complex_t julia_c, int max_iterations) {
    (void)julia_c;
    return mandelbrot_check(c, max_iterations);
}

#ifdef __AVX2__
static void mandelbrot_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations,
                                 double* results) {
    (void)julia_c;
    mandelbrot_check_avx2(cre, cim, max_iterations, results);
}
#endif

#ifdef __AVX512F__
static void mandelbrot_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations,
                                   double* results) {
    (void)julia_c;
    mandelbrot_check_avx512(cre, cim, max_iterations, results);
}
#endif

#ifdef __wasm_simd128__
static void mandelbrot_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations,
                                 double* results) {
    (void)julia_c;
    mandelbrot_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif

#ifdef __ARM_NEON
static void mandelbrot_neon_wrap(float64x2_t cre, float64x2_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    mandelbrot_check_neon(cre, cim, max_iterations, results);
}
#endif

#ifdef USE_SIMD_F128
static double mandelbrot_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre,
                                   simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre;
    (void)julia_cim;
    return mandelbrot_check_f128(cre, cim, max_iterations);
}

#ifdef __AVX2__
static void mandelbrot_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre,
                                   simd_f128x4 julia_cim, int max_iterations, double* results) {
    (void)julia_cre;
    (void)julia_cim;
    mandelbrot_check_f128x4(cre, cim, max_iterations, results);
}
#endif
#endif

// julia wrappers
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
static void julia_neon_wrap(float64x2_t zre, float64x2_t zim, complex_t julia_c, int max_iterations, double* results) {
    julia_check_neon(zre, zim, vdupq_n_f64(julia_c.re), vdupq_n_f64(julia_c.im), max_iterations, results);
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

// burning ship wrappers
static double burning_ship_scalar_wrap(complex_t c, complex_t julia_c, int max_iterations) {
    (void)julia_c;
    return burning_ship_check(c, max_iterations);
}

#ifdef __AVX2__
static void burning_ship_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations,
                                   double* results) {
    (void)julia_c;
    burning_ship_check_avx2(cre, cim, max_iterations, results);
}
#endif

#ifdef __AVX512F__
static void burning_ship_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c,
                                     int max_iterations, double* results) {
    (void)julia_c;
    burning_ship_check_avx512(cre, cim, max_iterations, results);
}
#endif

#ifdef __wasm_simd128__
static void burning_ship_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations,
                                   double* results) {
    (void)julia_c;
    burning_ship_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif

#ifdef __ARM_NEON
static void burning_ship_neon_wrap(float64x2_t cre, float64x2_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    burning_ship_check_neon(cre, cim, max_iterations, results);
}
#endif

#ifdef USE_SIMD_F128
static double burning_ship_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre,
                                     simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre;
    (void)julia_cim;
    return burning_ship_check_f128(cre, cim, max_iterations);
}

#ifdef __AVX2__
static void burning_ship_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre,
                                     simd_f128x4 julia_cim, int max_iterations, double* results) {
    (void)julia_cre;
    (void)julia_cim;
    burning_ship_check_f128x4(cre, cim, max_iterations, results);
}
#endif
#endif

// registers the standard default fractals into the registry
// tricorn wrappers
static double tricorn_scalar_wrap(complex_t c, complex_t julia_c, int max_iterations) {
    (void)julia_c;
    return tricorn_check(c, max_iterations);
}
#ifdef __AVX2__
static void tricorn_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    tricorn_check_avx2(cre, cim, max_iterations, results);
}
#endif
#ifdef __AVX512F__
static void tricorn_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    tricorn_check_avx512(cre, cim, max_iterations, results);
}
#endif
#ifdef __wasm_simd128__
static void tricorn_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    tricorn_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif
#ifdef __ARM_NEON
static void tricorn_neon_wrap(float64x2_t cre, float64x2_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    tricorn_check_neon(cre, cim, max_iterations, results);
}
#endif
#ifdef USE_SIMD_F128
static double tricorn_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre, simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre; (void)julia_cim;
    return tricorn_check_f128(cre, cim, max_iterations);
}
#ifdef __AVX2__
static void tricorn_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre, simd_f128x4 julia_cim, int max_iterations, double* results) {
    (void)julia_cre; (void)julia_cim;
    tricorn_check_f128x4(cre, cim, max_iterations, results);
}
#endif
#endif

// celtic wrappers
static double celtic_scalar_wrap(complex_t c, complex_t julia_c, int max_iterations) {
    (void)julia_c;
    return celtic_check(c, max_iterations);
}
#ifdef __AVX2__
static void celtic_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    celtic_check_avx2(cre, cim, max_iterations, results);
}
#endif
#ifdef __AVX512F__
static void celtic_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    celtic_check_avx512(cre, cim, max_iterations, results);
}
#endif
#ifdef __wasm_simd128__
static void celtic_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    celtic_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif
#ifdef __ARM_NEON
static void celtic_neon_wrap(float64x2_t cre, float64x2_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    celtic_check_neon(cre, cim, max_iterations, results);
}
#endif
#ifdef USE_SIMD_F128
static double celtic_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre, simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre; (void)julia_cim;
    return celtic_check_f128(cre, cim, max_iterations);
}
#ifdef __AVX2__
static void celtic_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre, simd_f128x4 julia_cim, int max_iterations, double* results) {
    (void)julia_cre; (void)julia_cim;
    celtic_check_f128x4(cre, cim, max_iterations, results);
}
#endif
#endif

// buffalo wrappers
static double buffalo_scalar_wrap(complex_t c, complex_t julia_c, int max_iterations) {
    (void)julia_c;
    return buffalo_check(c, max_iterations);
}
#ifdef __AVX2__
static void buffalo_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    buffalo_check_avx2(cre, cim, max_iterations, results);
}
#endif
#ifdef __AVX512F__
static void buffalo_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    buffalo_check_avx512(cre, cim, max_iterations, results);
}
#endif
#ifdef __wasm_simd128__
static void buffalo_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    buffalo_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif
#ifdef __ARM_NEON
static void buffalo_neon_wrap(float64x2_t cre, float64x2_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    buffalo_check_neon(cre, cim, max_iterations, results);
}
#endif
#ifdef USE_SIMD_F128
static double buffalo_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre, simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre; (void)julia_cim;
    return buffalo_check_f128(cre, cim, max_iterations);
}
#ifdef __AVX2__
static void buffalo_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre, simd_f128x4 julia_cim, int max_iterations, double* results) {
    (void)julia_cre; (void)julia_cim;
    buffalo_check_f128x4(cre, cim, max_iterations, results);
}
#endif
#endif

void init_fractal_registry(void) {

    // 1. mandelbrot
    FractalDefinition mandelbrot_def = {
        .mode = RENDER_MANDELBROT,
        .name = "mandelbrot",
        .display_name = "Mandelbrot",
        .explorer_title = "Mandelbrot Explorer",
        .default_center_re = -0.5,
        .default_center_im = 0.0,
        .default_zoom = 3.0,
        .check_scalar = mandelbrot_scalar_wrap,
#ifdef __AVX2__
        .check_avx2 = mandelbrot_avx2_wrap,
#endif
#ifdef __AVX512F__
        .check_avx512 = mandelbrot_avx512_wrap,
#endif
#ifdef __wasm_simd128__
        .check_wasm_simd128 = mandelbrot_wasm_wrap,
#endif
#ifdef __ARM_NEON
        .check_neon = mandelbrot_neon_wrap,
#endif
#ifdef USE_SIMD_F128
        .check_scalar_f128 = mandelbrot_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = mandelbrot_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&mandelbrot_def);

    // 2. julia
    FractalDefinition julia_def = {
        .mode = RENDER_JULIA,
        .name = "julia",
        .display_name = "Julia",
        .explorer_title = "Julia Explorer",
        .default_center_re = 0.0,
        .default_center_im = 0.0,
        .default_zoom = 3.0,
        .check_scalar = julia_scalar_wrap,
#ifdef __AVX2__
        .check_avx2 = julia_avx2_wrap,
#endif
#ifdef __AVX512F__
        .check_avx512 = julia_avx512_wrap,
#endif
#ifdef __wasm_simd128__
        .check_wasm_simd128 = julia_wasm_wrap,
#endif
#ifdef __ARM_NEON
        .check_neon = julia_neon_wrap,
#endif
#ifdef USE_SIMD_F128
        .check_scalar_f128 = julia_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = julia_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&julia_def);

    // 3. burning ship
    FractalDefinition burning_ship_def = {
        .mode = RENDER_BURNING_SHIP,
        .name = "burning_ship",
        .display_name = "Burning Ship",
        .explorer_title = "Burning Ship Explorer",
        .default_center_re = -0.4,
        .default_center_im = -0.6,
        .default_zoom = 3.5,
        .check_scalar = burning_ship_scalar_wrap,
#ifdef __AVX2__
        .check_avx2 = burning_ship_avx2_wrap,
#endif
#ifdef __AVX512F__
        .check_avx512 = burning_ship_avx512_wrap,
#endif
#ifdef __wasm_simd128__
        .check_wasm_simd128 = burning_ship_wasm_wrap,
#endif
#ifdef __ARM_NEON
        .check_neon = burning_ship_neon_wrap,
#endif
#ifdef USE_SIMD_F128
        .check_scalar_f128 = burning_ship_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = burning_ship_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&burning_ship_def);


    // 4. tricorn
    FractalDefinition tricorn_def = {
        .mode = RENDER_TRICORN,
        .name = "tricorn",
        .display_name = "Tricorn",
        .explorer_title = "Tricorn Explorer",
        .default_center_re = -0.1,
        .default_center_im = 0.0,
        .default_zoom = 3.5,
        .check_scalar = tricorn_scalar_wrap,
#ifdef __AVX2__
        .check_avx2 = tricorn_avx2_wrap,
#endif
#ifdef __AVX512F__
        .check_avx512 = tricorn_avx512_wrap,
#endif
#ifdef __wasm_simd128__
        .check_wasm_simd128 = tricorn_wasm_wrap,
#endif
#ifdef __ARM_NEON
        .check_neon = tricorn_neon_wrap,
#endif
#ifdef USE_SIMD_F128
        .check_scalar_f128 = tricorn_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = tricorn_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&tricorn_def);

    // 5. celtic
    FractalDefinition celtic_def = {
        .mode = RENDER_CELTIC,
        .name = "celtic",
        .display_name = "Celtic",
        .explorer_title = "Celtic Explorer",
        .default_center_re = -0.5,
        .default_center_im = 0.0,
        .default_zoom = 3.5,
        .check_scalar = celtic_scalar_wrap,
#ifdef __AVX2__
        .check_avx2 = celtic_avx2_wrap,
#endif
#ifdef __AVX512F__
        .check_avx512 = celtic_avx512_wrap,
#endif
#ifdef __wasm_simd128__
        .check_wasm_simd128 = celtic_wasm_wrap,
#endif
#ifdef __ARM_NEON
        .check_neon = celtic_neon_wrap,
#endif
#ifdef USE_SIMD_F128
        .check_scalar_f128 = celtic_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = celtic_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&celtic_def);

    // 6. buffalo
    FractalDefinition buffalo_def = {
        .mode = RENDER_BUFFALO,
        .name = "buffalo",
        .display_name = "Buffalo",
        .explorer_title = "Buffalo Explorer",
        .default_center_re = -0.4,
        .default_center_im = 0.0,
        .default_zoom = 4.0,
        .check_scalar = buffalo_scalar_wrap,
#ifdef __AVX2__
        .check_avx2 = buffalo_avx2_wrap,
#endif
#ifdef __AVX512F__
        .check_avx512 = buffalo_avx512_wrap,
#endif
#ifdef __wasm_simd128__
        .check_wasm_simd128 = buffalo_wasm_wrap,
#endif
#ifdef __ARM_NEON
        .check_neon = buffalo_neon_wrap,
#endif
#ifdef USE_SIMD_F128
        .check_scalar_f128 = buffalo_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = buffalo_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&buffalo_def);

}
