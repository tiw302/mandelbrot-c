#include "fractal.h"
#include <string.h>
#include <stdio.h>

// centralized registry for all fractal math kernels
// ensures pluggable registration of new fractals

#define MAX_REGISTERED_FRACTALS 16
static FractalDefinition registry[MAX_REGISTERED_FRACTALS];
static int registry_count = 0;
static int is_initialized = 0;

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
    if (!is_initialized) {
        init_fractal_registry();
    }
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
static void mandelbrot_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    mandelbrot_check_avx2(cre, cim, max_iterations, results);
}
#endif

#ifdef __AVX512F__
static void mandelbrot_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    mandelbrot_check_avx512(cre, cim, max_iterations, results);
}
#endif

#ifdef __wasm_simd128__
static void mandelbrot_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    mandelbrot_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif

#ifdef USE_SIMD_F128
static double mandelbrot_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre, simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre;
    (void)julia_cim;
    return mandelbrot_check_f128(cre, cim, max_iterations);
}

#ifdef __AVX2__
static void mandelbrot_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre, simd_f128x4 julia_cim, int max_iterations, double* results) {
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
static void julia_avx2_wrap(__m256d zre, __m256d zim, complex_t julia_c, int max_iterations, double* results) {
    julia_check_avx2(zre, zim, julia_c, max_iterations, results);
}
#endif

#ifdef __AVX512F__
static void julia_avx512_wrap(__m512d zre, __m512d zim, complex_t julia_c, int max_iterations, double* results) {
    julia_check_avx512(zre, zim, julia_c, max_iterations, results);
}
#endif

#ifdef __wasm_simd128__
static void julia_wasm_wrap(v128_t zre, v128_t zim, complex_t julia_c, int max_iterations, double* results) {
    julia_check_wasm_simd128(zre, zim, julia_c, max_iterations, results);
}
#endif

#ifdef USE_SIMD_F128
static double julia_f128_wrap(simd_f128 zre, simd_f128 zim, simd_f128 julia_cre, simd_f128 julia_cim, int max_iterations) {
    return julia_check_f128(zre, zim, julia_cre, julia_cim, max_iterations);
}

#ifdef __AVX2__
static void julia_f128x4_wrap(simd_f128x4 zre, simd_f128x4 zim, simd_f128x4 julia_cre, simd_f128x4 julia_cim, int max_iterations, double* results) {
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
static void burning_ship_avx2_wrap(__m256d cre, __m256d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    burning_ship_check_avx2(cre, cim, max_iterations, results);
}
#endif

#ifdef __AVX512F__
static void burning_ship_avx512_wrap(__m512d cre, __m512d cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    burning_ship_check_avx512(cre, cim, max_iterations, results);
}
#endif

#ifdef __wasm_simd128__
static void burning_ship_wasm_wrap(v128_t cre, v128_t cim, complex_t julia_c, int max_iterations, double* results) {
    (void)julia_c;
    burning_ship_check_wasm_simd128(cre, cim, max_iterations, results);
}
#endif

#ifdef USE_SIMD_F128
static double burning_ship_f128_wrap(simd_f128 cre, simd_f128 cim, simd_f128 julia_cre, simd_f128 julia_cim, int max_iterations) {
    (void)julia_cre;
    (void)julia_cim;
    return burning_ship_check_f128(cre, cim, max_iterations);
}

#ifdef __AVX2__
static void burning_ship_f128x4_wrap(simd_f128x4 cre, simd_f128x4 cim, simd_f128x4 julia_cre, simd_f128x4 julia_cim, int max_iterations, double* results) {
    (void)julia_cre;
    (void)julia_cim;
    burning_ship_check_f128x4(cre, cim, max_iterations, results);
}
#endif
#endif

// registers the standard default fractals into the registry
void init_fractal_registry(void) {
    if (is_initialized) return;

    // 1. mandelbrot
    FractalDefinition mandelbrot_def = {
        .mode = RENDER_MANDELBROT,
        .name = "mandelbrot",
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
#ifdef USE_SIMD_F128
        .check_scalar_f128 = burning_ship_f128_wrap,
#ifdef __AVX2__
        .check_avx2_f128 = burning_ship_f128x4_wrap,
#endif
#endif
    };
    register_fractal(&burning_ship_def);

    is_initialized = 1;
}
