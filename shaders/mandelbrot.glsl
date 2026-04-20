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
    vec2 center;
    float zoom;
    float iterations;
    float aspect_ratio;
};

in vec2 uv;
out vec4 frag_color;

// simple hsv to rgb
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    // map uv to complex plane
    float x = (uv.x - 0.5) * zoom * aspect_ratio + center.x;
    float y = (uv.y - 0.5) * zoom + center.y;

    vec2 c = vec2(x, y);
    vec2 z = vec2(0.0);
    float iter = 0.0;
    
    // mandelbrot iteration
    for (float i = 0.0; i < 1000.0; i++) {
        if (i >= iterations) break;
        
        float x2 = z.x * z.x;
        float y2 = z.y * z.y;
        
        if (x2 + y2 > 4.0) break;
        
        z = vec2(x2 - y2 + c.x, 2.0 * z.x * z.y + c.y);
        iter++;
    }

    if (iter >= iterations) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        // smooth coloring
        float dist = length(z);
        float smooth_iter = iter + 1.0 - log2(log(dist + 0.0001));
        
        float h = fract(smooth_iter * 0.05);
        float s = 0.8;
        float v = 1.0;
        
        frag_color = vec4(hsv2rgb(vec3(h, s, v)), 1.0);
    }
}
@end

@program mandelbrot vs fs
