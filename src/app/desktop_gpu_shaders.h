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
 *   - mandelbrot and julia set rendering (switched via u_fractal_type uniform)
 *   - 9 color palettes matching color.c exactly (switched via u_palette uniform)
 *   - 32-bit and dekker double-single 64-bit precision (switched via u_high_precision uniform)
 *   - cardioid and period-2 bulb early rejection in mandelbrot mode
 *   - smooth (fractional) iteration coloring to eliminate banding
 */

// vertex shader — shared between cpu and gpu fragment paths.
// maps clip-space quad positions to uv coordinates for the fragment stage.
static const char* dg_vs =
    "#version 330\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv_in;"
    "out vec2 uv;"
    "void main() { gl_Position = vec4(pos,0.0,1.0); uv = uv_in; }";

// cpu-mode fragment shader — used when rendering on the cpu thread pool.
// the cpu fills a buffer with argb8888 pixels; this shader simply
// samples that texture and outputs it to the screen.
static const char* dg_fs_cpu =
    "#version 330\n"
    "uniform sampler2D tex; in vec2 uv; out vec4 color;"
    "void main() { color = texture(tex, uv); }";

/* gpu-mode fragment shader — computes the full fractal on the gpu.
 *
 * uniforms:
 *   u_center_hi / u_center_lo  — hi-lo split of the double-precision view center
 *   u_julia_c_hi/u_julia_c_lo  — hi-lo split of the julia set c parameter
 *   u_zoom                     — zoom level (complex plane units per screen height)
 *   u_iters                    — maximum iteration count
 *   u_aspect                   — window aspect ratio (width / height)
 *   u_fractal_type             — 0.0 = mandelbrot, 1.0 = julia, 2.0 = burning ship
 *   u_palette                  — palette index 0-8 (matches PALETTE_NAMES in color.c)
 *   u_high_precision           — 0.0 = standard 32-bit, 1.0 = dekker double-single
 */
static const char* dg_fs_gpu =
    "#version 400\n"
    "uniform vec2 u_center_hi; uniform vec2 u_center_lo;"
    "uniform vec2 u_julia_c_hi; uniform vec2 u_julia_c_lo; uniform float u_zoom; uniform float "
    "u_iters; uniform float u_aspect;"
    "uniform float u_fractal_type; uniform float u_palette; uniform float u_high_precision;"
    "in vec2 uv; out vec4 color;\n"

    /* dekker double-single addition (knuth twosum algorithm).
     * computes (dsa + dsb) as a double-single pair with no precision loss.
     * 'precise' keyword prevents compiler optimizations that would break error compensation. */
    "vec2 ds_add(vec2 dsa, vec2 dsb) {\n"
    "  precise float t1 = dsa.x + dsb.x;\n"
    "  precise float e  = t1 - dsa.x;\n"
    "  precise float t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;\n"
    "  precise float t3 = t1 + t2;\n"
    "  return vec2(t3, t2 - (t3 - t1));\n"
    "}\n"

    /* dekker double-single multiplication (veltkamp split).
     * splits each operand into high/low 12-bit halves before multiplying,
     * recovering the full 48-bit product (near double precision) on 32-bit gpus. */
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

    /* color palette lookup — exact replica of the cpu-side lut logic.
     * fi is the fractional iteration count. interpolates between discrete
     * color steps to ensure smooth gradients. */
    "vec3 lut_color(float fi, int pal) {\n"
    "  float i = fi; vec3 a,b;\n"
    "  if (pal==0) {\n"  // sine wave
    "    a=vec3(sin(0.1*i+4.0)*127.+128., sin(0.1*i+2.0)*127.+128., "
    "sin(0.1*i+0.0)*127.+128.)/255.;\n"
    "    b=vec3(sin(0.1*(i+1.0)+4.0)*127.+128., sin(0.1*(i+1.0)+2.0)*127.+128., "
    "sin(0.1*(i+1.0)+0.0)*127.+128.)/255.;\n"
    "  } else if (pal==1) {\n"  // grayscale
    "    a=vec3(mod(i,256.)/255.); b=vec3(mod(i+1.,256.)/255.);\n"
    "  } else if (pal==2) {\n"  // fire
    "    a=vec3(255.-abs(mod(i*1.,510.)-255.), 255.-abs(mod(i*2.,510.)-255.), "
    "255.-abs(mod(i*4.,510.)-255.))/255.;\n"
    "    b=vec3(255.-abs(mod((i+1.)*1.,510.)-255.), 255.-abs(mod((i+1.)*2.,510.)-255.), "
    "255.-abs(mod((i+1.)*4.,510.)-255.))/255.;\n"
    "  } else if (pal==3) {\n"  // electric
    "    a=vec3(255.-abs(mod(i*8.,510.)-255.), 255.-abs(mod(i*4.,510.)-255.), "
    "255.-abs(mod(i*1.,510.)-255.))/255.;\n"
    "    b=vec3(255.-abs(mod((i+1.)*8.,510.)-255.), 255.-abs(mod((i+1.)*4.,510.)-255.), "
    "255.-abs(mod((i+1.)*1.,510.)-255.))/255.;\n"
    "  } else if (pal==4) {\n"  // ocean
    "    a=vec3(255.-abs(mod(i*5.,510.)-255.), 255.-abs(mod(i*2.,510.)-255.), "
    "255.-abs(mod(i*.5,510.)-255.))/255.;\n"
    "    b=vec3(255.-abs(mod((i+1.)*5.,510.)-255.), 255.-abs(mod((i+1.)*2.,510.)-255.), "
    "255.-abs(mod((i+1.)*.5,510.)-255.))/255.;\n"
    "  } else if (pal==5) {\n"  // inferno
    "    a=vec3(255.-abs(mod(i*.5,510.)-255.), 255.-abs(mod(i*2.,510.)-255.), "
    "255.-abs(mod(i*8.,510.)-255.))/255.;\n"
    "    b=vec3(255.-abs(mod((i+1.)*.5,510.)-255.), 255.-abs(mod((i+1.)*2.,510.)-255.), "
    "255.-abs(mod((i+1.)*8.,510.)-255.))/255.;\n"
    "  } else if (pal==6) {\n"  // viridis
    "    // ping-pong wrap for gpu shader\n"
    "    float t1 = 1.0 - abs(mod(i/256., 2.0) - 1.0); float t2 = 1.0 - abs(mod((i+1.)/256., 2.0) "
    "- 1.0);\n"
    "    a=vec3(0.267+t1*(0.993*t1-0.260), 0.004+t1*(1.490-t1*0.494), "
    "0.329+t1*(1.268*t1*t1-0.680*t1-0.259));\n"
    "    b=vec3(0.267+t2*(0.993*t2-0.260), 0.004+t2*(1.490-t2*0.494), "
    "0.329+t2*(1.268*t2*t2-0.680*t2-0.259));\n"
    "  } else if (pal==7) {\n"  // plasma
    "    float t1 = 1.0 - abs(mod(i/256., 2.0) - 1.0); float t2 = 1.0 - abs(mod((i+1.)/256., 2.0) "
    "- 1.0);\n"
    "    a=vec3(0.050+t1*(2.735-t1*1.785), max(0.,t1*(1.580*t1-0.580)), "
    "max(0.,0.530+t1*(0.750-t1*1.280)));\n"
    "    b=vec3(0.050+t2*(2.735-t2*1.785), max(0.,t2*(1.580*t2-0.580)), "
    "max(0.,0.530+t2*(0.750-t2*1.280)));\n"
    "  } else {\n"  // twilight
    "    float t1 = fract(i/128.); float t2 = fract((i+1.)/128.);\n"
    "    a=vec3(0.5+0.5*sin(6.283*t1), 0.3+0.2*sin(6.283*t1+2.094), 0.5+0.5*sin(6.283*t1+4.189));\n"
    "    b=vec3(0.5+0.5*sin(6.283*t2), 0.3+0.2*sin(6.283*t2+2.094), 0.5+0.5*sin(6.283*t2+4.189));\n"
    "  }\n"
    "  return mix(a, b, fract(fi));\n"
    "}\n"

    "void main() {\n"
    "  int m = int(u_iters), i = 0;\n"
    "  float mag2 = 0.0;\n"

    /* path 1: dekker double-single (64-bit emulation).
     * solves pixelation issues when zooming deep into the set by using
     * two 32-bit floats to store a high-precision coordinate. */
    "  if (u_high_precision > 0.5) {\n"
    "    vec2 uv_dx = vec2(uv.x - 0.5, 0.0);\n"
    "    vec2 uv_dy = vec2(0.5 - uv.y, 0.0);\n"
    "    vec2 zoom_a = vec2(u_zoom, 0.0);\n"
    "    vec2 aspect_a = vec2(u_aspect, 0.0);\n"
    "    vec2 cx = vec2(u_center_hi.x, u_center_lo.x);\n"
    "    vec2 cy = vec2(u_center_hi.y, u_center_lo.y);\n"
    "    vec2 px = ds_add(ds_mul(ds_mul(uv_dx, zoom_a), aspect_a), cx);\n"
    "    vec2 py = ds_add(ds_mul(uv_dy, zoom_a), cy);\n"
    "    vec2 c_val_x = (u_fractal_type == 1.0) ? vec2(u_julia_c_hi.x, u_julia_c_lo.x) : px;\n"
    "    vec2 c_val_y = (u_fractal_type == 1.0) ? vec2(u_julia_c_hi.y, u_julia_c_lo.y) : py;\n"
    "    vec2 zx = (u_fractal_type == 1.0) ? px : vec2(0.0);\n"
    "    vec2 zy = (u_fractal_type == 1.0) ? py : vec2(0.0);\n"
    "    if (u_fractal_type < 0.5) {\n"  // cardioid rejection (mandelbrot)
    "      float cr = px.x - 0.25, ci2 = py.x*py.x, q = cr*cr+ci2;\n"
    "      if (q*(q+cr) <= 0.25*ci2) { i = m; }\n"
    "      else { float cr1 = px.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }\n"
    "    }\n"
    "    for (i=0; i<2000; i++) {\n"
    "      if (i>=m) break;\n"
    "      vec2 x2 = ds_mul(zx, zx);\n"
    "      vec2 y2 = ds_mul(zy, zy);\n"
    "      mag2 = x2.x + y2.x;\n"
    "      if (mag2 > 100.0) break;\n"
    "      if (u_fractal_type > 1.5) {\n"  // burning ship
    "        vec2 abs_zx = (zx.x < 0.0) ? vec2(-zx.x, -zx.y) : zx;\n"
    "        vec2 abs_zy = (zy.x < 0.0) ? vec2(-zy.x, -zy.y) : zy;\n"
    "        vec2 zy_new = ds_add(ds_add(ds_mul(abs_zx, abs_zy), ds_mul(abs_zx, abs_zy)), "
    "c_val_y);\n"
    "        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);\n"
    "        zx = zx_new; zy = zy_new;\n"
    "      } else {\n"  // mandelbrot / julia
    "        vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);\n"
    "        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);\n"
    "        zx = zx_new; zy = zy_new;\n"
    "      }\n"
    "    }\n"

    /* path 2: standard float precision.
     * high-speed path for low zoom levels where 32-bit float mantissa is enough. */
    "  } else {\n"
    "    vec2 center = u_center_hi + u_center_lo;\n"
    "    vec2 p = vec2((uv.x-0.5)*u_zoom*u_aspect+center.x, (0.5-uv.y)*u_zoom+center.y);\n"
    "    vec2 c_val = (u_fractal_type == 1.0) ? (u_julia_c_hi + u_julia_c_lo) : p;\n"
    "    vec2 z = (u_fractal_type == 1.0) ? p : vec2(0.0);\n"
    "    if (u_fractal_type < 0.5) {\n"  // cardioid rejection
    "      float cr = p.x - 0.25, ci2 = p.y*p.y, q = cr*cr + ci2;\n"
    "      if (q*(q+cr) <= 0.25*ci2) { i = m; }\n"
    "      else { float cr1 = p.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }\n"
    "    }\n"
    "    for (; i<2000; i++) {\n"
    "      if (i>=m) break;\n"
    "      float x2=z.x*z.x, y2=z.y*z.y;\n"
    "      mag2 = x2+y2;\n"
    "      if (mag2 > 100.0) break;\n"
    "      if (u_fractal_type > 1.5) {\n"
    "        z = vec2(x2-y2+c_val.x, 2.0*abs(z.x)*abs(z.y)+c_val.y);\n"
    "      } else {\n"
    "        z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);\n"
    "      }\n"
    "    }\n"
    "  }\n"

    /* final output coloring */
    "  if (i>=m) { color=vec4(0,0,0,1); }\n"
    "  else {\n"
    "    float s = float(i)+2.0-log2(log(max(1.0,mag2)));\n"
    "    color = vec4(lut_color(max(0.0,s), int(u_palette)), 1.0);\n"
    "  }\n"
    "}\n";

#endif
