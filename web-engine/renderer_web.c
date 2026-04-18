#include "renderer_web.h"
#include "color.h"

void render_mandelbrot_wasm(uint32_t *pixels, int pitch,
                            int window_width, int window_height,
                            double re_min, double re_max,
                            double im_min, double im_max,
                            int max_iterations) {
    double re_factor = (window_width > 0) ? (re_max - re_min) / window_width : 0;
    double im_factor = (window_height > 0) ? (im_max - im_min) / window_height : 0;

    for (int y = 0; y < window_height; y++) {
        int x = 0;
#if defined(__wasm_simd128__)
        for (; x <= window_width - 2; x += 2) {
            double res_re[2], res_im[2], iterations[2];
            res_re[0] = re_min + (double)x * re_factor;
            res_re[1] = re_min + (double)(x + 1) * re_factor;
            res_im[0] = res_im[1] = im_min + (double)y * im_factor;

            mandelbrot_check_wasm_simd128(res_re, res_im, max_iterations, iterations);

            for (int i = 0; i < 2; i++) {
                uint8_t r, g, b;
                get_color(iterations[i], max_iterations, &r, &g, &b);
                pixels[y * (pitch / sizeof(uint32_t)) + (x + i)] =
                    (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
#endif
        for (; x < window_width; x++) {
            complex_t point;
            point.re = re_min + (double)x * re_factor;
            point.im = im_min + (double)y * im_factor;

            double iters = mandelbrot_check(point, max_iterations);
            uint8_t r, g, b;
            get_color(iters, max_iterations, &r, &g, &b);
            pixels[y * (pitch / sizeof(uint32_t)) + x] =
                (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void render_julia_wasm(uint32_t *pixels, int pitch,
                       int window_width, int window_height,
                       double re_min, double re_max,
                       double im_min, double im_max,
                       complex_t julia_c,
                       int max_iterations) {
    double re_factor = (window_width > 0) ? (re_max - re_min) / window_width : 0;
    double im_factor = (window_height > 0) ? (im_max - im_min) / window_height : 0;

    for (int y = 0; y < window_height; y++) {
        int x = 0;
#if defined(__wasm_simd128__)
        for (; x <= window_width - 2; x += 2) {
            double res_re[2], res_im[2], iterations[2];
            res_re[0] = re_min + (double)x * re_factor;
            res_re[1] = re_min + (double)(x + 1) * re_factor;
            res_im[0] = res_im[1] = im_min + (double)y * im_factor;

            julia_check_wasm_simd128(res_re, res_im, julia_c, max_iterations, iterations);

            for (int i = 0; i < 2; i++) {
                uint8_t r, g, b;
                get_color(iterations[i], max_iterations, &r, &g, &b);
                pixels[y * (pitch / sizeof(uint32_t)) + (x + i)] =
                    (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
#endif
        for (; x < window_width; x++) {
            complex_t point;
            point.re = re_min + (double)x * re_factor;
            point.im = im_min + (double)y * im_factor;

            double iters = julia_check(point, julia_c, max_iterations);
            uint8_t r, g, b;
            get_color(iters, max_iterations, &r, &g, &b);
            pixels[y * (pitch / sizeof(uint32_t)) + x] =
                (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}
