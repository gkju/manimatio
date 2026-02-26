//! ESDT/SDF generation for alpha and RGBA inputs.
//!
//! Color convention summary:
//! - `compute_rgba_esdt*` expects **premultiplied RGBA8** input (e.g. from `resvg/tiny-skia`).
//! - Internally, RGB is converted into a **straight-color spill field** using only trusted
//!   near-solid seeds, then propagated across transparent space.
//! - `compute_rgba_esdt` returns RGBA8 where:
//!     - RGB = straight propagated color field
//!     - A   = SDF remapped to 0..255 using `spread`
//! - `compute_rgba_esdt_f32` returns packed `[R, G, B, signed_distance]` per pixel where:
//!     - RGB = straight propagated color field in 0..1
//!     - D   = signed distance in pixels (positive outside, negative inside)
//!
//! Note: The returned RGB is for SDF reconstruction/morphing, not directly a display-ready
//! premultiplied image. If compositing into a premultiplied pipeline, premultiply at the end.

pub type Float = f32; // | f64

pub struct EsdtCoreResult {
    pub sdf: Vec<Float>,
    pub xo: Vec<Float>,
    pub yo: Vec<Float>,
}

pub fn compute_esdt_core(width: usize, height: usize, data: &[u8]) -> EsdtCoreResult {
    let np = width * height;
    let mut xo = vec![0.0 as Float; np];
    let mut yo = vec![0.0 as Float; np];
    let mut xi = vec![0.0 as Float; np];
    let mut yi = vec![0.0 as Float; np];
    
    let mut outer = vec![true; np];
    let mut inner = vec![false; np];

    for y in 0..height {
        for x in 0..width {
            let a = data[y * width + x];
            if a == 0 { continue; }
            
            let i = y * width + x;
            if a >= 254 {
                outer[i] = false;
                inner[i] = true;
            } else {
                outer[i] = false;
                inner[i] = false;
            }
        }
    }

    let get_data = |x: isize, y: isize| -> Float {
        if x >= 0 && (x as usize) < width && y >= 0 && (y as usize) < height {
            (data[y as usize * width + x as usize] as Float) / 255.0
        } else {
            0.0
        }
    };

    let is_solid = |v: Float| v <= 0.0 || v >= 1.0;
    let is_black = |v: Float| v <= 0.0;
    let is_white = |v: Float| v >= 1.0;

    for y in 0..(height as isize) {
        for x in 0..(width as isize) {
            let c = get_data(x, y);
            let j = (y as usize) * width + (x as usize);

            if !is_solid(c) {
                let dc = c - 0.5;
                let l = get_data(x - 1, y); let r = get_data(x + 1, y);
                let t = get_data(x, y - 1); let b = get_data(x, y + 1);
                let tl = get_data(x - 1, y - 1); let tr = get_data(x + 1, y - 1);
                let bl = get_data(x - 1, y + 1); let br = get_data(x + 1, y + 1);

                let ll = (tl + l * 2.0 + bl) / 4.0;
                let rr = (tr + r * 2.0 + br) / 4.0;
                let tt = (tl + t * 2.0 + tr) / 4.0;
                let bb = (bl + b * 2.0 + br) / 4.0;

                let min = [l, r, t, b, tl, tr, bl, br].into_iter().fold(Float::INFINITY, |a, b| a.min(b));
                let max = [l, r, t, b, tl, tr, bl, br].into_iter().fold(Float::NEG_INFINITY, |a, b| a.max(b));

                if min > 0.0 {
                    inner[j] = true;
                    continue;
                }
                if max < 1.0 {
                    outer[j] = true;
                    continue;
                }

                let mut dx = rr - ll;
                let mut dy = bb - tt;
                let dist_sq = dx * dx + dy * dy;
                
                if dist_sq > 0.0 {
                    let dl = 1.0 / dist_sq.sqrt();
                    dx *= dl; dy *= dl;
                } else {
                    dx = 0.0; dy = 0.0;
                }

                xo[j] = -dc * dx;
                yo[j] = -dc * dy;
            } else if is_white(c) {
                let l = get_data(x - 1, y); let r = get_data(x + 1, y);
                let t = get_data(x, y - 1); let b = get_data(x, y + 1);

                if is_black(l) && x > 0 {
                    xo[j - 1] = 0.4999;
                    outer[j - 1] = false; inner[j - 1] = false;
                }
                if is_black(r) && (x as usize) + 1 < width {
                    xo[j + 1] = -0.4999;
                    outer[j + 1] = false; inner[j + 1] = false;
                }
                if is_black(t) && y > 0 {
                    yo[j - width] = 0.4999;
                    outer[j - width] = false; inner[j - width] = false;
                }
                if is_black(b) && (y as usize) + 1 < height {
                    yo[j + width] = -0.4999;
                    outer[j + width] = false; inner[j + width] = false;
                }
            }
        }
    }

    for y in 0..(height as isize) {
        for x in 0..(width as isize) {
            let j = (y as usize) * width + (x as usize);
            let nx = xo[j]; let ny = yo[j];
            if nx == 0.0 && ny == 0.0 { continue; }

            let nn = (nx * nx + ny * ny).sqrt();
            let sx = if (nx / nn).abs() - 0.5 > 0.0 { if nx > 0.0 { 1 } else { -1 } } else { 0 };
            let sy = if (ny / nn).abs() - 0.5 > 0.0 { if ny > 0.0 { 1 } else { -1 } } else { 0 };

            let c = get_data(x, y);
            let d = get_data(x + sx, y + sy);
            let s = if d > c { 1.0 } else if d < c { -1.0 } else { 0.0 };

            let dlo = (nn + 0.4999 * s) / nn;
            let dli = (nn - 0.4999 * s) / nn;

            xo[j] = nx * dlo; yo[j] = ny * dlo;
            xi[j] = nx * dli; yi[j] = ny * dli;
        }
    }

    esdt(&mut outer, &mut xo, &mut yo, width, height);
    esdt(&mut inner, &mut xi, &mut yi, width, height);

    let mut out = vec![0.0; np];
    for i in 0..np {
        let outer_d = ((xo[i] * xo[i] + yo[i] * yo[i]).sqrt() - 0.5).max(0.0);
        let inner_d = ((xi[i] * xi[i] + yi[i] * yi[i]).sqrt() - 0.5).max(0.0);
        out[i] = if outer_d >= inner_d { outer_d } else { -inner_d };
    }
    
    EsdtCoreResult { sdf: out, xo, yo }
}

pub fn compute_sdf(width: usize, height: usize, data: &[u8]) -> Vec<Float> {
    compute_esdt_core(width, height, data).sdf
}

#[derive(Debug)]
struct Esdt1dScratch {
    f: Vec<Float>,
    z: Vec<Float>,     // length + 1
    b: Vec<Float>,
    t: Vec<Float>,
    v: Vec<usize>,
}

impl Esdt1dScratch {
    fn new(max_len: usize) -> Self {
        Self {
            f: vec![0.0; max_len],
            z: vec![0.0; max_len + 1],
            b: vec![0.0; max_len],
            t: vec![0.0; max_len],
            v: vec![0; max_len],
        }
    }

    #[inline]
    fn ensure_capacity(&mut self, max_len: usize) {
        if self.f.len() < max_len {
            self.f.resize(max_len, 0.0);
            self.b.resize(max_len, 0.0);
            self.t.resize(max_len, 0.0);
            self.v.resize(max_len, 0);
        }
        if self.z.len() < max_len + 1 {
            self.z.resize(max_len + 1, 0.0);
        }
    }
}

fn esdt(mask: &mut [bool], xs: &mut [Float], ys: &mut [Float], w: usize, h: usize) {
    let mut scratch = Esdt1dScratch::new(w.max(h));
    esdt_with_scratch(mask, xs, ys, w, h, &mut scratch);
}

fn esdt_with_scratch(
    mask: &mut [bool],
    xs: &mut [Float],
    ys: &mut [Float],
    w: usize,
    h: usize,
    scratch: &mut Esdt1dScratch,
) {
    scratch.ensure_capacity(w.max(h));

    for x in 0..w {
        esdt1d(mask, ys, xs, x, w, h, scratch);
    }
    for y in 0..h {
        esdt1d(mask, xs, ys, y * w, 1, w, scratch);
    }
}

fn esdt1d(
    mask: &mut [bool],
    xs: &mut [Float],
    ys: &mut [Float],
    offset: usize,
    stride: usize,
    length: usize,
    scratch: &mut Esdt1dScratch,
) {
    let inf = 1e10 as Float;

    // Reuse only the prefix we need for this line
    let (f, z, b, t, v) = (
        &mut scratch.f[..length],
        &mut scratch.z[..(length + 1)],
        &mut scratch.b[..length],
        &mut scratch.t[..length],
        &mut scratch.v[..length],
    );

    v[0] = 0;
    b[0] = xs[offset];
    t[0] = ys[offset];
    z[0] = -inf;
    z[1] = inf;
    f[0] = if mask[offset] { inf } else { ys[offset] * ys[offset] };

    let mut k = 0usize;
    for q in 1..length {
        let o = offset + q * stride;
        let dx = xs[o];
        let dy = ys[o];
        let fq = if mask[o] { inf } else { dy * dy };

        f[q] = fq;
        t[q] = dy;

        let qs = (q as Float) + dx;
        let q2 = qs * qs;
        b[q] = qs;

        let mut s;
        loop {
            let r = v[k];
            let rs = b[r];
            let r2 = rs * rs;
            s = (fq - f[r] + q2 - r2) / (qs - rs) / 2.0;
            if s <= z[k] && k > 0 {
                k -= 1;
            } else {
                break;
            }
        }

        k += 1;
        v[k] = q;
        z[k] = s;
        z[k + 1] = inf;
    }

    k = 0;
    for q in 0..length {
        while z[k + 1] < (q as Float) {
            k += 1;
        }

        let r = v[k];
        let rs = b[r];
        let dy = t[r];
        let rq = rs - (q as Float);

        let o = offset + q * stride;
        xs[o] = rq;
        ys[o] = dy;

        if r != q {
            mask[o] = false;
        }
    }
}

#[inline]
fn copy_rgb_at(rgb: &[u8], idx: usize, out: &mut [u8], out_i4: usize) {
    let s3 = idx * 3;
    out[out_i4 + 0] = rgb[s3 + 0];
    out[out_i4 + 1] = rgb[s3 + 1];
    out[out_i4 + 2] = rgb[s3 + 2];
}

// Fill color field from seeded pixels only (separable nearest fill: rows, then cols).
fn fill_seed_field(width: usize, height: usize, seed_rgb: &mut [u8], seed_mask: &mut [u8]) {
    for y in 0..height {
        fill_seed_field_1d(seed_rgb, seed_mask, y * width, 1, width);
    }
    for x in 0..width {
        fill_seed_field_1d(seed_rgb, seed_mask, x, width, height);
    }
}

fn fill_seed_field_1d(
    seed_rgb: &mut [u8],
    seed_mask: &mut [u8],
    offset: usize,
    stride: usize,
    length: usize,
) {
    let mut s: isize = -1;

    for i in 0..length {
        let idx = offset + stride * i;
        if seed_mask[idx] != 0 {
            if s < (i as isize - 1) {
                if s == -1 {
                    // Fill left/start gap with this seed.
                    fill_seed_span(seed_rgb, seed_mask, offset, stride, i, 0);
                } else {
                    // Split the gap at midpoint between previous seed and this seed.
                    let su = s as usize;
                    let m = (su + i) / 2;
                    fill_seed_span(seed_rgb, seed_mask, offset, stride, su, m);
                    fill_seed_span(seed_rgb, seed_mask, offset, stride, i, m);
                }
            }
            s = i as isize;
        }
    }

    // Fill trailing gap to the end.
    if s >= 0 && (s as usize) < length - 1 {
        fill_seed_span(seed_rgb, seed_mask, offset, stride, s as usize, length - 1);
    }
}

fn fill_seed_span(
    seed_rgb: &mut [u8],
    seed_mask: &mut [u8],
    offset: usize,
    stride: usize,
    from: usize,
    to: usize,
) {
    let src = offset + stride * from;
    let src3 = src * 3;
    let r = seed_rgb[src3 + 0];
    let g = seed_rgb[src3 + 1];
    let b = seed_rgb[src3 + 2];

    if from <= to {
        for i in from..=to {
            let idx = offset + stride * i;
            let d3 = idx * 3;
            seed_rgb[d3 + 0] = r;
            seed_rgb[d3 + 1] = g;
            seed_rgb[d3 + 2] = b;
            seed_mask[idx] = 255;
        }
    } else {
        for i in (to..=from).rev() {
            let idx = offset + stride * i;
            let d3 = idx * 3;
            seed_rgb[d3 + 0] = r;
            seed_rgb[d3 + 1] = g;
            seed_rgb[d3 + 2] = b;
            seed_mask[idx] = 255;
        }
    }
}

pub fn propagate_rgba_colors(
    width: usize,
    height: usize,
    rgba: &[u8],
    xo: &[Float],
    yo: &[Float],
) -> Vec<u8> {
    let np = width * height;
    let mut out_rgba = vec![0u8; np * 4];

    // Strict Acko-style seeding for premultiplied renderer output:
    // only near-solid pixels are trusted as color sources.
    const SEED_ALPHA_MIN: u8 = 250;

    // Seed field (RGB only) + occupancy mask used for separable fill.
    let mut seed_rgb = vec![0u8; np * 3];
    let mut seed_mask = vec![0u8; np];

    // Pass 1a: seed ONLY from trusted near-solid pixels.
    let mut any_seed = false;
    for i in 0..np {
        let i4 = i * 4;
        if rgba[i4 + 3] >= SEED_ALPHA_MIN {
            let d3 = i * 3;
            seed_rgb[d3 + 0] = rgba[i4 + 0];
            seed_rgb[d3 + 1] = rgba[i4 + 1];
            seed_rgb[d3 + 2] = rgba[i4 + 2];
            seed_mask[i] = 255;
            any_seed = true;
        }
    }

    // Preserve original raster alpha here (caller may overwrite with SDF alpha later).
    for i in 0..np {
        out_rgba[i * 4 + 3] = rgba[i * 4 + 3];
    }

    // No trusted seeds at all (e.g. fully translucent-only content).
    if !any_seed {
        return out_rgba; // RGB stays zero
    }

    // Pass 1b: fill/spill seed colors across the whole image (Acko-style separable fill).
    fill_seed_field(width, height, &mut seed_rgb, &mut seed_mask);

    // Pass 2: map each pixel to its nearest boundary location using ESDT and sample the filled field.
    for y in 0..height {
        for x in 0..width {
            let i = y * width + x;
            let i4 = i * 4;

            // Preserve exact trusted seed colors for seed pixels (avoids unnecessary remap on interiors).
            if rgba[i4 + 3] >= SEED_ALPHA_MIN {
                out_rgba[i4 + 0] = rgba[i4 + 0];
                out_rgba[i4 + 1] = rgba[i4 + 1];
                out_rgba[i4 + 2] = rgba[i4 + 2];
                continue;
            }

            let sx = (x as Float + xo[i]).round() as isize;
            let sy = (y as Float + yo[i]).round() as isize;

            let sx = sx.clamp(0, width as isize - 1) as usize;
            let sy = sy.clamp(0, height as isize - 1) as usize;
            let si = sy * width + sx;

            copy_rgb_at(&seed_rgb, si, &mut out_rgba, i4);
        }
    }

    out_rgba
}

pub fn compute_rgba_esdt(
    width: usize,
    height: usize,
    rgba: &[u8],
    spread: Float,
) -> Vec<u8> {
    let np = width * height;
    let mut alpha_chan = vec![0u8; np];
    for i in 0..np {
        alpha_chan[i] = rgba[i * 4 + 3];
    }

    let core = compute_esdt_core(width, height, &alpha_chan);
    let mut out_rgba = propagate_rgba_colors(width, height, rgba, &core.xo, &core.yo);

    for i in 0..np {
        let dist = core.sdf[i];
        let norm = 0.5 - 0.5 * (dist / spread);
        let a = (norm * 255.0).clamp(0.0, 255.0) as u8;
        out_rgba[i * 4 + 3] = a;
    }

    out_rgba
}

pub fn compute_rgba_esdt_f32(
    width: usize,
    height: usize,
    rgba: &[u8],
) -> Vec<f32> {
    let np = width * height;
    let mut alpha_chan = vec![0u8; np];
    for i in 0..np {
        alpha_chan[i] = rgba[i * 4 + 3];
    }

    let core = compute_esdt_core(width, height, &alpha_chan);
    let propagated = propagate_rgba_colors(width, height, rgba, &core.xo, &core.yo);

    let mut out = Vec::with_capacity(np * 4);
    for i in 0..np {
        out.push(propagated[i * 4] as f32 / 255.0);
        out.push(propagated[i * 4 + 1] as f32 / 255.0);
        out.push(propagated[i * 4 + 2] as f32 / 255.0);
        out.push(core.sdf[i]);
    }
    out
}