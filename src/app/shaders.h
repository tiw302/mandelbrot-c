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
    "uniform vec2 u_center_hi;\n"
    "uniform vec2 u_center_lo;\n"
    "uniform vec2 u_julia_c;\n"
    "uniform float u_zoom;\n"
    "uniform float u_iters;\n"
    "uniform float u_aspect;\n"
    "uniform float u_is_julia;\n"
    "uniform float u_palette;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"

    "vec3 lut_color(float fi, int pal) {\n"
    "    float i = fi;\n"
    "    vec3 a, b;\n"
    "    if (pal == 0) {\n"  /* sine wave: Swapped phases (4,2,0) to match CPU Mint appearance */
    "        a = vec3(sin(0.1*i+4.0)*127.0+128.0, sin(0.1*i+2.0)*127.0+128.0, sin(0.1*i+0.0)*127.0+128.0) / 255.0;\n"
    "        b = vec3(sin(0.1*(i+1.0)+4.0)*127.0+128.0, sin(0.1*(i+1.0)+2.0)*127.0+128.0, sin(0.1*(i+1.0)+0.0)*127.0+128.0) / 255.0;\n"
    "    } else if (pal == 1) {\n"  /* grayscale */
    "        float v = mod(i, 256.0) / 255.0;\n"
    "        float v2 = mod(i+1.0, 256.0) / 255.0;\n"
    "        a = vec3(v); b = vec3(v2);\n"
    "    } else if (pal == 2) {\n"  /* fire (swapped to match CPU) */
    "        a = vec3(min(255.0,i*1.0), min(255.0,i*2.0), min(255.0,i*4.0)) / 255.0;\n"
    "        b = vec3(min(255.0,(i+1.0)*1.0), min(255.0,(i+1.0)*2.0), min(255.0,(i+1.0)*4.0)) / 255.0;\n"
    "    } else if (pal == 3) {\n"  /* electric (swapped to match CPU) */
    "        a = vec3(min(255.0,i*8.0), min(255.0,i*4.0), min(255.0,i*1.0)) / 255.0;\n"
    "        b = vec3(min(255.0,(i+1.0)*8.0), min(255.0,(i+1.0)*4.0), min(255.0,(i+1.0)*1.0)) / 255.0;\n"
    "    } else if (pal == 4) {\n"  /* ocean (swapped to match CPU) */
    "        a = vec3(min(255.0,i*5.0), min(255.0,i*2.0), min(255.0,i*0.5)) / 255.0;\n"
    "        b = vec3(min(255.0,(i+1.0)*5.0), min(255.0,(i+1.0)*2.0), min(255.0,(i+1.0)*0.5)) / 255.0;\n"
    "    } else {\n"  /* inferno (swapped to match CPU) */
    "        a = vec3(min(255.0,i*0.5), min(255.0,i*2.0), min(255.0,i*8.0)) / 255.0;\n"
    "        b = vec3(min(255.0,(i+1.0)*0.5), min(255.0,(i+1.0)*2.0), min(255.0,(i+1.0)*8.0)) / 255.0;\n"
    "    }\n"
    "    return mix(a, b, fract(fi));\n"
    "}\n"

    "void main() {\n"
    "    vec2 center = u_center_hi + u_center_lo;\n"
    "    vec2 p = vec2((uv.x - 0.5) * u_zoom * u_aspect + center.x,\n"
    "                  (0.5 - uv.y) * u_zoom + center.y);\n"
    "    vec2 c_val = (u_is_julia > 0.5) ? u_julia_c : p;\n"
    "    vec2 z = (u_is_julia > 0.5) ? p : vec2(0.0);\n"
    "    float escape_sq = 100.0;\n"
    "    int m = int(u_iters), i = 0;\n"
    "    for (i = 0; i < 2000; i++) {\n"
    "        if (i >= m) break;\n"
    "        float x2 = z.x * z.x, y2 = z.y * z.y;\n"
    "        if (x2 + y2 > escape_sq) break;\n"
    "        z = vec2(x2 - y2 + c_val.x, 2.0 * z.x * z.y + c_val.y);\n"
    "    }\n"
    "    if (i >= m) {\n"
    "        frag_color = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "    } else {\n"
    "        float mag_sq = z.x*z.x + z.y*z.y;\n"
    "        float s = float(i) + 2.0 - log2(log(max(1.0, mag_sq)));\n"
    "        frag_color = vec4(lut_color(max(0.0, s), int(u_palette)), 1.0);\n"
    "    }\n"
    "}\n";

#endif
