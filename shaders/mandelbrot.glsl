@vs vs
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = texcoord0;
}
@end

@fs fs
uniform fs_params {
    vec2  center_hi;       // high bits of double-precision center
    vec2  center_lo;       // low bits (center = center_hi + center_lo)
    vec2  julia_c;         // julia c parameter
    float zoom;
    float iters;
    float aspect_ratio;
    float palette_idx;     // 0-5, matches PALETTE_NAMES in color.c
    float is_julia;        // 0.0 = mandelbrot, 1.0 = julia
    float high_precision;  // 0.0 = 32-bit, 1.0 = Dekker double-single
};

in vec2 uv;
out vec4 frag_color;

#define ESCAPE_RADIUS_SQ  100.0   // ESCAPE_RADIUS=10 in config.h, 10^2=100

// ---------------------------------------------------------------------------
// Dekker double-single arithmetic
// A ds number is a vec2 where .x=high part, .y=error residual.
// Together they recover ~48 mantissa bits from two 24-bit floats.
// ---------------------------------------------------------------------------
vec2 ds_add(vec2 a, vec2 b) {
    precise float t1 = a.x + b.x;
    precise float e  = t1 - a.x;
    precise float t2 = ((b.x - e) + (a.x - (t1 - e))) + a.y + b.y;
    precise float t3 = t1 + t2;
    return vec2(t3, t2 - (t3 - t1));
}

vec2 ds_mul(vec2 a, vec2 b) {
    precise float cona = a.x * 4097.0;
    precise float conb = b.x * 4097.0;
    precise float a1   = cona - (cona - a.x);
    precise float b1   = conb - (conb - b.x);
    precise float a2   = a.x - a1;
    precise float b2   = b.x - b1;
    precise float c11  = a.x * b.x;
    precise float c21  = a2*b2 + (a2*b1 + (a1*b2 + (a1*b1 - c11)));
    precise float c2   = a.x * b.y + a.y * b.x;
    precise float t1   = c11 + c2;
    precise float e    = t1 - c11;
    precise float t2   = a.y * b.y + ((c2 - e) + (c11 - (t1 - e))) + c21;
    precise float t3   = t1 + t2;
    return vec2(t3, t2 - (t3 - t1));
}

// ---------------------------------------------------------------------------
// Color palettes - exact match to color.c (all 6, same coefficients)
// Fractional iteration interpolation eliminates color banding.
// ---------------------------------------------------------------------------
vec3 apply_palette(float iter, float max_iter) {
    if (iter >= max_iter) return vec3(0.0);
    float fi   = floor(iter);
    float frac = fract(iter);
    vec3 a, b;
    int p = int(palette_idx) % 6;
    if (p == 0) {
        a = vec3(sin(0.1*fi+4.0), sin(0.1*fi+2.0), sin(0.1*fi)) * 127.0 + 128.0;
        b = vec3(sin(0.1*(fi+1.0)+4.0), sin(0.1*(fi+1.0)+2.0), sin(0.1*(fi+1.0))) * 127.0 + 128.0;
    } else if (p == 1) {
        a = vec3(mod(fi, 256.0)); b = vec3(mod(fi+1.0, 256.0));
    } else if (p == 2) {
        a = clamp(vec3(fi*1.0, fi*2.0, fi*4.0), 0.0, 255.0);
        b = clamp(vec3((fi+1.0)*1.0, (fi+1.0)*2.0, (fi+1.0)*4.0), 0.0, 255.0);
    } else if (p == 3) {
        a = clamp(vec3(fi*8.0, fi*4.0, fi*1.0), 0.0, 255.0);
        b = clamp(vec3((fi+1.0)*8.0, (fi+1.0)*4.0, (fi+1.0)*1.0), 0.0, 255.0);
    } else if (p == 4) {
        a = clamp(vec3(fi*5.0, fi*2.0, fi*0.5), 0.0, 255.0);
        b = clamp(vec3((fi+1.0)*5.0, (fi+1.0)*2.0, (fi+1.0)*0.5), 0.0, 255.0);
    } else {
        a = clamp(vec3(fi*0.5, fi*2.0, fi*8.0), 0.0, 255.0);
        b = clamp(vec3((fi+1.0)*0.5, (fi+1.0)*2.0, (fi+1.0)*8.0), 0.0, 255.0);
    }
    return mix(a, b, frac) / 255.0;
}

// ---------------------------------------------------------------------------
// Cardioid and period-2 bulb rejection (matches mandelbrot.c exactly)
// ---------------------------------------------------------------------------
bool in_main_set(vec2 c) {
    float cr  = c.x - 0.25;
    float ci2 = c.y * c.y;
    float q   = cr * cr + ci2;
    if (q * (q + cr) <= 0.25 * ci2) return true;
    float cr1 = c.x + 1.0;
    if (cr1 * cr1 + ci2 <= 0.0625)  return true;
    return false;
}

void main() {
    int m = int(iters);
    int i = 0;
    float mag2 = 0.0;

    if (high_precision > 0.5) {
        // High-precision: Dekker double-single arithmetic
        vec2 zoom_ds = vec2(zoom, 0.0);
        vec2 asp_ds  = vec2(aspect_ratio, 0.0);
        vec2 cx = vec2(center_hi.x, center_lo.x);
        vec2 cy = vec2(center_hi.y, center_lo.y);
        vec2 px = ds_add(ds_mul(ds_mul(vec2(uv.x-0.5, 0.0), zoom_ds), asp_ds), cx);
        vec2 py = ds_add(ds_mul(vec2(0.5-uv.y, 0.0), zoom_ds), cy);

        vec2 c_val_x = (is_julia > 0.5) ? vec2(julia_c.x, 0.0) : px;
        vec2 c_val_y = (is_julia > 0.5) ? vec2(julia_c.y, 0.0) : py;
        vec2 zx      = (is_julia > 0.5) ? px : vec2(0.0);
        vec2 zy      = (is_julia > 0.5) ? py : vec2(0.0);

        if (is_julia < 0.5) {
            float cr = px.x-0.25, ci2 = py.x*py.x, q = cr*cr+ci2;
            if (q*(q+cr) <= 0.25*ci2) { i = m; }
            else { float cr1 = px.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
        }

        for (; i < m; i++) {
            vec2 x2  = ds_mul(zx, zx);
            vec2 y2  = ds_mul(zy, zy);
            mag2     = x2.x + y2.x;
            if (mag2 > ESCAPE_RADIUS_SQ) break;
            vec2 nzx = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
            vec2 nzy = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);
            zx = nzx; zy = nzy;
        }
    } else {
        // Standard 32-bit path
        vec2 center = center_hi + center_lo;
        vec2 p = vec2((uv.x-0.5)*zoom*aspect_ratio + center.x,
                      (0.5-uv.y)*zoom + center.y);
        vec2 c_val = (is_julia > 0.5) ? julia_c : p;
        vec2 z     = (is_julia > 0.5) ? p : vec2(0.0);

        if (is_julia < 0.5) {
            float cr = p.x-0.25, ci2 = p.y*p.y, q = cr*cr+ci2;
            if (q*(q+cr) <= 0.25*ci2) { i = m; }
            else { float cr1 = p.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
        }

        for (; i < m; i++) {
            float x2 = z.x*z.x, y2 = z.y*z.y;
            mag2 = x2 + y2;
            if (mag2 > ESCAPE_RADIUS_SQ) break;
            z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);
        }
    }

    if (i >= m) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        float s = float(i) + 2.0 - log2(log(max(1.0, mag2)));
        frag_color = vec4(apply_palette(max(0.0, s), iters), 1.0);
    }
}
@end

@program mandelbrot vs fs
