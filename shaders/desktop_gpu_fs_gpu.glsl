#version 400
uniform vec2 u_center_hi;
uniform vec2 u_center_lo;
uniform vec2 u_julia_c_hi;
uniform vec2 u_julia_c_lo;
uniform float u_zoom;
uniform float u_zoom_lo;  // low part of zoom for Dekker hi-lo d0 calculation
uniform float u_iters;
uniform float u_aspect;
uniform vec2 u_ref_offset;
uniform float u_fractal_type;  // 0=mandelbrot, 1=julia, 2=burning_ship, 3=tricorn, 4=celtic, 5=buffalo
uniform float u_palette;
uniform float u_high_precision;
uniform float u_use_perturbation;
uniform float u_orbit_len;
uniform sampler2D u_orbit;
in vec2 uv;
out vec4 color;

// dekker double-single addition (knuth twosum algorithm).
// computes (dsa + dsb) as a double-single pair with no precision loss.
// 'precise' keyword prevents compiler optimizations that would break error compensation.
vec2 ds_add(vec2 dsa, vec2 dsb) {
  precise float t1 = dsa.x + dsb.x;
  precise float e  = t1 - dsa.x;
  precise float t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;
  precise float t3 = t1 + t2;
  return vec2(t3, t2 - (t3 - t1));
}

// dekker double-single multiplication (veltkamp split).
// splits each operand into high/low 12-bit halves before multiplying,
// recovering the full 48-bit product (near double precision) on 32-bit gpus.
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

// dekker double-single squaring.
// splits the operand only once to save multiple float operations
// compared to general double-single multiplication.
vec2 ds_sqr(vec2 dsa) {
  precise float cona = dsa.x * 4097.0;
  precise float a1 = cona - (cona - dsa.x);
  precise float a2 = dsa.x - a1;
  precise float c11 = dsa.x * dsa.x;
  precise float c21 = a2*a2 + (2.0 * a1*a2 + (a1*a1 - c11));
  precise float c2  = 2.0 * dsa.x * dsa.y;
  precise float t1  = c11 + c2;
  precise float e   = t1 - c11;
  precise float t2  = dsa.y * dsa.y + ((c2 - e) + (c11 - (t1 - e))) + c21;
  precise float t3  = t1 + t2;
  return vec2(t3, t2 - (t3 - t1));
}

// color palette lookup — exact replica of the cpu-side lut logic.
// fi is the fractional iteration count. interpolates between discrete
// color steps to ensure smooth gradients.
vec3 lut_color(float fi, int pal) {
  float i = fi; vec3 a,b;
  if (pal==0) { // sine wave
    a=vec3(sin(0.1*i+4.0)*127.+128., sin(0.1*i+2.0)*127.+128., sin(0.1*i+0.0)*127.+128.)/255.;
    b=vec3(sin(0.1*(i+1.0)+4.0)*127.+128., sin(0.1*(i+1.0)+2.0)*127.+128., sin(0.1*(i+1.0)+0.0)*127.+128.)/255.;
    return mix(a, b, fract(fi));
  } else if (pal==1) { // volumetric magma
    float t1 = mod(i / 128.0, 1.0); float t2 = mod((i+1.0) / 128.0, 1.0);
    a = vec3(1.0 - exp(-4.0 * t1), pow(max(t1, 1e-5), 2.2), pow(max(t1, 1e-5), 7.0));
    b = vec3(1.0 - exp(-4.0 * t2), pow(max(t2, 1e-5), 2.2), pow(max(t2, 1e-5), 7.0));
    return mix(a, b, fract(fi));
  } else if (pal==3) { // grayscale
    a=vec3(mod(i, 256.0)/255.0);
    b=vec3(mod(i+1.0, 256.0)/255.0);
    return mix(a, b, fract(fi));
  } else if (pal==4) { // electric
    a=vec3(mod(i*1.0, 256.0)/255.0, mod(i*4.0, 256.0)/255.0, mod(i*8.0, 256.0)/255.0);
    b=vec3(mod((i+1.0)*1.0, 256.0)/255.0, mod((i+1.0)*4.0, 256.0)/255.0, mod((i+1.0)*8.0, 256.0)/255.0);
    return mix(a, b, fract(fi));
  } else if (pal==5) { // ocean
    a=vec3(mod(i*0.5, 256.0)/255.0, mod(i*2.0, 256.0)/255.0, mod(i*5.0, 256.0)/255.0);
    b=vec3(mod((i+1.0)*0.5, 256.0)/255.0, mod((i+1.0)*2.0, 256.0)/255.0, mod((i+1.0)*5.0, 256.0)/255.0);
    return mix(a, b, fract(fi));
  } else if (pal==6) { // inferno
    a=vec3(mod(i*8.0, 256.0)/255.0, mod(i*2.0, 256.0)/255.0, mod(i*0.5, 256.0)/255.0);
    b=vec3(mod((i+1.0)*8.0, 256.0)/255.0, mod((i+1.0)*2.0, 256.0)/255.0, mod((i+1.0)*0.5, 256.0)/255.0);
    return mix(a, b, fract(fi));
  } else { // viridis (pal==2 and fallback)
    float t1 = 1.0 - abs(mod(i/256., 2.0) - 1.0); float t2 = 1.0 - abs(mod((i+1.)/256., 2.0) - 1.0);
    a=vec3(0.267+t1*(0.993*t1-0.260), 0.004+t1*(1.490-t1*0.494), 0.329+t1*(1.268*t1*t1-0.680*t1-0.259));
    b=vec3(0.267+t2*(0.993*t2-0.260), 0.004+t2*(1.490-t2*0.494), 0.329+t2*(1.268*t2*t2-0.680*t2-0.259));
    return mix(a, b, fract(fi));
  }
}

// compute next derivative based on fractal type
vec2 get_next_dz(vec2 z, vec2 dz, float type, vec2 c_deriv) {
  if (type < 0.5 || type == 1.0) { // mandelbrot / julia
    return 2.0 * vec2(z.x * dz.x - z.y * dz.y, z.x * dz.y + z.y * dz.x) + c_deriv;
  } else if (type < 2.5) { // burning ship
    vec2 abs_z = abs(z);
    vec2 sig = sign(z);
    return 2.0 * vec2(abs_z.x * dz.x * sig.x - abs_z.y * dz.y * sig.y, abs_z.x * dz.y * sig.y + abs_z.y * dz.x * sig.x) + c_deriv;
  } else if (type < 3.5) { // tricorn
    return 2.0 * vec2(z.x * dz.x - z.y * dz.y, -z.x * dz.y - z.y * dz.x) + c_deriv;
  } else if (type < 4.5) { // celtic
    float sig_re = sign(z.x * z.x - z.y * z.y);
    return 2.0 * vec2((z.x * dz.x - z.y * dz.y) * sig_re, z.x * dz.y + z.y * dz.x) + c_deriv;
  } else { // buffalo
    float sig_re = sign(z.x * z.x - z.y * z.y);
    vec2 abs_z = abs(z);
    vec2 sig = sign(z);
    return 2.0 * vec2((z.x * dz.x - z.y * dz.y) * sig_re, -(abs_z.x * dz.y * sig.y + abs_z.y * dz.x * sig.x)) + c_deriv;
  }
}

void main() {
  int m = int(u_iters), i = 0;
  float mag2 = 0.0;
  vec2 c_deriv = (u_fractal_type == 1.0) ? vec2(0.0) : vec2(1.0, 0.0);
  vec2 dz = (u_fractal_type == 1.0) ? vec2(1.0, 0.0) : vec2(0.0);
  float trap_dist = 1e20;
  float linear_trap = 1e20;
  vec2 final_z = vec2(0.0);
  vec2 prev_u = vec2(0.0);
  vec2 curr_u = vec2(0.0);
  float prev_ang = 0.0;
  float curve_sum = 0.0;
  float ripple_sum = 0.0;
  float inner_axes = 1e20;
  float min_bubble_dist = 1e20;
  vec2 bubble_z = vec2(0.0);
  float bubble_iter = 0.0;
  float min_grid_dist = 1e20;

  // path 0: perturbation theory (high-speed deep zoom).
  // uses a single high-precision reference orbit computed on the cpu (zn)
  // and approximates the pixel position using a double-single delta (dn).
  // formula: delta_{n+1} = 2 * zn * delta_n + delta_n^2 + delta_0
  if (u_use_perturbation > 0.5 && u_fractal_type < 0.5) {
    vec2 zoom_ds = vec2(u_zoom, u_zoom_lo);
    vec2 d0_re = ds_mul(vec2(uv.x - 0.5 - u_ref_offset.x, 0.0), ds_mul(zoom_ds, vec2(u_aspect, 0.0)));
    vec2 d0_im = ds_mul(vec2(0.5 - uv.y - u_ref_offset.y, 0.0), zoom_ds);
    vec2 dn_re = d0_re;
    vec2 dn_im = d0_im;
    int orbit_n = int(u_orbit_len);
    for (; i < 2000; i++) {
      if (i >= orbit_n || i >= m) break;
      vec4 Zn_data = texture(u_orbit, vec2((float(i) + 0.5) / 10000.0, 0.5));
      vec2 Zn_re = vec2(Zn_data.r, Zn_data.g);
      vec2 Zn_im = vec2(Zn_data.b, Zn_data.a);
      
      vec2 Wn_re = ds_add(Zn_re, dn_re);
      vec2 Wn_im = ds_add(Zn_im, dn_im);
      vec2 Wn = vec2(Wn_re.x, Wn_im.x);
      mag2 = dot(Wn, Wn);
      if (mag2 > 100.0) break;

      // track derivative and orbit trap using reconstructed coordinate
      dz = get_next_dz(Wn, dz, u_fractal_type, c_deriv);
      if (dot(Wn, Wn) > 1e-12) {
        trap_dist = min(trap_dist, abs(Wn.x * Wn.y));
        linear_trap = min(linear_trap, min(abs(Wn.x), abs(Wn.y)));
      }
      prev_u = curr_u;
      curr_u = vec2(Wn.x * dz.x + Wn.y * dz.y, Wn.y * dz.x - Wn.x * dz.y);
      final_z = Wn;

      // track curvature, ripples and inner axes
      float ang = atan(Wn.y, Wn.x);
      float diff = abs(ang - prev_ang);
      if (diff > 3.14159) diff = 6.28318 - diff;
      curve_sum += diff;
      prev_ang = ang;
      ripple_sum += sin(8.0 * length(Wn));
      inner_axes = min(inner_axes, min(abs(Wn.x), abs(Wn.y)));
      float cell_dist = length(fract(Wn) - 0.5);
      if (cell_dist < min_bubble_dist) {
        min_bubble_dist = cell_dist;
        bubble_z = fract(Wn) - 0.5;
        bubble_iter = float(i);
      }
      float current_grid = min(abs(fract(Wn.x) - 0.5), abs(fract(Wn.y) - 0.5)) / length(dz);
      min_grid_dist = min(min_grid_dist, current_grid);

      // delta equation in double-single math
      vec2 dn_sq_re = ds_add(ds_sqr(dn_re), vec2(-ds_sqr(dn_im).x, -ds_sqr(dn_im).y));
      vec2 dn_sq_im = ds_mul(dn_re, dn_im);
      dn_sq_im = ds_add(dn_sq_im, dn_sq_im);

      vec2 Zn_dn_re = ds_add(ds_mul(Zn_re, dn_re), vec2(-ds_mul(Zn_im, dn_im).x, -ds_mul(Zn_im, dn_im).y));
      vec2 Zn_dn_im = ds_add(ds_mul(Zn_re, dn_im), ds_mul(Zn_im, dn_re));
      vec2 two_Zn_dn_re = ds_add(Zn_dn_re, Zn_dn_re);
      vec2 two_Zn_dn_im = ds_add(Zn_dn_im, Zn_dn_im);

      dn_re = ds_add(ds_add(two_Zn_dn_re, dn_sq_re), d0_re);
      dn_im = ds_add(ds_add(two_Zn_dn_im, dn_sq_im), d0_im);
    }

  // path 1: dekker double-single 64-bit emulation.
  } else if (u_high_precision > 0.5) {
    vec2 uv_dx = vec2(uv.x - 0.5, 0.0);
    vec2 uv_dy = vec2(0.5 - uv.y, 0.0);
    vec2 zoom_a = vec2(u_zoom, 0.0);
    vec2 aspect_a = vec2(u_aspect, 0.0);
    vec2 cx = vec2(u_center_hi.x, u_center_lo.x);
    vec2 cy = vec2(u_center_hi.y, u_center_lo.y);
    vec2 px = ds_add(ds_mul(ds_mul(uv_dx, zoom_a), aspect_a), cx);
    vec2 py = ds_add(ds_mul(uv_dy, zoom_a), cy);
    vec2 c_val_x = (u_fractal_type == 1.0) ? vec2(u_julia_c_hi.x, u_julia_c_lo.x) : px;
    vec2 c_val_y = (u_fractal_type == 1.0) ? vec2(u_julia_c_hi.y, u_julia_c_lo.y) : py;
    vec2 zx = (u_fractal_type == 1.0) ? px : vec2(0.0);
    vec2 zy = (u_fractal_type == 1.0) ? py : vec2(0.0);
    if (u_fractal_type < 0.5) { // cardioid rejection (mandelbrot only)
      float cr = px.x - 0.25, ci2 = py.x*py.x, q = cr*cr+ci2;
      if (q*(q+cr) <= 0.25*ci2) { i = m; }
      else { float cr1 = px.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
    }
    for (; i<2000; i++) {
      if (i>=m) break;
      vec2 x2 = ds_sqr(zx);
      vec2 y2 = ds_sqr(zy);
      mag2 = x2.x + y2.x;
      if (mag2 > 100.0) break;

      // track derivative and orbit trap using high parts
      vec2 z_approx = vec2(zx.x, zy.x);
      dz = get_next_dz(z_approx, dz, u_fractal_type, c_deriv);
      if (dot(z_approx, z_approx) > 1e-12) {
        trap_dist = min(trap_dist, abs(z_approx.x * z_approx.y));
        linear_trap = min(linear_trap, min(abs(z_approx.x), abs(z_approx.y)));
      }
      prev_u = curr_u;
      curr_u = vec2(z_approx.x * dz.x + z_approx.y * dz.y, z_approx.y * dz.x - z_approx.x * dz.y);
      final_z = z_approx;

      // track curvature, ripples and inner axes
      float ang = atan(z_approx.y, z_approx.x);
      float diff = abs(ang - prev_ang);
      if (diff > 3.14159) diff = 6.28318 - diff;
      curve_sum += diff;
      prev_ang = ang;
      ripple_sum += sin(8.0 * length(z_approx));
      inner_axes = min(inner_axes, min(abs(z_approx.x), abs(z_approx.y)));
      float cell_dist = length(fract(z_approx) - 0.5);
      if (cell_dist < min_bubble_dist) {
        min_bubble_dist = cell_dist;
        bubble_z = fract(z_approx) - 0.5;
        bubble_iter = float(i);
      }
      float current_grid = min(abs(fract(z_approx.x) - 0.5), abs(fract(z_approx.y) - 0.5)) / length(dz);
      min_grid_dist = min(min_grid_dist, current_grid);

      if (u_fractal_type == 2.0) {
        // burning ship: z = (|re(z)| + i|im(z)|)^2 + c
        vec2 abs_zx = (zx.x < 0.0) ? vec2(-zx.x, -zx.y) : zx;
        vec2 abs_zy = (zy.x < 0.0) ? vec2(-zy.x, -zy.y) : zy;
        vec2 zy_new = ds_add(ds_add(ds_mul(abs_zx, abs_zy), ds_mul(abs_zx, abs_zy)), c_val_y);
        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
        zx = zx_new; zy = zy_new;
      } else if (u_fractal_type == 3.0) {
        // tricorn (mandelbar): z = conj(z)^2 + c  =>  im = -2*re*im + c_im
        vec2 two_zx_zy = ds_add(ds_mul(zx, zy), ds_mul(zx, zy));
        vec2 zy_new = ds_add(vec2(-two_zx_zy.x, -two_zx_zy.y), c_val_y);
        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
        zx = zx_new; zy = zy_new;
      } else if (u_fractal_type == 4.0) {
        // celtic: z = |re(z^2)| + i*im(z^2) + c
        vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);
        vec2 diff = ds_add(x2, vec2(-y2.x, -y2.y));
        vec2 abs_diff = (diff.x < 0.0) ? vec2(-diff.x, -diff.y) : diff;
        vec2 zx_new = ds_add(abs_diff, c_val_x);
        zx = zx_new; zy = zy_new;
      } else if (u_fractal_type == 5.0) {
        // buffalo: z = |re(z^2)| - i*2|re(z)||im(z)| + c
        vec2 abs_zx = (zx.x < 0.0) ? vec2(-zx.x, -zx.y) : zx;
        vec2 abs_zy = (zy.x < 0.0) ? vec2(-zy.x, -zy.y) : zy;
        vec2 two_abs = ds_add(ds_mul(abs_zx, abs_zy), ds_mul(abs_zx, abs_zy));
        vec2 zy_new = ds_add(vec2(-two_abs.x, -two_abs.y), c_val_y);
        vec2 diff = ds_add(x2, vec2(-y2.x, -y2.y));
        vec2 abs_diff = (diff.x < 0.0) ? vec2(-diff.x, -diff.y) : diff;
        vec2 zx_new = ds_add(abs_diff, c_val_x);
        zx = zx_new; zy = zy_new;
      } else {
        // mandelbrot / julia: z = z^2 + c
        vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);
        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
        zx = zx_new; zy = zy_new;
      }
    }

  // path 2: standard 32-bit float precision.
  // high-speed path for low zoom levels where float mantissa is sufficient.
  } else {
    vec2 center = u_center_hi + u_center_lo;
    vec2 p = vec2((uv.x-0.5)*u_zoom*u_aspect+center.x, (0.5-uv.y)*u_zoom+center.y);
    vec2 c_val = (u_fractal_type == 1.0) ? (u_julia_c_hi + u_julia_c_lo) : p;
    vec2 z = (u_fractal_type == 1.0) ? p : vec2(0.0);
    if (u_fractal_type < 0.5) { // cardioid rejection (mandelbrot only)
      float cr = p.x - 0.25, ci2 = p.y*p.y, q = cr*cr + ci2;
      if (q*(q+cr) <= 0.25*ci2) { i = m; }
      else { float cr1 = p.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
    }
    for (; i<2000; i++) {
      if (i>=m) break;
      float x2=z.x*z.x, y2=z.y*z.y;
      mag2 = x2+y2;
      if (mag2 > 100.0) break;

      // track derivative and orbit trap
      dz = get_next_dz(z, dz, u_fractal_type, c_deriv);
      if (dot(z, z) > 1e-12) {
        trap_dist = min(trap_dist, abs(z.x * z.y));
        linear_trap = min(linear_trap, min(abs(z.x), abs(z.y)));
      }
      prev_u = curr_u;
      curr_u = vec2(z.x * dz.x + z.y * dz.y, z.y * dz.x - z.x * dz.y);
      final_z = z;

      // track curvature, ripples and inner axes
      float ang = atan(z.y, z.x);
      float diff = abs(ang - prev_ang);
      if (diff > 3.14159) diff = 6.28318 - diff;
      curve_sum += diff;
      prev_ang = ang;
      ripple_sum += sin(8.0 * length(z));
      inner_axes = min(inner_axes, min(abs(z.x), abs(z.y)));
      float cell_dist = length(fract(z) - 0.5);
      if (cell_dist < min_bubble_dist) {
        min_bubble_dist = cell_dist;
        bubble_z = fract(z) - 0.5;
        bubble_iter = float(i);
      }
      float current_grid = min(abs(fract(z.x) - 0.5), abs(fract(z.y) - 0.5)) / length(dz);
      min_grid_dist = min(min_grid_dist, current_grid);

      if (u_fractal_type == 2.0) {
        // burning ship
        z = vec2(x2-y2+c_val.x, 2.0*abs(z.x)*abs(z.y)+c_val.y);
      } else if (u_fractal_type == 3.0) {
        // tricorn (mandelbar): negate imaginary part only
        z = vec2(x2-y2+c_val.x, -2.0*z.x*z.y+c_val.y);
      } else if (u_fractal_type == 4.0) {
        // celtic: abs on real part of z^2
        z = vec2(abs(x2-y2)+c_val.x, 2.0*z.x*z.y+c_val.y);
      } else if (u_fractal_type == 5.0) {
        // buffalo: abs on real part of z^2 + negate imaginary
        z = vec2(abs(x2-y2)+c_val.x, -2.0*abs(z.x)*abs(z.y)+c_val.y);
      } else {
        // mandelbrot / julia
        z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);
      }
    }
  }

  // final output coloring
  int pal = int(u_palette);
  if (pal == 7) {
    // Retro Binary
    if (i >= m) {
      color = vec4(0.5, 0.9, 1.0, 1.0);
    } else {
      if (mod(float(i), 2.0) < 1.0) {
        color = vec4(0.2, 0.75, 0.25, 1.0);
      } else {
        color = vec4(0.1, 0.2, 0.8, 1.0);
      }
    }
  } else if (pal == 8) {
    // orbit mesh: visible inside and outside
    vec3 bg = vec3(0.01, 0.03, 0.15);
    if (i < m) {
      float s = float(i) + 2.0 - log2(log(max(1.0, mag2)));
      float factor = clamp(s / 60.0, 0.0, 1.0);
      bg = mix(vec3(0.0, 0.8, 0.85), vec3(0.01, 0.04, 0.2), factor);
      float boundary_glow = clamp(exp(-0.2 * (float(m) - s)), 0.0, 1.0);
      bg = mix(bg, vec3(0.0, 0.95, 1.0), boundary_glow * 0.65);
    }
    float line_factor = clamp(exp(-40.0 * linear_trap), 0.0, 1.0);
    color = vec4(mix(bg, vec3(1.0), line_factor), 1.0);
  } else if (pal == 10) {
    // conformal ripples: visible inside and outside
    float rip = (i > 0) ? (ripple_sum / float(i)) : 0.0;
    float val = 0.5 + 0.5 * rip;
    vec3 base = mix(vec3(0.01, 0.02, 0.15), vec3(0.0, 0.7, 0.9), val);
    if (i < m) {
      float s = float(i) + 2.0 - log2(log(max(1.0, mag2)));
      float boundary_glow = clamp(exp(-0.25 * (float(m) - s)), 0.0, 1.0);
      base = mix(base, vec3(0.0, 0.95, 1.0), boundary_glow * 0.5);
    }
    float line_factor = clamp(exp(-40.0 * linear_trap), 0.0, 1.0);
    color = vec4(mix(base, vec3(1.0, 0.1, 0.6), line_factor), 1.0);
  } else if (pal == 11) {
    // curvature marble: visible inside and outside
    float curve = (i > 0) ? (curve_sum / float(i)) : 0.0;
    float c = fract(curve * 1.5);
    vec3 col = vec3(
      sin(c * 3.14159 + 0.0) * 0.45 + 0.55,
      sin(c * 3.14159 + 1.0) * 0.45 + 0.55,
      sin(c * 3.14159 + 2.0) * 0.45 + 0.55
    );
    if (i >= m) {
      col *= 0.35;
    }
    color = vec4(col, 1.0);
  } else if (pal == 12) {
    // Conformal Grid: curved grid lines
    vec3 bg = vec3(0.01, 0.02, 0.1);
    if (i < m) {
      float s = float(i) + 2.0 - log2(log(max(1.0, mag2)));
      float factor = clamp(s / 80.0, 0.0, 1.0);
      bg = mix(vec3(0.0, 0.8, 0.95), vec3(0.0, 0.05, 0.35), factor);
      float boundary_glow = clamp(exp(-0.25 * (float(m) - s)), 0.0, 1.0);
      bg = mix(bg, vec3(0.8, 0.95, 1.0), boundary_glow * 0.75);
    }
    float pixel_w = u_zoom / 800.0;
    float line_factor = clamp(exp(-min_grid_dist / (pixel_w * 1.5)), 0.0, 1.0);
    color = vec4(mix(vec3(1.0), bg, line_factor), 1.0);
  } else if (pal == 13) {
    // cyber grid: visible inside and outside, but distinct inside
    float bg_factor = clamp(float(i) / float(m), 0.0, 1.0);
    vec3 bg = mix(vec3(0.02, 0.01, 0.08), vec3(0.05, 0.0, 0.15), bg_factor);
    if (i >= m) {
      // glowing web inside the bulbs
      float line_factor = clamp(exp(-25.0 * inner_axes), 0.0, 1.0);
      bg = mix(bg, vec3(0.0, 1.0, 0.6), line_factor * 0.85);
    } else {
      // glowing web outside
      float line_factor = clamp(exp(-25.0 * linear_trap), 0.0, 1.0);
      bg = mix(bg, vec3(0.0, 0.7, 1.0), line_factor * 0.6);
    }
    color = vec4(bg, 1.0);
  } else if (pal == 14) {
    // Bubble Pearl (3D): bubbles along filaments
    vec3 bg = vec3(0.02, 0.03, 0.06);
    if (min_bubble_dist < 0.45) {
      vec2 n2d = bubble_z / 0.45;
      float nz = sqrt(max(0.0, 1.0 - dot(n2d, n2d)));
      vec3 normal = normalize(vec3(n2d.x, n2d.y, nz));
      vec3 light = normalize(vec3(-1.0, 1.0, 1.5));
      float diffuse = max(0.0, dot(normal, light));
      vec3 view = vec3(0.0, 0.0, 1.0);
      vec3 half_vec = normalize(light + view);
      float specular = pow(max(0.0, dot(normal, half_vec)), 48.0);
      
      float t = clamp(bubble_iter / float(m), 0.0, 1.0);
      vec3 base = mix(vec3(0.1, 0.35, 0.6), vec3(0.7, 0.4, 0.2), t);
      vec3 lit = base * (0.3 + 0.7 * diffuse) + vec3(0.85 * specular);
      float edge = clamp((0.45 - min_bubble_dist) * 20.0, 0.0, 1.0);
      bg = mix(vec3(0.01, 0.02, 0.04), lit, edge);
    }
    color = vec4(bg, 1.0);
  } else if (i >= m) {
    int pal = int(u_palette);
    if (pal == 19) {
      // Classic Royal Blue: glowing cyan portal interior
      float dist = length(final_z);
      vec3 inner_col = mix(vec3(0.0, 0.0, 0.05), vec3(0.0, 0.6, 0.9), exp(-2.0 * dist));
      color = vec4(inner_col, 1.0);
    } else if (pal == 20) {
      // Classic Fire Red: glowing lava magma core interior
      float dist = length(final_z);
      vec3 inner_col = mix(vec3(0.05, 0.0, 0.0), vec3(1.0, 0.3, 0.0), exp(-3.0 * dist));
      color = vec4(inner_col, 1.0);
    } else {
      color = vec4(0.0, 0.0, 0.0, 1.0);
    }
  } else {
    float s = float(i) + 2.0 - log2(log(max(1.0, mag2)));

    // blinn-phong lighting
    float f = fract(s);
    vec2 u = mix(prev_u, curr_u, f);
    vec2 normal2d = vec2(0.0, 1.0);
    if (dot(u, u) > 1e-12) {
      normal2d = normalize(u);
    }
    vec3 normal3d = normalize(vec3(normal2d.x, normal2d.y, 1.5));
    vec3 light = normalize(vec3(-1.0, 1.0, 1.5));
    float diffuse = max(0.0, dot(normal3d, light));
    vec3 view = vec3(0.0, 0.0, 1.0);
    vec3 half_vec = normalize(light + view);
    float specular = pow(max(0.0, dot(normal3d, half_vec)), 32.0);

    if (pal == 9) {
      // palette 9: biomorph trap (previously 4)
      float factor = clamp(s / 80.0, 0.0, 1.0);
      vec3 bg = mix(vec3(0.4, 0.65, 0.95), vec3(0.01, 0.1, 0.5), factor);
      vec3 gold = vec3(0.9, 0.72, 0.15);
      float trap_factor = clamp(exp(-3.0 * trap_dist), 0.0, 1.0);
      vec3 final_c = mix(bg, gold, trap_factor);
      float outline = clamp(1.0 - abs(trap_dist - 0.15) * 8.0, 0.0, 1.0);
      final_c = mix(final_c, vec3(0.02, 0.02, 0.1), outline * 0.7);
      color = vec4(final_c, 1.0);
    } else if (pal == 15) {
      // palette 15: liquid chrome (previously 7)
      vec3 reflection = reflect(vec3(0.0, 0.0, -1.0), normal3d);
      // iridescent metallic reflection gradient
      vec3 chrome = vec3(
        sin(reflection.x * 4.0 + 1.0) * 0.4 + 0.6,
        sin(reflection.y * 4.0 + 2.0) * 0.45 + 0.55,
        sin(reflection.z * 4.0 + 3.0) * 0.4 + 0.6
      );
      // blend with highlights and smooth iteration depth
      float depth = clamp(s / 100.0, 0.0, 1.0);
      vec3 base = mix(chrome, vec3(0.02, 0.05, 0.1), depth * 0.4);
      vec3 lit = base * (0.4 + 0.6 * diffuse) + vec3(0.7 * specular);
      color = vec4(lit, 1.0);
    } else if (pal == 16) {
      // palette 16: refractive 3d glass (previously 12)
      vec3 refracted = refract(vec3(0.0, 0.0, -1.0), normal3d, 1.0 / 1.5);
      float warped_s = s + refracted.x * 8.0 + refracted.y * 8.0;
      vec3 glass_color = vec3(
        0.5 + 0.4 * sin(warped_s * 0.12 + 1.0),
        0.7 + 0.2 * sin(warped_s * 0.12 + 2.0),
        0.9 + 0.1 * sin(warped_s * 0.12 + 3.0)
      );
      float Fresnel = pow(1.0 - max(0.0, dot(normal3d, vec3(0.0, 0.0, 1.0))), 4.0);
      vec3 lit = glass_color * (0.55 + 0.45 * diffuse);
      vec3 final_color = mix(lit, vec3(1.0), Fresnel * 0.6) + vec3(specular * 0.9);
      color = vec4(final_color, 1.0);
    } else if (pal == 17) {
      // Ultra Fractal Classic
      float t = fract(s / 40.0);
      vec3 base_c;
      if (t < 0.2) {
        base_c = mix(vec3(0.0, 0.3, 0.7), vec3(1.0, 1.0, 1.0), (t - 0.0) / 0.2);
      } else if (t < 0.4) {
        base_c = mix(vec3(1.0, 1.0, 1.0), vec3(1.0, 0.8, 0.0), (t - 0.2) / 0.2);
      } else if (t < 0.65) {
        base_c = mix(vec3(1.0, 0.8, 0.0), vec3(0.5, 0.1, 0.05), (t - 0.4) / 0.25);
      } else if (t < 0.85) {
        base_c = mix(vec3(0.5, 0.1, 0.05), vec3(0.0, 0.05, 0.3), (t - 0.65) / 0.2);
      } else {
        base_c = mix(vec3(0.0, 0.05, 0.3), vec3(0.0, 0.3, 0.7), (t - 0.85) / 0.15);
      }
      // Add a subtle concentric mathematical ripple to make it look "weird" in a cool way!
      float rip = 0.85 + 0.15 * sin(s * 1.5);
      color = vec4(base_c * rip, 1.0);
    } else if (pal == 18) {
      // Pure Binary BW
      color = vec4(1.0, 1.0, 1.0, 1.0);
    } else if (pal == 19) {
      // Classic Royal Blue
      float t = fract(s / 60.0);
      if (t < 0.3) {
        color = vec4(mix(vec3(0.0, 0.02, 0.15), vec3(0.0, 0.1, 0.5), (t - 0.0) / 0.3), 1.0);
      } else if (t < 0.75) {
        color = vec4(mix(vec3(0.0, 0.1, 0.5), vec3(0.0, 0.8, 1.0), (t - 0.3) / 0.45), 1.0);
      } else if (t < 0.95) {
        color = vec4(mix(vec3(0.0, 0.8, 1.0), vec3(1.0, 1.0, 1.0), (t - 0.75) / 0.2), 1.0);
      } else {
        color = vec4(mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.02, 0.15), (t - 0.95) / 0.05), 1.0);
      }
    } else if (pal == 20) {
      // Classic Fire Red
      float t = fract(s / 50.0);
      if (t < 0.35) {
        color = vec4(mix(vec3(0.3, 0.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.0) / 0.35), 1.0);
      } else if (t < 0.75) {
        color = vec4(mix(vec3(1.0, 0.0, 0.0), vec3(1.0, 0.7, 0.0), (t - 0.35) / 0.40), 1.0);
      } else if (t < 0.95) {
        color = vec4(mix(vec3(1.0, 0.7, 0.0), vec3(1.0, 1.0, 0.4), (t - 0.75) / 0.20), 1.0);
      } else {
        color = vec4(mix(vec3(1.0, 1.0, 0.4), vec3(0.3, 0.0, 0.0), (t - 0.95) / 0.05), 1.0);
      }
    } else if (pal == 21) {
      // Silver Crimson
      float wave = 0.6 + 0.4 * sin(s * 0.8 + 6.0 * atan(normal2d.y, normal2d.x));
      vec3 silver_color = vec3(0.82 + 0.15 * wave);
      vec3 lit = silver_color * (0.65 + 0.35 * diffuse) + vec3(0.6 * specular);
      
      // Glowing crimson border
      float red_factor = clamp(exp(-0.04 * (float(m) - s)), 0.0, 1.0);
      red_factor = pow(red_factor, 0.6); // widen the glow
      
      color = vec4(mix(lit, vec3(0.95, 0.08, 0.02), red_factor), 1.0);
    } else {
      color = vec4(lut_color(max(0.0, s), pal), 1.0);
    }
  }
}
