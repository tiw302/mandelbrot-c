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

/* gpu shader: all 5 palettes implemented in GLSL to match cpu color.c */
static const char* fs_gpu_src =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform vec2 u_center;\n"
    "uniform float u_zoom;\n"
    "uniform float u_iters;\n"
    "uniform float u_aspect;\n"
    "uniform vec2 u_julia_c;\n"
    "uniform float u_is_julia;\n"
    "uniform float u_palette;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"

    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
    "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
    "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
    "}\n"

    "vec3 palette_color(float s, int pal) {\n"
    /* 0: smooth - classic smooth hue cycle */
    "    if (pal == 0) return hsv2rgb(vec3(fract(s * 0.05), 0.8, 1.0));\n"
    /* 1: zebra - alternating black/white bands */
    "    if (pal == 1) {\n"
    "        float b = mod(floor(s), 2.0);\n"
    "        return vec3(b, b, b);\n"
    "    }\n"
    /* 2: neon - vivid electric hues at high saturation */
    "    if (pal == 2) return hsv2rgb(vec3(fract(s * 0.07 + 0.6), 1.0, 1.0));\n"
    /* 3: fire - red → orange → yellow → white */
    "    if (pal == 3) {\n"
    "        float t = fract(s * 0.05);\n"
    "        return vec3(\n"
    "            min(1.0, t * 3.0),\n"
    "            min(1.0, max(0.0, t * 3.0 - 1.0)),\n"
    "            min(1.0, max(0.0, t * 3.0 - 2.0))\n"
    "        );\n"
    "    }\n"
    /* 4: ice - cool blue gradient */
    "    float t2 = fract(s * 0.05);\n"
    "    return vec3(t2 * 0.3, t2 * 0.7, 1.0);\n"
    "}\n"

    "void main() {\n"
    "    vec2 p = vec2((uv.x - 0.5) * u_zoom * u_aspect + u_center.x,\n"
    "                  (0.5 - uv.y) * u_zoom + u_center.y);\n"
    "    vec2 c_val = (u_is_julia > 0.5) ? u_julia_c : p;\n"
    "    vec2 z = (u_is_julia > 0.5) ? p : vec2(0.0);\n"
    "    int m = int(u_iters);\n"
    "    int i = 0;\n"
    "    for (i = 0; i < 2000; i++) {\n"
    "        if (i >= m) break;\n"
    "        float x2 = z.x * z.x, y2 = z.y * z.y;\n"
    "        if (x2 + y2 > 4.0) break;\n"
    "        z = vec2(x2 - y2 + c_val.x, 2.0 * z.x * z.y + c_val.y);\n"
    "    }\n"
    "    if (i >= m) {\n"
    "        frag_color = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "    } else {\n"
    "        float dist = length(z);\n"
    "        float s = float(i) + 1.0 - log2(log(dist + 0.00001));\n"
    "        frag_color = vec4(palette_color(s, int(u_palette)), 1.0);\n"
    "    }\n"
    "}\n";

#endif // SHADERS_H
