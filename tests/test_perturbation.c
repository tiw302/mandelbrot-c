/* test_perturbation.c
 *
 * simple command-line test suite for perturbation logic.
 * verifies orbit calculations, escape detection, and basic state transitions.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "perturbation.h"

/*
 * [TEST CASE] zero orbit
 * tests the functionality of zero orbit.
 */
void test_zero_orbit(void) {
    // center at (0.0, 0.0) should not escape, length should equal max_iter
    int max_iter = 100;
    RefOrbit* orb = perturbation_compute(0.0, 0.0, max_iter);
    assert(orb != NULL);
    assert(orb->len == max_iter);

    // Z0 should be 0.0
    assert(orb->zn[0].re == 0.0f);
    assert(orb->zn[0].im == 0.0f);

    // Z1 should be C = 0.0
    assert(orb->zn[1].re == 0.0f);
    assert(orb->zn[1].im == 0.0f);

    perturbation_free(orb);
    printf("test_zero_orbit passed!\n");
}

/*
 * [TEST CASE] escape orbit
 * tests the functionality of escape orbit.
 */
void test_escape_orbit(void) {
    // center at (11.0, 0.0) should escape on the second step check (i=2)
    int max_iter = 100;
    RefOrbit* orb = perturbation_compute(11.0, 0.0, max_iter);
    assert(orb != NULL);
    assert(orb->len == 2);
    assert(orb->zn[0].re == 0.0f);
    assert(orb->zn[1].re == 11.0f);

    perturbation_free(orb);
    printf("test_escape_orbit passed!\n");
}

/*
 * [TEST CASE] manual orbit
 * tests the functionality of manual orbit.
 */
void test_manual_orbit(void) {
    /* C = (0.5, 0.5)
     * Z0 = (0, 0)
     * Z1 = C = (0.5, 0.5)
     * Z2 = Z1^2 + C = (0.5+0.5i)^2 + (0.5+0.5i) = (0.25 - 0.25 + 0.5i) + (0.5+0.5i) = (0.5, 1.0) */
    int max_iter = 5;
    RefOrbit* orb = perturbation_compute(0.5, 0.5, max_iter);
    assert(orb != NULL);
    assert(orb->len >= 3);

    assert(orb->zn[0].re == 0.5f * 0.0f && orb->zn[0].im == 0.0f);
    assert(orb->zn[1].re == 0.5f && orb->zn[1].im == 0.5f);
    assert(orb->zn[2].re == 0.5f && orb->zn[2].im == 1.0f);

    perturbation_free(orb);
    printf("test_manual_orbit passed!\n");
}

int main(void) {
    printf("running perturbation tests...\n");
    test_zero_orbit();
    test_escape_orbit();
    test_manual_orbit();
    printf("all perturbation tests passed successfully!\n");
    return 0;
}
