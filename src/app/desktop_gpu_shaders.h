#ifndef DESKTOP_GPU_SHADERS_H
#define DESKTOP_GPU_SHADERS_H

/* desktop_gpu_shaders.h
 *
 * inline glsl source strings for the desktop gpu rendering path.
 * three shaders are defined:
 *
 *   dg_vs      — shared vertex shader (pass-through uv to fragment stage)
 *   dg_fs_cpu  — cpu-mode fragment shader (samples a texture uploaded by the cpu renderer)
 *   dg_fs_gpu  — gpu-mode fragment shader (computes fractal entirely on the gpu)
 *
 * the gpu fragment shader supports:
 *   - mandelbrot and julia set rendering (switched via u_is_julia uniform)
 *   - 6 color palettes matching color.c exactly (switched via u_palette uniform)
 *   - 32-bit and dekker double-single 64-bit precision (switched via u_high_precision uniform)
 *   - cardioid and period-2 bulb early rejection in mandelbrot mode
 *   - smooth (fractional) iteration coloring to eliminate banding
 */

/* vertex shader — shared between cpu and gpu fragment paths.
 * maps clip-space quad positions to uv coordinates for the fragment stage. */
static const char* dg_vs =
    "#version 330\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv_in;"
    "out vec2 uv;"
    "void main() { gl_Position = vec4(pos,0.0,1.0); uv = uv_in; }";

/* cpu-mode fragment shader — used when rendering on the cpu thread pool.
 * the cpu fills an sdl texture with argb8888 pixels; this shader simply
 * samples that texture and outputs it to the screen. */
static const char* dg_fs_cpu =
    "#version 330\n"
    "uniform sampler2D tex; in vec2 uv; out vec4 color;"
    "void main() { color = texture(tex, uv); }";

/* gpu-mode fragment shader — computes the full fractal on the gpu.
 *
 * uniforms:
 *   u_center_hi / u_center_lo  — hi-lo split of the double-precision view center
 *   u_julia_c                  — julia set c parameter (used when u_is_julia > 0.5)
 *   u_zoom                     — zoom level (complex plane units per screen height)
 *   u_iters                    — maximum iteration count
 *   u_aspect                   — window aspect ratio (width / height)
 *   u_is_julia                 — 0.0 = mandelbrot mode, 1.0 = julia mode
 *   u_palette                  — palette index 0-5 (matches PALETTE_NAMES in color.c)
 *   u_high_precision           — 0.0 = standard 32-bit, 1.0 = dekker double-single
 */
static const char* dg_fs_gpu =
    "#version 400\n"
    "uniform vec2 u_center_hi; uniform vec2 u_center_lo;"
    "uniform vec2 u_julia_c; uniform float u_zoom; uniform float u_iters; uniform float u_aspect;"
    "uniform float u_is_julia; uniform float u_palette; uniform float u_high_precision;"
    "in vec2 uv; out vec4 color;\n"

    /* dekker double-single addition (knuth twosum algorithm).
     * computes (dsa + dsb) as a double-single pair with no precision loss.
     * safe against fma fusion because it only uses addition and subtraction. */
    "vec2 ds_add(vec2 dsa, vec2 dsb) {\n"
    "  precise float t1 = dsa.x + dsb.x;\n"
    "  precise float e  = t1 - dsa.x;\n"
    "  precise float t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;\n"
    "  precise float t3 = t1 + t2;\n"
    "  return vec2(t3, t2 - (t3 - t1));\n"
    "}\n"

    /* dekker double-single multiplication (veltkamp split).
     * splits each operand into high/low 12-bit halves before multiplying,
     * recovering the full 48-bit product without hardware fp64. */
    "vec2 ds_mul(vec2 dsa, vec2 dsb) {\n"
    "  precise float cona = dsa.x * 4097.0;\n"
    "  precise float conb = dsb.x * 4097.0;\n"
    "  precise float a1 = cona - (cona - dsa.x);\n"
    "  precise float b1 = conb - (conb - dsb.x);\n"
    "  precise float a2 = dsa.x - a1;\n"
    "  precise float b2 = dsb.x - b1;\n"
    "  precise float c11 = dsa.x * dsb.x;\n"
    "  precise float c21 = a2*b2 + (a2*b1 + (a1*b2 + (a1*b1 - c11)));\n"
    "  precise float c2  = dsa.x * dsb.y + dsa.y * dsb.x;\n"
    "  precise float t1  = c11 + c2;\n"
    "  precise float e   = t1 - c11;\n"
    "  precise float t2  = dsa.y * dsb.y + ((c2 - e) + (c11 - (t1 - e))) + c21;\n"
    "  precise float t3  = t1 + t2;\n"
    "  return vec2(t3, t2 - (t3 - t1));\n"
    "}\n"

    /* color palette lookup — exact match to color.c lut logic.
     * fi is the fractional iteration count (smooth coloring formula output).
     * pal selects one of 6 palettes: 0=sine wave, 1=grayscale, 2=fire,
     * 3=electric, 4=ocean, 5=inferno.
     * interpolates between floor(fi) and floor(fi)+1 to eliminate banding. */
    "vec3 lut_color(float fi, int pal) {\n"
    "  float i = fi; vec3 a,b;\n"
    "  if (pal==0) {\n" /* sine wave: phase offsets (4,2,0) match color.c */
    "    a=vec3(sin(0.1*i+4.0)*127.+128., sin(0.1*i+2.0)*127.+128., "
    "sin(0.1*i+0.0)*127.+128.)/255.;\n"
    "    b=vec3(sin(0.1*(i+1.0)+4.0)*127.+128., sin(0.1*(i+1.0)+2.0)*127.+128., "
    "sin(0.1*(i+1.0)+0.0)*127.+128.)/255.;\n"
    "  } else if (pal==1) {\n" /* grayscale: iteration mod 256 -> brightness */
    "    a=vec3(mod(i,256.)/255.); b=vec3(mod(i+1.,256.)/255.);\n"
    "  } else if (pal==2) {\n" /* fire: coefficients r*1, g*2, b*4 */
    "    a=vec3(min(255.,i*1.),min(255.,i*2.),min(255.,i*4.))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*1.),min(255.,(i+1.)*2.),min(255.,(i+1.)*4.))/255.;\n"
    "  } else if (pal==3) {\n" /* electric: coefficients r*8, g*4, b*1 */
    "    a=vec3(min(255.,i*8.),min(255.,i*4.),min(255.,i*1.))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*8.),min(255.,(i+1.)*4.),min(255.,(i+1.)*1.))/255.;\n"
    "  } else if (pal==4) {\n" /* ocean: coefficients r*5, g*2, b*0.5 */
    "    a=vec3(min(255.,i*5.),min(255.,i*2.),min(255.,i*.5))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*5.),min(255.,(i+1.)*2.),min(255.,(i+1.)*.5))/255.;\n"
    "  } else {\n" /* inferno: coefficients r*0.5, g*2, b*8 */
    "    a=vec3(min(255.,i*.5),min(255.,i*2.),min(255.,i*8.))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*.5),min(255.,(i+1.)*2.),min(255.,(i+1.)*8.))/255.;\n"
    "  }\n"
    "  return mix(a, b, fract(fi));\n"
    "}\n"

    "void main() {\n"
    "  int m = int(u_iters), i = 0;\n"
    "  float mag2 = 0.0;\n"

    /* high-precision path: dekker double-single arithmetic.
     * reconstructs the pixel coordinate as a (hi, lo) pair to preserve
     * mantissa bits that are lost when adding a small pixel offset to a
     * large center coordinate at high zoom levels. */
    "  if (u_high_precision > 0.5) {\n"
    "    vec2 uv_dx = vec2(uv.x - 0.5, 0.0);\n"
    "    vec2 uv_dy = vec2(0.5 - uv.y, 0.0);\n"
    "    vec2 zoom_a = vec2(u_zoom, 0.0);\n"
    "    vec2 aspect_a = vec2(u_aspect, 0.0);\n"
    "    vec2 cx = vec2(u_center_hi.x, u_center_lo.x);\n"
    "    vec2 cy = vec2(u_center_hi.y, u_center_lo.y);\n"
    "    vec2 px = ds_add(ds_mul(ds_mul(uv_dx, zoom_a), aspect_a), cx);\n"
    "    vec2 py = ds_add(ds_mul(uv_dy, zoom_a), cy);\n"
    "    vec2 c_val_x = (u_is_julia > 0.5) ? vec2(u_julia_c.x, 0.0) : px;\n"
    "    vec2 c_val_y = (u_is_julia > 0.5) ? vec2(u_julia_c.y, 0.0) : py;\n"
    "    vec2 zx = (u_is_julia > 0.5) ? px : vec2(0.0);\n"
    "    vec2 zy = (u_is_julia > 0.5) ? py : vec2(0.0);\n"
    "    for (i=0; i<2000; i++) {\n"
    "      if (i>=m) break;\n"
    "      vec2 x2 = ds_mul(zx, zx);\n"
    "      vec2 y2 = ds_mul(zy, zy);\n"
    "      mag2 = x2.x + y2.x;\n"
    "      if (mag2 > 100.0) break;\n" /* escape radius = 10, so escape_sq = 100 */
    "      vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);\n"
    "      vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);\n"
    "      zx = zx_new; zy = zy_new;\n"
    "    }\n"

    /* standard 32-bit path — fast for low zoom levels where float precision is sufficient. */
    "  } else {\n"
    "    vec2 center = u_center_hi + u_center_lo;\n"
    "    vec2 p = vec2((uv.x-0.5)*u_zoom*u_aspect+center.x, (0.5-uv.y)*u_zoom+center.y);\n"
    "    vec2 c_val = (u_is_julia>0.5) ? u_julia_c : p;\n"
    "    vec2 z = (u_is_julia>0.5) ? p : vec2(0.0);\n"
    /* cardioid and period-2 bulb rejection (mandelbrot only).
     * matches the checks in mandelbrot.c — points confirmed inside the main
     * set skip the iteration loop entirely, saving significant gpu cycles
     * in the dense black regions of the fractal. */
    "    if (u_is_julia < 0.5) {\n"
    "      float cr = p.x - 0.25, ci2 = p.y*p.y;\n"
    "      float q = cr*cr + ci2;\n"
    "      if (q*(q+cr) <= 0.25*ci2) { i = m; }\n"         /* main cardioid */
    "      else { float cr1 = p.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }\n" /* period-2 bulb */
    "    }\n"
    "    for (; i<2000; i++) {\n"
    "      if (i>=m) break;\n"
    "      float x2=z.x*z.x, y2=z.y*z.y;\n"
    "      mag2 = x2+y2;\n"
    "      if (mag2 > 100.0) break;\n" /* escape radius = 10, so escape_sq = 100 */
    "      z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);\n"
    "    }\n"
    "  }\n"

    /* output: black for points inside the set, smooth color otherwise.
     * smooth coloring formula: i + 2 - log2(log(|z|^2)) removes the
     * discrete iteration boundary, producing continuous color gradients. */
    "  if (i>=m) { color=vec4(0,0,0,1); }\n"
    "  else {\n"
    "    float s = float(i)+2.0-log2(log(max(1.0,mag2)));\n"
    "    color = vec4(lut_color(max(0.0,s), int(u_palette)), 1.0);\n"
    "  }\n"
    "}\n";

#endif