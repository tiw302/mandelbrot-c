/* reference only — this file is NOT compiled or loaded at runtime.
 * the actual shaders are inlined in:
 *   src/app/shaders.h             (web / gles3)
 *   src/app/desktop_gpu_shaders.h (desktop / glcore)
 *
 * this file serves as a readable reference for the sokol-shdc format.
 * it defines the vertex pass-through and the dual-path fractal fragment shader. */

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
    vec2  center_lo;       // error residual for simulated 64-bit precision
    vec2  julia_c_hi;      // julia set c-parameter (high part)
    vec2  julia_c_lo;      // julia set c-parameter (low part)
    float zoom;            // complex units per screen height
    float iters;           // maximum iteration cap
    float aspect_ratio;    // win_w / win_h
    float fractal_type;    // 0=mandelbrot, 1=julia, 2=burning ship
    float palette_idx;     // index into the 9 available palettes
    float high_precision;  // 0.0=32-bit (fast), 1.0=64-bit dekker (deep)
};

in vec2 uv;
out vec4 frag_color;

#define ESCAPE_RADIUS_SQ  100.0 // matching config.h (10.0^2)

/* dekker double-single addition (knuth twosum).
 * provides exact sum representation using two floats. */
vec2 ds_add(vec2 dsa, vec2 dsb) {
    precise float t1 = dsa.x + dsb.x;
    precise float e  = t1 - dsa.x;
    precise float t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;
    precise float t3 = t1 + t2;
    return vec2(t3, t2 - (t3 - t1));
}

/* dekker double-single multiplication (veltkamp split).
 * recovers the full 48-bit product on 32-bit floating point hardware. */
vec2 ds_mul(vec2 dsa, vec2 dsb) {
    precise float cona = dsa.x * 4097.0;
    precise float conb = dsb.x * 4097.0;
    precise float a1 = cona - (cona - dsa.x);
    precise float b1 = conb - (conb - dsb.x);
    precise float a2 = dsa.x - a1;
    precise float b2 = dsb.x - b1;
    precise float c11 = dsa.x * dsb.x;
    precise float c21 = a2*b2 + (a2*b1 + (a1*b2 + (a1*b1 - c11)));
    precise float c2  = dsa.x * dsb.y + dsa.y * dsb.x;
    precise float t1  = c11 + c2;
    precise float e   = t1 - c11;
    precise float t2  = dsa.y * dsb.y + ((c2 - e) + (c11 - (t1 - e))) + c21;
    precise float t3  = t1 + t2;
    return vec2(t3, t2 - (t3 - t1));
}

/* color palette logic — replica of color.c.
 * uses smooth (fractional) iteration counts to interpolate between colors,
 * effectively removing discrete banding artifacts. */
vec3 apply_palette(float iter, float max_iter) {
    if (iter >= max_iter) return vec3(0.0, 0.0, 0.0);
    float fi = iter; 
    vec3 a, b;
    int p = int(palette_idx) % 9;

    if (p == 0) { // sine wave
        a = vec3(sin(0.1*fi+4.0), sin(0.1*fi+2.0), sin(0.1*fi)) * 127.0 + 128.0;
        b = vec3(sin(0.1*(fi+1.0)+4.0), sin(0.1*(fi+1.0)+2.0), sin(0.1*(fi+1.0))) * 127.0 + 128.0;
    } else if (p == 1) { // grayscale
        a = vec3(mod(fi, 256.0)); b = vec3(mod(fi+1.0, 256.0));
    } else if (p == 2) { // fire
        a = vec3(255.-abs(mod(fi*1.,510.)-255.), 255.-abs(mod(fi*2.,510.)-255.), 255.-abs(mod(fi*4.,510.)-255.));
        b = vec3(255.-abs(mod((fi+1.)*1.,510.)-255.), 255.-abs(mod((fi+1.)*2.,510.)-255.), 255.-abs(mod((fi+1.)*4.,510.)-255.));
    } else if (p == 3) { // electric
        a = vec3(255.-abs(mod(fi*8.,510.)-255.), 255.-abs(mod(fi*4.,510.)-255.), 255.-abs(mod(fi*1.,510.)-255.));
        b = vec3(255.-abs(mod((fi+1.)*8.,510.)-255.), 255.-abs(mod((fi+1.)*4.,510.)-255.), 255.-abs(mod((fi+1.)*1.,510.)-255.));
    } else if (p == 4) { // ocean
        a = vec3(255.-abs(mod(fi*5.,510.)-255.), 255.-abs(mod(fi*2.,510.)-255.), 255.-abs(mod(fi*.5,510.)-255.));
        b = vec3(255.-abs(mod((fi+1.)*5.,510.)-255.), 255.-abs(mod((fi+1.)*2.,510.)-255.), 255.-abs(mod((fi+1.)*.5,510.)-255.));
    } else if (p == 5) { // inferno
        a = vec3(255.-abs(mod(fi*.5,510.)-255.), 255.-abs(mod(fi*2.,510.)-255.), 255.-abs(mod(fi*8.,510.)-255.));
        b = vec3(255.-abs(mod((fi+1.)*.5,510.)-255.), 255.-abs(mod((fi+1.)*2.,510.)-255.), 255.-abs(mod((fi+1.)*8.,510.)-255.));
    } else if (p == 6) { // viridis
        // ping-pong wrap for glsl
        float t1 = 1.0 - abs(mod(fi/256., 2.0) - 1.0);
        float t2 = 1.0 - abs(mod((fi+1.)/256., 2.0) - 1.0);
        a=vec3(0.267+t1*(0.993*t1-0.260), 0.004+t1*(1.490-t1*0.494), 0.329+t1*(1.268*t1*t1-0.680*t1-0.259))*255.;
        b=vec3(0.267+t2*(0.993*t2-0.260), 0.004+t2*(1.490-t2*0.494), 0.329+t2*(1.268*t2*t2-0.680*t2-0.259))*255.;
    } else if (p == 7) { // plasma
        float t1 = 1.0 - abs(mod(fi/256., 2.0) - 1.0);
        float t2 = 1.0 - abs(mod((fi+1.)/256., 2.0) - 1.0);
        a=vec3(0.050+t1*(2.735-t1*1.785), max(0.,t1*(1.580*t1-0.580)), max(0.,0.530+t1*(0.750-t1*1.280)))*255.;
        b=vec3(0.050+t2*(2.735-t2*1.785), max(0.,t2*(1.580*t2-0.580)), max(0.,0.530+t2*(0.750-t2*1.280)))*255.;
    } else { // twilight
        float t1 = fract(fi/128.), t2 = fract((fi+1.)/128.);
        a=vec3(0.5+0.5*sin(6.283*t1), 0.3+0.2*sin(6.283*t1+2.094), 0.5+0.5*sin(6.283*t1+4.189))*255.;
        b=vec3(0.5+0.5*sin(6.283*t2), 0.3+0.2*sin(6.283*t2+2.094), 0.5+0.5*sin(6.283*t2+4.189))*255.;
    }
    return mix(a, b, fract(iter)) / 255.0;
}

void main() {
    int m = int(iters), i = 0;
    float mag2 = 0.0;

    // path 1: high-precision (dekker double-single)
    if (high_precision > 0.5) {
        vec2 zoom_ds = vec2(zoom, 0.0), asp_ds = vec2(aspect_ratio, 0.0);
        vec2 cx = vec2(center_hi.x, center_lo.x), cy = vec2(center_hi.y, center_lo.y);
        vec2 px = ds_add(ds_mul(ds_mul(vec2(uv.x-0.5, 0.0), zoom_ds), asp_ds), cx);
        vec2 py = ds_add(ds_mul(vec2(0.5-uv.y, 0.0), zoom_ds), cy);

        vec2 c_val_x = (fractal_type == 1.0) ? vec2(julia_c_hi.x, julia_c_lo.x) : px;
        vec2 c_val_y = (fractal_type == 1.0) ? vec2(julia_c_hi.y, julia_c_lo.y) : py;
        vec2 zx = (fractal_type == 1.0) ? px : vec2(0.0), zy = (fractal_type == 1.0) ? py : vec2(0.0);

        // cardioid rejection (mandelbrot only)
        if (fractal_type < 0.5) {
            float cr = px.x-0.25, ci2 = py.x*py.x, q = cr*cr+ci2;
            if (q*(q+cr) <= 0.25*ci2) { i = m; }
            else { float cr1 = px.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
        }

        for (; i < 2000; i++) {
            if (i >= m) break;
            vec2 x2 = ds_mul(zx, zx), y2 = ds_mul(zy, zy);
            mag2 = x2.x + y2.x;
            if (mag2 > ESCAPE_RADIUS_SQ) break;
            if (fractal_type > 1.5) { // burning ship
                vec2 abs_zx = (zx.x < 0.0) ? vec2(-zx.x, -zx.y) : zx;
                vec2 abs_zy = (zy.x < 0.0) ? vec2(-zy.x, -zy.y) : zy;
                vec2 nzy = ds_add(ds_add(ds_mul(abs_zx, abs_zy), ds_mul(abs_zx, abs_zy)), c_val_y);
                vec2 nzx = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
                zx = nzx; zy = nzy;
            } else { // mandelbrot / julia
                vec2 nzy = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);
                vec2 nzx = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
                zx = nzx; zy = nzy;
            }
        }
    } else {
        // path 2: standard 32-bit (fast)
        vec2 center = center_hi + center_lo;
        vec2 p = vec2((uv.x-0.5)*zoom*aspect_ratio + center.x, (0.5-uv.y)*zoom + center.y);
        vec2 c_val = (fractal_type == 1.0) ? (julia_c_hi + julia_c_lo) : p;
        vec2 z = (fractal_type == 1.0) ? p : vec2(0.0);

        if (fractal_type < 0.5) {
            float cr = p.x-0.25, ci2 = p.y*p.y, q = cr*cr+ci2;
            if (q*(q+cr) <= 0.25*ci2) { i = m; }
            else { float cr1 = p.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
        }

        for (; i < 2000; i++) {
            if (i >= m) break;
            float x2 = z.x*z.x, y2 = z.y*z.y;
            mag2 = x2 + y2;
            if (mag2 > ESCAPE_RADIUS_SQ) break;
            if (fractal_type > 1.5) z = vec2(x2-y2+c_val.x, 2.0*abs(z.x)*abs(z.y)+c_val.y);
            else z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);
        }
    }

    // smooth coloring and output
    if (i >= m) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        float s = float(i) + 2.0 - log2(log(max(1.0, mag2)));
        frag_color = vec4(apply_palette(max(0.0, s), iters), 1.0);
    }
}
@end

@program mandelbrot vs fs
