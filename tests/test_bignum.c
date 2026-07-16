/* test_bignum.c
 *
 * unit tests for the arbitrary-precision BigNum engine.
 * verifies arithmetic correctness, round-trip double conversion,
 * and mandelbrot iteration consistency against the standard double path.
 */

#include "bignum.h"
#include "mandelbrot_bignum.h"
#include "mandelbrot.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// tolerance for comparing bignum results to double-precision reference
#define BN_DOUBLE_TOLERANCE 1e-8

/*
 * [TEST CASE] bn_from_double / bn_to_double round-trip
 * verifies that converting to BigNum and back preserves value within double precision.
 */
void test_round_trip(void) {
    double test_vals[] = { 0.0, 1.0, -1.0, 0.5, -0.5, 1.23456789, -2.71828182, 2.99999 };
    int count = (int)(sizeof(test_vals) / sizeof(test_vals[0]));

    for (int i = 0; i < count; i++) {
        BigNum bn;
        bn_from_double(&bn, test_vals[i]);
        double result = bn_to_double(&bn);
        double diff = fabs(result - test_vals[i]);
        assert(diff < BN_DOUBLE_TOLERANCE);
    }

    printf("test_round_trip passed!\n");
}

/*
 * [TEST CASE] bn_add correctness
 * verifies addition with same signs, opposite signs, and near-cancel cases.
 */
void test_add(void) {
    BigNum a, b, out;

    // 1.5 + 0.5 = 2.0
    bn_from_double(&a, 1.5);
    bn_from_double(&b, 0.5);
    bn_add(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - 2.0) < BN_DOUBLE_TOLERANCE);

    // 1.0 + (-0.25) = 0.75
    bn_from_double(&a, 1.0);
    bn_from_double(&b, -0.25);
    bn_add(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - 0.75) < BN_DOUBLE_TOLERANCE);

    // -1.0 + (-1.0) = -2.0
    bn_from_double(&a, -1.0);
    bn_from_double(&b, -1.0);
    bn_add(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - (-2.0)) < BN_DOUBLE_TOLERANCE);

    // cancel: 1.0 + (-1.0) = 0.0
    bn_from_double(&a, 1.0);
    bn_from_double(&b, -1.0);
    bn_add(&out, &a, &b);
    assert(fabs(bn_to_double(&out)) < BN_DOUBLE_TOLERANCE);

    printf("test_add passed!\n");
}

/*
 * [TEST CASE] bn_sub correctness
 */
void test_sub(void) {
    BigNum a, b, out;

    // 2.0 - 0.75 = 1.25
    bn_from_double(&a, 2.0);
    bn_from_double(&b, 0.75);
    bn_sub(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - 1.25) < BN_DOUBLE_TOLERANCE);

    // 0.5 - 1.5 = -1.0
    bn_from_double(&a, 0.5);
    bn_from_double(&b, 1.5);
    bn_sub(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - (-1.0)) < BN_DOUBLE_TOLERANCE);

    printf("test_sub passed!\n");
}

/*
 * [TEST CASE] bn_mul correctness
 * verifies multiplication against known exact values.
 */
void test_mul(void) {
    BigNum a, b, out;

    // 0.5 * 0.5 = 0.25
    bn_from_double(&a, 0.5);
    bn_from_double(&b, 0.5);
    bn_mul(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - 0.25) < BN_DOUBLE_TOLERANCE);

    // 1.5 * 2.0 = 3.0
    bn_from_double(&a, 1.5);
    bn_from_double(&b, 2.0);
    bn_mul(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - 3.0) < BN_DOUBLE_TOLERANCE);

    // -0.7 * 0.3 = -0.21
    bn_from_double(&a, -0.7);
    bn_from_double(&b, 0.3);
    bn_mul(&out, &a, &b);
    assert(fabs(bn_to_double(&out) - (-0.21)) < BN_DOUBLE_TOLERANCE);

    printf("test_mul passed!\n");
}

/*
 * [TEST CASE] bn_mul2 (shift-by-1 optimization)
 */
void test_mul2(void) {
    BigNum a, out;

    // 2 * 1.25 = 2.5
    bn_from_double(&a, 1.25);
    bn_mul2(&out, &a);
    assert(fabs(bn_to_double(&out) - 2.5) < BN_DOUBLE_TOLERANCE);

    // 2 * (-0.5) = -1.0
    bn_from_double(&a, -0.5);
    bn_mul2(&out, &a);
    assert(fabs(bn_to_double(&out) - (-1.0)) < BN_DOUBLE_TOLERANCE);

    printf("test_mul2 passed!\n");
}

/*
 * [TEST CASE] mandelbrot consistency
 * compares mandelbrot_check_bignum() against mandelbrot_check() (double scalar).
 * result should be within a small tolerance for standard-precision coordinates.
 *
 * test points chosen to avoid borderline cases (points deep inside or right on boundary
 * may diverge between precision levels due to smooth coloring sensitivity).
 */
void test_mandelbrot_consistency(void) {
    // list of test coordinates and expected interior/exterior behavior
    struct { double re; double im; int expect_inside; } cases[] = {
        { 0.0,   0.0,   1 },  // origin — inside (period-1 fixed point in main cardioid)
        { 0.5,   0.5,   0 },  // clearly outside
        { -0.4,  0.6,   0 },  // outside
        { -1.5,  0.0,   1 },  // inside (period-3 bulb on the negative real axis)
        { 2.0,   0.0,   0 },  // outside (far right, escapes on first iteration)
    };
    int count = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < count; i++) {
        BigNum c_re, c_im;
        bn_from_double(&c_re, cases[i].re);
        bn_from_double(&c_im, cases[i].im);

        double bn_result = mandelbrot_check_bignum(&c_re, &c_im, 1000);
        double std_result = mandelbrot_check((complex_t){ cases[i].re, cases[i].im }, 1000);

        int bn_inside = (bn_result >= 999.0);
        int std_inside = (std_result >= 999.0);

        // both should agree on inside/outside classification
        assert(bn_inside == cases[i].expect_inside);
        assert(std_inside == cases[i].expect_inside);
        assert(bn_inside == std_inside);
    }

    printf("test_mandelbrot_consistency passed!\n");
}

/*
 * [TEST CASE] reference orbit
 * checks that mandelbrot_bignum_orbit() produces orbits consistent with
 * the standard double reference for normal-precision coordinates.
 */
void test_reference_orbit(void) {
    BigNum c_re, c_im;
    // c = (0.5, 0.5) — known orbit: z1 = (0.5, 0.5), z2 = (0.5, 1.0)
    bn_from_double(&c_re, 0.5);
    bn_from_double(&c_im, 0.5);

    int max_iter = 10;
    double* orbit_re = malloc(sizeof(double) * max_iter);
    double* orbit_im = malloc(sizeof(double) * max_iter);
    assert(orbit_re && orbit_im);

    int len = mandelbrot_bignum_orbit(&c_re, &c_im, max_iter, orbit_re, orbit_im);
    assert(len >= 3);

    // z_0 = (0, 0)
    assert(fabs(orbit_re[0]) < BN_DOUBLE_TOLERANCE);
    assert(fabs(orbit_im[0]) < BN_DOUBLE_TOLERANCE);

    // z_1 = c = (0.5, 0.5)
    assert(fabs(orbit_re[1] - 0.5) < BN_DOUBLE_TOLERANCE);
    assert(fabs(orbit_im[1] - 0.5) < BN_DOUBLE_TOLERANCE);

    // z_2 = z_1^2 + c = (0.5+0.5i)^2 + (0.5+0.5i) = (0+0.5i) + (0.5+0.5i) = (0.5, 1.0)
    assert(fabs(orbit_re[2] - 0.5) < BN_DOUBLE_TOLERANCE);
    assert(fabs(orbit_im[2] - 1.0) < BN_DOUBLE_TOLERANCE);

    free(orbit_re);
    free(orbit_im);
    printf("test_reference_orbit passed!\n");
}

int main(void) {
    printf("running bignum tests...\n");
    test_round_trip();
    test_add();
    test_sub();
    test_mul();
    test_mul2();
    test_mandelbrot_consistency();
    test_reference_orbit();
    printf("all bignum tests passed successfully!\n");
    return 0;
}
