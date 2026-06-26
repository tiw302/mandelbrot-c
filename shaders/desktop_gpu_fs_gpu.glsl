#version 400
uniform vec2 u_center_hi;
uniform vec2 u_center_lo;
uniform vec2 u_julia_c_hi;
uniform vec2 u_julia_c_lo;
uniform float u_zoom;
uniform float u_zoom_lo;  // low part of zoom for Dekker hi-lo d0 calculation
uniform float u_iters;
uniform float u_aspect;
uniform float u_fractal_type;
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
  } else if (pal==1) { // grayscale
    a=vec3(mod(i,256.)/255.); b=vec3(mod(i+1.,256.)/255.);
  } else if (pal==2) { // fire
    a=vec3(255.-abs(mod(i*1.,510.)-255.), 255.-abs(mod(i*2.,510.)-255.), 255.-abs(mod(i*4.,510.)-255.))/255.;
    b=vec3(255.-abs(mod((i+1.)*1.,510.)-255.), 255.-abs(mod((i+1.)*2.,510.)-255.), 255.-abs(mod((i+1.)*4.,510.)-255.))/255.;
  } else if (pal==3) { // electric
    a=vec3(255.-abs(mod(i*8.,510.)-255.), 255.-abs(mod(i*4.,510.)-255.), 255.-abs(mod(i*1.,510.)-255.))/255.;
    b=vec3(255.-abs(mod((i+1.)*8.,510.)-255.), 255.-abs(mod((i+1.)*4.,510.)-255.), 255.-abs(mod((i+1.)*1.,510.)-255.))/255.;
  } else if (pal==4) { // ocean
    a=vec3(255.-abs(mod(i*5.,510.)-255.), 255.-abs(mod(i*2.,510.)-255.), 255.-abs(mod(i*.5,510.)-255.))/255.;
    b=vec3(255.-abs(mod((i+1.)*5.,510.)-255.), 255.-abs(mod((i+1.)*2.,510.)-255.), 255.-abs(mod((i+1.)*.5,510.)-255.))/255.;
  } else if (pal==5) { // inferno
    a=vec3(255.-abs(mod(i*.5,510.)-255.), 255.-abs(mod(i*2.,510.)-255.), 255.-abs(mod(i*8.,510.)-255.))/255.;
    b=vec3(255.-abs(mod((i+1.)*.5,510.)-255.), 255.-abs(mod((i+1.)*2.,510.)-255.), 255.-abs(mod((i+1.)*8.,510.)-255.))/255.;
  } else if (pal==6) { // viridis
    // ping-pong wrap for gpu shader
    float t1 = 1.0 - abs(mod(i/256., 2.0) - 1.0); float t2 = 1.0 - abs(mod((i+1.)/256., 2.0) - 1.0);
    a=vec3(0.267+t1*(0.993*t1-0.260), 0.004+t1*(1.490-t1*0.494), 0.329+t1*(1.268*t1*t1-0.680*t1-0.259));
    b=vec3(0.267+t2*(0.993*t2-0.260), 0.004+t2*(1.490-t2*0.494), 0.329+t2*(1.268*t2*t2-0.680*t2-0.259));
  } else if (pal==7) { // plasma
    float t1 = 1.0 - abs(mod(i/256., 2.0) - 1.0); float t2 = 1.0 - abs(mod((i+1.)/256., 2.0) - 1.0);
    a=vec3(0.050+t1*(2.735-t1*1.785), max(0.,t1*(1.580*t1-0.580)), max(0.,0.530+t1*(0.750-t1*1.280)));
    b=vec3(0.050+t2*(2.735-t2*1.785), max(0.,t2*(1.580*t2-0.580)), max(0.,0.530+t2*(0.750-t2*1.280)));
  } else { // twilight
    float t1 = fract(i/128.); float t2 = fract((i+1.)/128.);
    a=vec3(0.5+0.5*sin(6.283*t1), 0.3+0.2*sin(6.283*t1+2.094), 0.5+0.5*sin(6.283*t1+4.189));
    b=vec3(0.5+0.5*sin(6.283*t2), 0.3+0.2*sin(6.283*t2+2.094), 0.5+0.5*sin(6.283*t2+4.189));
  }
  return mix(a, b, fract(fi));
}

void main() {
  int m = int(u_iters), i = 0;
  float mag2 = 0.0;

  // path 0: perturbation theory (high-speed deep zoom).
  // uses a single high-precision reference orbit computed on the cpu (Zn)
  // and approximates the pixel position using a low-precision float delta (dn).
  // formula: delta_{n+1} = 2 * Zn * delta_n + delta_n^2 + delta_0
  if (u_use_perturbation > 0.5 && u_fractal_type < 0.5) {
    // d0 is the complex offset of this pixel from the reference center.
    // computed with Dekker double-single arithmetic so that even at extreme zoom
    // levels (below float range) the pixel offset retains sufficient precision.
    vec2 zoom_ds = vec2(u_zoom, u_zoom_lo);
    vec2 d0 = vec2(
        ds_mul(vec2(uv.x - 0.5, 0.0), ds_mul(zoom_ds, vec2(u_aspect, 0.0))).x,
        ds_mul(vec2(0.5 - uv.y, 0.0), zoom_ds).x
    );
    vec2 dn = d0;
    int orbit_n = int(u_orbit_len);
    for (; i < 2000; i++) {
      if (i >= orbit_n || i >= m) break;
      
      // sample reference orbit point Z_n from texture
      vec2 Zn = texture(u_orbit, vec2((float(i) + 0.5) / 10000.0, 0.5)).rg;
      
      // calculate actual coordinate value W_n = Z_n + delta_n
      vec2 Wn = Zn + dn;
      mag2 = dot(Wn, Wn);
      if (mag2 > 100.0) break;
      
      // compute next delta: dn_next = 2 * Zn * dn + dn^2 + d0
      vec2 dn_sq = vec2(dn.x * dn.x - dn.y * dn.y, 2.0 * dn.x * dn.y);
      vec2 Zn_dn = vec2(Zn.x * dn.x - Zn.y * dn.y, Zn.x * dn.y + Zn.y * dn.x);
      dn = 2.0 * Zn_dn + dn_sq + d0;
    }
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
    if (u_fractal_type < 0.5) { // cardioid rejection (mandelbrot)
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
      if (u_fractal_type > 1.5) { // burning ship
        vec2 abs_zx = (zx.x < 0.0) ? vec2(-zx.x, -zx.y) : zx;
        vec2 abs_zy = (zy.x < 0.0) ? vec2(-zy.x, -zy.y) : zy;
        vec2 zy_new = ds_add(ds_add(ds_mul(abs_zx, abs_zy), ds_mul(abs_zx, abs_zy)), c_val_y);
        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
        zx = zx_new; zy = zy_new;
      } else { // mandelbrot / julia
        vec2 zy_new = ds_add(ds_add(ds_mul(zx, zy), ds_mul(zx, zy)), c_val_y);
        vec2 zx_new = ds_add(ds_add(x2, vec2(-y2.x, -y2.y)), c_val_x);
        zx = zx_new; zy = zy_new;
      }
    }

    // path 2: standard float precision.
    // high-speed path for low zoom levels where 32-bit float mantissa is enough.
  } else {
    vec2 center = u_center_hi + u_center_lo;
    vec2 p = vec2((uv.x-0.5)*u_zoom*u_aspect+center.x, (0.5-uv.y)*u_zoom+center.y);
    vec2 c_val = (u_fractal_type == 1.0) ? (u_julia_c_hi + u_julia_c_lo) : p;
    vec2 z = (u_fractal_type == 1.0) ? p : vec2(0.0);
    if (u_fractal_type < 0.5) { // cardioid rejection
      float cr = p.x - 0.25, ci2 = p.y*p.y, q = cr*cr + ci2;
      if (q*(q+cr) <= 0.25*ci2) { i = m; }
      else { float cr1 = p.x+1.0; if (cr1*cr1+ci2 <= 0.0625) i = m; }
    }
    for (; i<2000; i++) {
      if (i>=m) break;
      float x2=z.x*z.x, y2=z.y*z.y;
      mag2 = x2+y2;
      if (mag2 > 100.0) break;
      if (u_fractal_type > 1.5) {
        z = vec2(x2-y2+c_val.x, 2.0*abs(z.x)*abs(z.y)+c_val.y);
      } else {
        z = vec2(x2-y2+c_val.x, 2.0*z.x*z.y+c_val.y);
      }
    }
  }

  // final output coloring
  if (i>=m) { color=vec4(0,0,0,1); }
  else {
    float s = float(i)+2.0-log2(log(max(1.0,mag2)));
    color = vec4(lut_color(max(0.0,s), int(u_palette)), 1.0);
  }
}
