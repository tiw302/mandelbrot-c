# gen_shaders.py
#
# embedded glsl shaders for the webassembly webgl2 pipeline.
# generated automatically from desktop glsl shaders by scripts/gen_shaders.py.

import os

def escape_glsl(glsl_code):
    escaped = []
    for line in glsl_code.splitlines():
        escaped_line = line.replace('\\', '\\\\').replace('"', '\\"')
        escaped.append(f'    "{escaped_line}\\n"')
    return "\n".join(escaped)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    shaders_dir = os.path.join(project_dir, "shaders")
    output_file = os.path.join(project_dir, "src", "app", "shaders.h")

    # 1. read and translate vertex shader
    with open(os.path.join(shaders_dir, "desktop_gpu_vs.glsl"), "r") as f:
        vs_src = f.read()
    vs_web = vs_src.replace("#version 330", "#version 300 es\nprecision highp float;")
    vs_web = vs_web.replace("layout(location=0) in vec2 pos;", "in vec2 pos;")
    vs_web = vs_web.replace("layout(location=1) in vec2 uv_in;", "in vec2 uv_in;")

    # 2. read and translate cpu blit fragment shader
    with open(os.path.join(shaders_dir, "desktop_gpu_fs_cpu.glsl"), "r") as f:
        fs_cpu_src = f.read()
    fs_cpu_web = fs_cpu_src.replace("#version 330", "#version 300 es\nprecision mediump float;")

    # 3. read and translate main gpu render fragment shader
    with open(os.path.join(shaders_dir, "desktop_gpu_fs_gpu.glsl"), "r") as f:
        fs_gpu_src = f.read()
    fs_gpu_web = fs_gpu_src.replace("#version 330", "#version 300 es\nprecision highp float;\nprecision highp int;")

    # 4. generate shaders.h file content
    header_content = f"""/* shaders.h
 *
 * embedded glsl shaders for the webassembly webgl2 pipeline.
 * generated automatically from desktop GLSL shaders by scripts/gen_shaders.py.
 */

#ifndef SHADERS_H
#define SHADERS_H

static const char* vs_src =
{escape_glsl(vs_web)};

static const char* fs_cpu_src =
{escape_glsl(fs_cpu_web)};

static const char* fs_gpu_src =
{escape_glsl(fs_gpu_web)};

#endif // shaders_h
"""

    with open(output_file, "w") as f:
        f.write(header_content)
    print("generated src/app/shaders.h successfully.")

if __name__ == "__main__":
    main()
