#ifndef SHADERS_H
#define SHADERS_H

static const char* vs_src =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 pos;\n"
    "in vec2 uv_in;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    uv = uv_in;\n"
    "}\n";

static const char* fs_cpu_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv);\n"
    "}\n";

/* gpu shader: exact match to cpu color.c palettes and mandelbrot.c smooth formula. */
static const char* fs_gpu_src =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "uniform vec2 u_center_hi;\n"
    "uniform vec2 u_center_lo;\n"
    "uniform vec2 u_julia_c_hi;\n"
    "uniform vec2 u_julia_c_lo;\n"
    "uniform float u_zoom;\n"
    "uniform float u_iters;\n"
    "uniform float u_aspect;\n"
    "uniform float u_fractal_type;\n"
    "uniform float u_palette;\n"
    "uniform float u_high_precision;\n"
    "uniform float u_zero;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"

    /* ---------------------------------------------------------------
     * Optimization barrier for GLSL ES 3.0 (WebGL2).
     *
     * GLSL ES 3.0 has no 'precise' qualifier. The compiler is free to
     * algebraically simplify (a+b)-a → b, destroying the rounding-
     * error that TwoSum/Dekker arithmetic relies on.
     *
     * u_zero is a uniform always set to 0.0 from the CPU side.
     * x + u_zero == x exactly (IEEE 754), but the compiler CANNOT
     * know u_zero's value at compile time, so it MUST emit an actual
     * ADD instruction — preventing algebraic folding.
     * --------------------------------------------------------------- */
    "float B(float x) { return x + u_zero; }\n"
    "\n"
    /* ds_add: Knuth TwoSum algorithm (DSFUN90).
     * B() barriers prevent the compiler from folding (a+b)-a → b. */
    "vec2 ds_add(vec2 dsa, vec2 dsb) {\n"
    "    float t1 = dsa.x + dsb.x;\n"
    "    float e  = B(t1) - dsa.x;\n"
    "    float t2 = ((dsb.x - e) + (dsa.x - (B(t1) - e))) + dsa.y + dsb.y;\n"
    "    float hi = t1 + t2;\n"
    "    return vec2(hi, t2 - (B(hi) - t1));\n"
    "}\n"
    "\n"
    /* ds_mul: Dekker multiplication (DSFUN90 / Veltkamp split).
     * Split constant = 4097 = 2^12+1, correct for 24-bit float mantissa.
     * B() on c11/t1/hi prevents compiler from expanding a*b symbolically. */
    "vec2 ds_mul(vec2 dsa, vec2 dsb) {\n"
    "    float cona = dsa.x * 4097.0;\n"
    "    float conb = dsb.x * 4097.0;\n"
    "    float a1 = cona - (B(cona) - dsa.x);\n"
    "    float b1 = conb - (B(conb) - dsb.x);\n"
    "    float a2 = dsa.x - a1;\n"
    "    float b2 = dsb.x - b1;\n"
    "    float c11 = dsa.x * dsb.x;\n"
    "    float c21 = a2*b2 + (a2*b1 + (a1*b2 + (a1*b1 - B(c11))));\n"
    "    float c2  = dsa.x * dsb.y + dsa.y * dsb.x;\n"
    "    float t1  = c11 + c2;\n"
    "    float e   = B(t1) - c11;\n"
    "    float t2  = dsa.y * dsb.y + ((c2 - e) + (c11 - (B(t1) - e))) + c21;\n"
    "    float hi  = t1 + t2;\n"
    "    return vec2(hi, t2 - (B(hi) - t1));\n"
    "}\n"

    "vec3 lut_color(float fi, int pal) {\n"
    "    float i = fi;\n"
    "    vec3 a, b;\n"
    "    if (pal == 0) {\n" /* sine wave: swapped phases (4,2,0) to match cpu mint appearance */
    "        a = vec3(sin(0.1*i+4.0)*127.0+128.0, sin(0.1*i+2.0)*127.0+128.0, "
    "sin(0.1*i+0.0)*127.0+128.0) / 255.0;\n"
    "        b = vec3(sin(0.1*(i+1.0)+4.0)*127.0+128.0, sin(0.1*(i+1.0)+2.0)*127.0+128.0, "
    "sin(0.1*(i+1.0)+0.0)*127.0+128.0) / 255.0;\n"
    "    } else if (pal == 1) {\n" /* grayscale */
    "        float v = mod(i, 256.0) / 255.0;\n"
    "        float v2 = mod(i+1.0, 256.0) / 255.0;\n"
    "        a = vec3(v); b = vec3(v2);\n"
    "    } else if (pal == 2) {\n" /* fire (swapped to match cpu) */
    "        a = vec3(255.0-abs(mod(i*1.0, 510.0)-255.0), 255.0-abs(mod(i*2.0, 510.0)-255.0), 255.0-abs(mod(i*4.0, 510.0)-255.0)) / 255.0;\n"
    "        b = vec3(255.0-abs(mod((i+1.0)*1.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*2.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*4.0, 510.0)-255.0)) / 255.0;\n"
    "    } else if (pal == 3) {\n" /* electric (swapped to match cpu) */
    "        a = vec3(255.0-abs(mod(i*8.0, 510.0)-255.0), 255.0-abs(mod(i*4.0, 510.0)-255.0), 255.0-abs(mod(i*1.0, 510.0)-255.0)) / 255.0;\n"
    "        b = vec3(255.0-abs(mod((i+1.0)*8.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*4.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*1.0, 510.0)-255.0)) / 255.0;\n"
    "    } else if (pal == 4) {\n" /* ocean (swapped to match cpu) */
    "        a = vec3(255.0-abs(mod(i*5.0, 510.0)-255.0), 255.0-abs(mod(i*2.0, 510.0)-255.0), 255.0-abs(mod(i*0.5, 510.0)-255.0)) / 255.0;\n"
    "        b = vec3(255.0-abs(mod((i+1.0)*5.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*2.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*0.5, 510.0)-255.0)) / 255.0;\n"
    "    } else if (pal == 5) {\n" /* inferno */
    "        a = vec3(255.0-abs(mod(i*0.5, 510.0)-255.0), 255.0-abs(mod(i*2.0, 510.0)-255.0), 255.0-abs(mod(i*8.0, 510.0)-255.0)) / 255.0;\n"
    "        b = vec3(255.0-abs(mod((i+1.0)*0.5, 510.0)-255.0), 255.0-abs(mod((i+1.0)*2.0, 510.0)-255.0), 255.0-abs(mod((i+1.0)*8.0, 510.0)-255.0)) / 255.0;\n"
    "    } else if (pal == 6) {\n" /* viridis */
    "        float t1 = fract(i/256.0); float t2 = fract((i+1.0)/256.0);\n"
    "        a = vec3(0.267+t1*(0.993*t1-0.260), 0.004+t1*(1.490-t1*0.494), 0.329+t1*(1.268*t1*t1-0.680*t1-0.259));\n"
    "        b = vec3(0.267+t2*(0.993*t2-0.260), 0.004+t2*(1.490-t2*0.494), 0.329+t2*(1.268*t2*t2-0.680*t2-0.259));\n"
    "    } else if (pal == 7) {\n" /* plasma */
    "        float t1 = fract(i/256.0); float t2 = fract((i+1.0)/256.0);\n"
    "        a = vec3(0.050+t1*(2.735-t1*1.785), max(0.0,t1*(1.580*t1-0.580)), max(0.0,0.530+t1*(0.750-t1*1.280)));\n"
    "        b = vec3(0.050+t2*(2.735-t2*1.785), max(0.0,t2*(1.580*t2-0.580)), max(0.0,0.530+t2*(0.750-t2*1.280)));\n"
    "    } else {\n" /* twilight */
    "        float t1 = fract(i/128.0); float t2 = fract((i+1.0)/128.0);\n"
    "        a = vec3(0.5+0.5*sin(6.283*t1), 0.3+0.2*sin(6.283*t1+2.094), 0.5+0.5*sin(6.283*t1+4.189));\n"
    "        b = vec3(0.5+0.5*sin(6.283*t2), 0.3+0.2*sin(6.283*t2+2.094), 0.5+0.5*sin(6.283*t2+4.189));\n"
    "    }\n"
    "    return mix(a, b, fract(fi));\n"
    "}\n"

    "void main() {\n"
    "    int m = int(u_iters), i = 0;\n"
    "    float mag_sq = 0.0;\n"
    "    if (u_high_precision > 0.5) {\n"
    "        vec2 uv_dx = vec2(uv.x - 0.5, 0.0);\n"
    "        vec2 uv_dy = vec2(0.5 - uv.y, 0.0);\n"
    "        vec2 zoom_a = vec2(u_zoom, 0.0);\n"
    "        vec2 aspect_a = vec2(u_aspect, 0.0);\n"
    "        vec2 cx = vec2(u_center_hi.x, u_center_lo.x);\n"
    "        vec2 cy = vec2(u_center_hi.y, u_center_lo.y);\n"
    "        vec2 px = ds_add(ds_mul(ds_mul(uv_dx, zoom_a), aspect_a), cx);\n"
    "        vec2 py = ds_add(ds_mul(uv_dy, zoom_a), cy);\n"
    "        vec2 c_val_x = (u_fractal_type == 1.0) ? vec2(u_julia_c_hi.x, u_julia_c_lo.x) : px;\n"
    "        vec2 c_val_y = (u_fractal_type == 1.0) ? vec2(u_julia_c_hi.y, u_julia_c_lo.y) : py;\n"
    "        vec2 zx = (u_fractal_type == 1.0) ? px : vec2(0.0);\n"
    "        vec2 zy = (u_fractal_type == 1.0) ? py : vec2(0.0);\n"
    "        if (u_fractal_type < 0.5) {\n"
    "            float cr = px.x - 0.25, ci2 = py.x * py.x, q = cr*cr + ci2;\n"
    "            if (q * (q + cr) <= 0.25 * ci2) { i = m; }\n"
    "            else { float cr1 = px.x + 1.0; if (cr1*cr1 + ci2 <= 0.0625) i = m; }\n"
    "        }\n"
    /* gpu loop cap: glsl needs a compile-time constant bound on many drivers.
     * 2000 is safe for broad compatibility. for deeper counts use cpu mode. */
    "        for (i = i; i < 2000; i++) {\n"
    "            if (i >= m) break;\n"
    "            vec2 x2 = ds_mul(zx, zx);\n"
    "            vec2 y2 = ds_mul(zy, zy);\n"
    "            mag_sq = x2.x + y2.x;\n"
    "            if (mag_sq > 100.0) break;\n"
    "            if (u_fractal_type > 1.5) {\n"
    "                vec2 abs_zx = (zx.x < 0.0) ? vec2(-zx.x, -zx.y) : zx;\n"
    "                vec2 abs_zy = (zy.x < 0.0) ? vec2(-zy.x, -zy.y) : zy;\n"
    "                vec2 zy_new = ds_add(ds_add(ds_mul(abs_zx, abs_zy), ds_mul(abs_zx, abs_zy)), c_val_y);\n"
    "                vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);\n"
    "                zx = zx_new; zy = zy_new;\n"
    "            } else {\n"
    "                vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);\n"
    "                vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);\n"
    "                zx = zx_new; zy = zy_new;\n"
    "            }\n"
    "        }\n"
    "    } else {\n"
    "        vec2 center = u_center_hi + u_center_lo;\n"
    "        vec2 p = vec2((uv.x - 0.5) * u_zoom * u_aspect + center.x,\n"
    "                      (0.5 - uv.y) * u_zoom + center.y);\n"
    "        vec2 c_val = (u_fractal_type == 1.0) ? (u_julia_c_hi + u_julia_c_lo) : p;\n"
    "        vec2 z = (u_fractal_type == 1.0) ? p : vec2(0.0);\n"
    "        float escape_sq = 100.0;\n"
    "        if (u_fractal_type < 0.5) {\n"
    "            float cr = p.x - 0.25, ci2 = p.y * p.y, q = cr*cr + ci2;\n"
    "            if (q * (q + cr) <= 0.25 * ci2) { i = m; }\n"
    "            else { float cr1 = p.x + 1.0; if (cr1*cr1 + ci2 <= 0.0625) i = m; }\n"
    "        }\n"
    /* same cap applies to the standard 32-bit path */
    "        for (i = i; i < 2000; i++) {\n"
    "            if (i >= m) break;\n"
    "            float x2 = z.x * z.x, y2 = z.y * z.y;\n"
    "            mag_sq = x2 + y2;\n"
    "            if (mag_sq > escape_sq) break;\n"
    "            if (u_fractal_type > 1.5) {\n"
    "                z = vec2(x2 - y2 + c_val.x, 2.0 * abs(z.x) * abs(z.y) + c_val.y);\n"
    "            } else {\n"
    "                z = vec2(x2 - y2 + c_val.x, 2.0 * z.x * z.y + c_val.y);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    if (i >= m) {\n"
    "        frag_color = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "    } else {\n"
    "        float s = float(i) + 2.0 - log2(log(max(1.0, mag_sq)));\n"
    "        frag_color = vec4(lut_color(max(0.0, s), int(u_palette)), 1.0);\n"
    "    }\n"
    "}\n";

#endif
