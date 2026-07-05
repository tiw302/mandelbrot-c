# merge_shaders.py
#
# script to merge separate glsl shader files into a single sokol-shdc compatible file.

import os

with open('shaders/desktop_gpu_vs.glsl', 'r') as f:
    vs_code = f.read().splitlines()[1:] # skip #version

with open('shaders/desktop_gpu_fs_cpu.glsl', 'r') as f:
    cpu_code = f.read().splitlines()[1:]

with open('shaders/desktop_gpu_fs_gpu.glsl', 'r') as f:
    gpu_code = f.read().splitlines()[1:]

# process vs
vs_final = "@vs dg_vs\n"
for line in vs_code:
    if line.startswith('layout'):
        vs_final += line.split(')')[1].strip() + "\n"
    else:
        vs_final += line + "\n"
vs_final += "@end\n\n"

# process cpu fs
cpu_final = "@fs dg_fs_cpu\n"
cpu_final += "layout(binding=0) uniform texture2D tex;\nlayout(binding=0) uniform sampler smp;\n"
for line in cpu_code:
    if line.startswith('uniform sampler2D'): continue
    if 'texture(tex' in line:
        line = line.replace('texture(tex', 'texture(sampler2D(tex, smp)')
    cpu_final += line + "\n"
cpu_final += "@end\n\n"

# process gpu fs
gpu_final = "@fs dg_fs_gpu\n"
gpu_final += "layout(binding=0) uniform params_t {\n"

gpu_body = ""
in_uniforms = True
for line in gpu_code:
    if line.startswith('uniform sampler2D'):
        in_uniforms = False
        gpu_final += "};\n"
        gpu_final += "layout(binding=0) uniform texture2D u_orbit;\nlayout(binding=0) uniform sampler u_orbit_smp;\n"
        continue
    if line.startswith('in vec2'):
        if in_uniforms:
            gpu_final += "};\n"
            in_uniforms = False
    
    if line.startswith('uniform'):
        gpu_final += "    " + line.replace('uniform ', '') + "\n"
    else:
        if 'texture(u_orbit' in line:
            line = line.replace('texture(u_orbit', 'texture(sampler2D(u_orbit, u_orbit_smp)')
        gpu_body += line + "\n"

gpu_final += gpu_body
gpu_final += "@end\n\n"

prog_final = "@program desktop_gpu dg_vs dg_fs_gpu\n@program desktop_cpu dg_vs dg_fs_cpu\n"

with open('shaders/mandelbrot.glsl', 'w') as f:
    f.write(vs_final + cpu_final + gpu_final + prog_final)

print("Merged to shaders/mandelbrot.glsl")
