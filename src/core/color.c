#include "color.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

typedef struct {
    double pos;
    uint8_t r, g, b;
} ColorStop;

typedef struct {
    char name[64];
    ColorStop* stops;
    int num_stops;
} PaletteDef;

static PaletteDef* loaded_palettes = NULL;
static int loaded_palette_count = 0;

static int current_palette = 0;
static uint32_t* palette_lut_argb = NULL;
static int lut_size = 0;

#define NUM_BUILTIN_PALETTES 9
static const char* builtin_palette_names[NUM_BUILTIN_PALETTES] = {
    "Sine Wave (GPU)", "Grayscale", "Fire",   "Electric", "Ocean",
    "Inferno",         "Viridis",   "Plasma", "Twilight"};

static void get_builtin_color(double fi, int pal, uint8_t* r, uint8_t* g, uint8_t* b) {
    double i_val = floor(fi);
    double fract = fi - i_val;
    double a[3], b_vec[3];

    for (int step = 0; step < 2; step++) {
        double iv = i_val + step;
        double* out = (step == 0) ? a : b_vec;

        if (pal == 0) {
            out[0] = (sin(0.1 * iv + 4.0) * 127.0 + 128.0) / 255.0;
            out[1] = (sin(0.1 * iv + 2.0) * 127.0 + 128.0) / 255.0;
            out[2] = (sin(0.1 * iv + 0.0) * 127.0 + 128.0) / 255.0;
        } else if (pal == 1) {
            out[0] = fmod(iv, 256.0) / 255.0;
            out[1] = out[0];
            out[2] = out[0];
        } else if (pal == 2) {
            out[0] = (255.0 - fabs(fmod(iv * 1.0, 510.0) - 255.0)) / 255.0;
            out[1] = (255.0 - fabs(fmod(iv * 2.0, 510.0) - 255.0)) / 255.0;
            out[2] = (255.0 - fabs(fmod(iv * 4.0, 510.0) - 255.0)) / 255.0;
        } else if (pal == 3) {
            out[0] = (255.0 - fabs(fmod(iv * 8.0, 510.0) - 255.0)) / 255.0;
            out[1] = (255.0 - fabs(fmod(iv * 4.0, 510.0) - 255.0)) / 255.0;
            out[2] = (255.0 - fabs(fmod(iv * 1.0, 510.0) - 255.0)) / 255.0;
        } else if (pal == 4) {
            out[0] = (255.0 - fabs(fmod(iv * 5.0, 510.0) - 255.0)) / 255.0;
            out[1] = (255.0 - fabs(fmod(iv * 2.0, 510.0) - 255.0)) / 255.0;
            out[2] = (255.0 - fabs(fmod(iv * 0.5, 510.0) - 255.0)) / 255.0;
        } else if (pal == 5) {
            out[0] = (255.0 - fabs(fmod(iv * 0.5, 510.0) - 255.0)) / 255.0;
            out[1] = (255.0 - fabs(fmod(iv * 2.0, 510.0) - 255.0)) / 255.0;
            out[2] = (255.0 - fabs(fmod(iv * 8.0, 510.0) - 255.0)) / 255.0;
        } else if (pal == 6) {
            double t1 = 1.0 - fabs(fmod(iv / 256.0, 2.0) - 1.0);
            out[0] = 0.267 + t1 * (0.993 * t1 - 0.260);
            out[1] = 0.004 + t1 * (1.490 - t1 * 0.494);
            out[2] = 0.329 + t1 * (1.268 * t1 * t1 - 0.680 * t1 - 0.259);
        } else if (pal == 7) {
            double t1 = 1.0 - fabs(fmod(iv / 256.0, 2.0) - 1.0);
            out[0] = 0.050 + t1 * (2.735 - t1 * 1.785);
            out[1] = t1 * (1.580 * t1 - 0.580);
            if (out[1] < 0) out[1] = 0;
            out[2] = 0.530 + t1 * (0.750 - t1 * 1.280);
            if (out[2] < 0) out[2] = 0;
        } else {
            double t1 = iv / 128.0;
            t1 = t1 - floor(t1);  // fract
            out[0] = 0.5 + 0.5 * sin(6.283 * t1);
            out[1] = 0.3 + 0.2 * sin(6.283 * t1 + 2.094);
            out[2] = 0.5 + 0.5 * sin(6.283 * t1 + 4.189);
        }
    }

    *r = (uint8_t)((a[0] + (b_vec[0] - a[0]) * fract) * 255.0);
    *g = (uint8_t)((a[1] + (b_vec[1] - a[1]) * fract) * 255.0);
    *b = (uint8_t)((a[2] + (b_vec[2] - a[2]) * fract) * 255.0);
}

static void load_palettes_json(void) {
    if (loaded_palettes) return;

    cjsonx_doc_t* doc = cjsonx_read_file("assets/palettes.json");
    if (!doc) return;

    cjsonx_val_t root = doc->root;
    if (cjsonx_get_type(root) != CJSONX_ARRAY) {
        cjsonx_doc_free(doc);
        return;
    }

    loaded_palette_count = cjsonx_size(root);
    if (loaded_palette_count == 0) {
        cjsonx_doc_free(doc);
        return;
    }

    loaded_palettes = (PaletteDef*)malloc(sizeof(PaletteDef) * loaded_palette_count);

    for (int i = 0; i < loaded_palette_count; i++) {
        cjsonx_val_t p_obj = cjsonx_get_index(root, i);
        cjsonx_val_t name = cjsonx_get(p_obj, "name");
        cjsonx_val_t stops = cjsonx_get(p_obj, "stops");

        if (cjsonx_get_type(name) == CJSONX_STRING) {
            size_t len = cjsonx_str_len(name);
            if (len > 63) len = 63;
            strncpy(loaded_palettes[i].name, cjsonx_str(name), len);
            loaded_palettes[i].name[len] = '\0';
        } else {
            strcpy(loaded_palettes[i].name, "Unknown");
        }

        if (cjsonx_get_type(stops) == CJSONX_ARRAY) {
            loaded_palettes[i].num_stops = cjsonx_size(stops);
            loaded_palettes[i].stops =
                (ColorStop*)malloc(sizeof(ColorStop) * loaded_palettes[i].num_stops);
            for (int j = 0; j < loaded_palettes[i].num_stops; j++) {
                cjsonx_val_t s_obj = cjsonx_get_index(stops, j);
                cjsonx_val_t pos = cjsonx_get(s_obj, "pos");
                cjsonx_val_t r = cjsonx_get(s_obj, "r");
                cjsonx_val_t g = cjsonx_get(s_obj, "g");
                cjsonx_val_t b = cjsonx_get(s_obj, "b");

                loaded_palettes[i].stops[j].pos =
                    (cjsonx_get_type(pos) == CJSONX_NUMBER) ? cjsonx_num(pos) : 0.0;
                loaded_palettes[i].stops[j].r =
                    (cjsonx_get_type(r) == CJSONX_NUMBER) ? cjsonx_int(r) : 0;
                loaded_palettes[i].stops[j].g =
                    (cjsonx_get_type(g) == CJSONX_NUMBER) ? cjsonx_int(g) : 0;
                loaded_palettes[i].stops[j].b =
                    (cjsonx_get_type(b) == CJSONX_NUMBER) ? cjsonx_int(b) : 0;
            }
        } else {
            loaded_palettes[i].num_stops = 0;
            loaded_palettes[i].stops = NULL;
        }
    }

    cjsonx_doc_free(doc);
}

int get_palette_count(void) {
    if (!loaded_palettes && loaded_palette_count == 0) load_palettes_json();
    return NUM_BUILTIN_PALETTES + loaded_palette_count;
}

const char* get_palette_name(int idx) {
    if (!loaded_palettes && loaded_palette_count == 0) load_palettes_json();
    int total_palettes = NUM_BUILTIN_PALETTES + loaded_palette_count;
    if (idx < 0 || idx >= total_palettes) return "Unknown";

    if (idx < NUM_BUILTIN_PALETTES) {
        return builtin_palette_names[idx];
    } else {
        return loaded_palettes[idx - NUM_BUILTIN_PALETTES].name;
    }
}

uint32_t* get_palette_lut(void) {
    return palette_lut_argb;
}

int get_palette_lut_size(void) {
    return lut_size;
}

int init_color_palette(int max_iterations, int palette_idx) {
    if (!loaded_palettes && loaded_palette_count == 0) load_palettes_json();

    int total_palettes = NUM_BUILTIN_PALETTES + loaded_palette_count;
    current_palette = palette_idx % total_palettes;

    int needed_size = (max_iterations + 2) * 256;
    if (!palette_lut_argb || lut_size < needed_size) {
        uint32_t* new_lut = (uint32_t*)realloc(palette_lut_argb, sizeof(uint32_t) * needed_size);
        if (new_lut) {
            palette_lut_argb = new_lut;
            lut_size = needed_size;
        } else {
            return 0;
        }
    }

    if (current_palette < NUM_BUILTIN_PALETTES) {
        for (int fi = 0; fi < needed_size; fi++) {
            double iter_frac = (double)fi / 256.0;
            uint8_t r, g, b;
            get_builtin_color(iter_frac, current_palette, &r, &g, &b);
            palette_lut_argb[fi] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    } else {
        PaletteDef* p = &loaded_palettes[current_palette - NUM_BUILTIN_PALETTES];

        for (int fi = 0; fi < needed_size; fi++) {
            double iter_frac = (double)fi / 256.0;
            double t = fmod(iter_frac / 256.0, 1.0);

            uint8_t r = 0, g = 0, b = 0;
            if (p->num_stops > 0) {
                int stop_idx = 0;
                while (stop_idx < p->num_stops - 1 && p->stops[stop_idx + 1].pos < t) {
                    stop_idx++;
                }
                if (stop_idx == p->num_stops - 1) {
                    r = p->stops[stop_idx].r;
                    g = p->stops[stop_idx].g;
                    b = p->stops[stop_idx].b;
                } else {
                    ColorStop* s1 = &p->stops[stop_idx];
                    ColorStop* s2 = &p->stops[stop_idx + 1];
                    double frac = (t - s1->pos) / (s2->pos - s1->pos);
                    r = (uint8_t)(s1->r + frac * (s2->r - s1->r));
                    g = (uint8_t)(s1->g + frac * (s2->g - s1->g));
                    b = (uint8_t)(s1->b + frac * (s2->b - s1->b));
                }
            }
            palette_lut_argb[fi] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return 1;
}

void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (iterations >= max_iterations || !palette_lut_argb) {
        *r = *g = *b = 0;
        return;
    }

    int idx = (int)(iterations * 256.0);
    if (idx < 0) idx = 0;
    if (idx >= lut_size) idx = lut_size - 1;

    uint32_t color = palette_lut_argb[idx];
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

void cleanup_color_palette(void) {
    if (palette_lut_argb) {
        free(palette_lut_argb);
        palette_lut_argb = NULL;
    }
    lut_size = 0;
    if (loaded_palettes) {
        for (int i = 0; i < loaded_palette_count; i++) {
            free(loaded_palettes[i].stops);
        }
        free(loaded_palettes);
        loaded_palettes = NULL;
    }
    loaded_palette_count = 0;
}
