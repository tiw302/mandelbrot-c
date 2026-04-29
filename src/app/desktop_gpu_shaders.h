#ifndef DESKTOP_GPU_SHADERS_H
#define DESKTOP_GPU_SHADERS_H

static const char* dg_vs =
    "#version 330\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv_in;"
    "out vec2 uv;"
    "void main() { gl_Position = vec4(pos,0.0,1.0); uv = uv_in; }";

static const char* dg_fs_cpu =
    "#version 330\n"
    "uniform sampler2D tex; in vec2 uv; out vec4 color;"
    "void main() { color = texture(tex, uv); }";

static const char* dg_fs_gpu =
    "#version 330\n"
    "uniform vec2 u_center_hi; uniform vec2 u_center_lo;"
    "uniform vec2 u_julia_c; uniform float u_zoom; uniform float u_iters; uniform float u_aspect;"
    "uniform float u_is_julia; uniform float u_palette; uniform float u_high_precision;"
    "in vec2 uv; out vec4 color;\n"

    "vec2 ds_add(vec2 dsa, vec2 dsb) {\n"
    "  float t1 = dsa.x + dsb.x;\n"
    "  float e = t1 - dsa.x;\n"
    "  float t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;\n"
    "  float t3 = t1 + t2;\n"
    "  return vec2(t3, t2 - (t3 - t1));\n"
    "}\n"

    "vec2 ds_split(float a) {\n"
    "  float c = 4097.0 * a;\n"
    "  float abig = c - a;\n"
    "  float ahi = c - abig;\n"
    "  float alo = a - ahi;\n"
    "  return vec2(ahi, alo);\n"
    "}\n"

    "vec2 ds_mul(vec2 a, vec2 b) {\n"
    "  float p1 = a.x * b.x;\n"
    "  vec2 a_s = ds_split(a.x);\n"
    "  vec2 b_s = ds_split(b.x);\n"
    "  float err = ((a_s.x * b_s.x - p1) + a_s.y * b_s.x + a_s.x * b_s.y) + a_s.y * b_s.y;\n"
    "  float p2 = a.y * b.x + a.x * b.y + err;\n"
    "  float s1 = p1 + p2;\n"
    "  return vec2(s1, p2 - (s1 - p1));\n"
    "}\n"

    "vec3 lut_color(float fi, int pal) {\n"
    "  float i = fi; vec3 a,b;\n"
    "  if (pal==0) {\n" /* sine wave: swapped phases (4,2,0) for mint */
    "    a=vec3(sin(0.1*i+4.0)*127.+128., sin(0.1*i+2.0)*127.+128., "
    "sin(0.1*i+0.0)*127.+128.)/255.;\n"
    "    b=vec3(sin(0.1*(i+1.0)+4.0)*127.+128., sin(0.1*(i+1.0)+2.0)*127.+128., "
    "sin(0.1*(i+1.0)+0.0)*127.+128.)/255.;\n"
    "  } else if (pal==1) {\n"
    "    a=vec3(mod(i,256.)/255.); b=vec3(mod(i+1.,256.)/255.);\n"
    "  } else if (pal==2) {\n" /* fire swapped */
    "    a=vec3(min(255.,i*1.),min(255.,i*2.),min(255.,i*4.))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*1.),min(255.,(i+1.)*2.),min(255.,(i+1.)*4.))/255.;\n"
    "  } else if (pal==3) {\n" /* electric swapped */
    "    a=vec3(min(255.,i*8.),min(255.,i*4.),min(255.,i*1.))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*8.),min(255.,(i+1.)*4.),min(255.,(i+1.)*1.))/255.;\n"
    "  } else if (pal==4) {\n" /* ocean swapped */
    "    a=vec3(min(255.,i*5.),min(255.,i*2.),min(255.,i*.5))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*5.),min(255.,(i+1.)*2.),min(255.,(i+1.)*.5))/255.;\n"
    "  } else {\n" /* inferno swapped */
    "    a=vec3(min(255.,i*.5),min(255.,i*2.),min(255.,i*8.))/255.;\n"
    "    b=vec3(min(255.,(i+1.)*.5),min(255.,(i+1.)*2.),min(255.,(i+1.)*8.))/255.;\n"
    "  }\n"
    "  return mix(a, b, fract(fi));\n"
    "}\n"

    "void main() {\n"
    "  int m = int(u_iters), i = 0;\n"
    "  float mag2 = 0.0;\n"
    "  if (u_high_precision > 0.5) {\n"
    "    vec2 uv_dx = ds_split(uv.x - 0.5);\n"
    "    vec2 uv_dy = ds_split(0.5 - uv.y);\n"
    "    vec2 zoom_a = ds_split(u_zoom);\n"
    "    vec2 aspect_a = ds_split(u_aspect);\n"
    "    vec2 cx = vec2(u_center_hi.x, u_center_lo.x);\n"
    "    vec2 cy = vec2(u_center_hi.y, u_center_lo.y);\n"
    "    vec2 px = ds_add(ds_mul(ds_mul(uv_dx, zoom_a), aspect_a), cx);\n"
    "    vec2 py = ds_add(ds_mul(uv_dy, zoom_a), cy);\n"
    "    vec2 c_val_x = (u_is_julia > 0.5) ? ds_split(u_julia_c.x) : px;\n"
    "    vec2 c_val_y = (u_is_julia > 0.5) ? ds_split(u_julia_c.y) : py;\n"
    "    vec2 zx = (u_is_julia > 0.5) ? px : vec2(0.0);\n"
    "    vec2 zy = (u_is_julia > 0.5) ? py : vec2(0.0);\n"
    "    for (i=0; i<2000; i++) {\n"
    "      if (i>=m) break;\n"
    "      vec2 x2 = ds_mul(zx, zx);\n"
    "      vec2 y2 = ds_mul(zy, zy);\n"
    "      mag2 = x2.x + y2.x;\n"
    "      if (mag2 > 100.0) break;\n"
    "      vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);\n"
    "      vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);\n"
    "      zx = zx_new; zy = zy_new;\n"
    "    }\n"
    "  } else {\n"
    "    vec2 center = u_center_hi + u_center_lo;\n"
    "    vec2 p = vec2((uv.x-0.5)*u_zoom*u_aspect+center.x, (0.5-uv.y)*u_zoom+center.y);\n"
    "    vec2 c_val = (u_is_julia>0.5) ? u_julia_c : p;\n"
    "    vec2 z = (u_is_julia>0.5) ? p : vec2(0.0);\n"
    "    for (i=0; i<2000; i++) {\n"
    "      if (i>=m) break;\n"
    "      float x2=z.x*z.x, y2=z.y*z.y;\n"
    "      mag2 = x2+y2;\n"
    "      if (mag2 > 100.0) break;\n"
    "      z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);\n"
    "    }\n"
    "  }\n"
    "  if (i>=m) { color=vec4(0,0,0,1); }\n"
    "  else {\n"
    "    float s = float(i)+2.0-log2(log(max(1.0,mag2)));\n"
    "    color = vec4(lut_color(max(0.0,s), int(u_palette)), 1.0);\n"
    "  }\n"
    "}\n";

#endif
