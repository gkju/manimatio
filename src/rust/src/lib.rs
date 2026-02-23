mod esdt;

use chrono::{Datelike, Local};
use std::sync::OnceLock;
use typst::diag::{FileError, FileResult};
use typst::foundations::{Bytes, Datetime, Dict, Str, Value};
use typst::syntax::{FileId, Source, VirtualPath};
use typst::text::{Font, FontBook};
use typst::utils::LazyHash;
use typst::{Library, LibraryExt, World};
use typst_kit::download::{DownloadState, Downloader, Progress};
use typst_kit::fonts::FontSlot;
use typst_kit::package::PackageStorage;
use resvg::{tiny_skia, usvg};

#[cxx::bridge]
pub mod ffi {
    struct RenderResult {
        svg: String,
        error: String,
    }

    struct SvgSequenceResult {
        svgs: Vec<String>,
        error: String,
    }

    struct RasterResult {
        width: u32,
        height: u32,
        data: Vec<u8>,
        error: String,
    }

    struct RasterResultF32 {
        width: u32,
        height: u32,
        data: Vec<f32>, 
        error: String,
    }

    extern "Rust" {
        fn render_typst_to_svg(code: &str) -> RenderResult;
        fn render_typst_sequence(template_code: &str, params_json: &str) -> SvgSequenceResult;
        
        /// Rasterize an SVG to an RGBA8 pixel buffer using `resvg` + `tiny-skia`.
        ///
        /// Output format:
        /// - `data`: RGBA8 with **premultiplied alpha**
        /// - `width`, `height`: snapped to `grid_size` unless overridden by `force_w/force_h`
        ///
        /// The SVG is scaled by `scale` and centered in the output canvas.
        /// This is useful for consistent alignment in morphing/compositing pipelines.
        fn rasterize_svg(svg: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> RasterResult;
        
        // Typst -> SDF
        fn render_typst_to_sdf_f32(code: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> RasterResultF32;

        // Typst -> packed float RGBD (not RGBA):
        // [R, G, B, signed_distance, ...]
        // RGB is straight propagated color; distance is in pixel units.
        fn render_typst_to_rgba_esdt(code: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32, spread: f32) -> RasterResult;
        /// Compile Typst -> SVG -> raster -> color-propagated ESDT.
        ///
        /// Returns packed float data in `[R, G, B, D]` layout per pixel:
        /// - `R,G,B` are **straight propagated colors** in `[0,1]`
        /// - `D` is signed distance in pixel units (`<0` inside, `>0` outside)
        ///
        /// Input rasterization is premultiplied RGBA (from tiny-skia), but AA-edge RGB is ignored
        /// during color seeding to avoid premultiplication artifacts.
        fn render_typst_to_rgba_esdt_f32(code: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> RasterResultF32;

        // ESDT fns
        fn compute_esdt_alpha(width: u32, height: u32, alpha: &[u8]) -> RasterResultF32;
        /// Compute an RGBA8 SDF texture from premultiplied RGBA8 input.
        ///
        /// Output:
        /// - RGB: straight propagated color field (spilled into transparent space)
        /// - A:   normalized SDF alpha encoded with `spread`
        ///
        /// This output is intended for SDF reconstruction/shader sampling and may require
        /// premultiplication at final render time depending on the consumer pipeline.
        fn compute_esdt_rgba8(width: u32, height: u32, rgba: &[u8], spread: f32) -> RasterResult;
        fn compute_esdt_rgba32f(width: u32, height: u32, rgba: &[u8]) -> RasterResultF32;
    }
}

pub fn rasterize_svg(svg: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> ffi::RasterResult {
    let opt = usvg::Options::default();
    
    let tree = match usvg::Tree::from_str(svg, &opt) {
        Ok(t) => t,
        Err(e) => {
            return ffi::RasterResult {
                width: 0, height: 0, data: vec![],
                error: format!("SVG parsing failed: {:?}", e),
            }
        }
    };

    let intrinsic_w = tree.size().width() * scale;
    let intrinsic_h = tree.size().height() * scale;
    
    let mut w = intrinsic_w.ceil() as u32;
    let mut h = intrinsic_h.ceil() as u32;

    // Apply strict bounds or snap to grid composition size
    if force_w > 0 { w = force_w; }
    else if grid_size > 0 { w = ((w + grid_size - 1) / grid_size) * grid_size; }

    if force_h > 0 { h = force_h; }
    else if grid_size > 0 { h = ((h + grid_size - 1) / grid_size) * grid_size; }
    
    if w == 0 || h == 0 {
        return ffi::RasterResult {
            width: 0, height: 0, data: vec![],
            error: "Invalid SVG dimensions (0-width or 0-height)".to_string(),
        };
    }

    let mut pixmap = match tiny_skia::Pixmap::new(w, h) {
        Some(p) => p,
        None => return ffi::RasterResult { width: 0, height: 0, data: vec![], error: "Failed to allocate pixel map".to_string() }
    };

    // Center the SVG on the canvas (crucial for grid layouts and morphing alignment)
    let dx = (w as f32 - intrinsic_w) / 2.0;
    let dy = (h as f32 - intrinsic_h) / 2.0;
    
    let transform = tiny_skia::Transform::from_scale(scale, scale).post_translate(dx, dy);
    resvg::render(&tree, transform, &mut pixmap.as_mut());

    let data = pixmap.take();

    ffi::RasterResult { width: w, height: h, data, error: String::new() }
}

pub fn render_typst_to_sdf_f32(code: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> ffi::RasterResultF32 {
    let svg_res = render_typst_to_svg(code);
    if !svg_res.error.is_empty() {
        return ffi::RasterResultF32 { width: 0, height: 0, data: vec![], error: svg_res.error };
    }

    let raster_res = rasterize_svg(&svg_res.svg, scale, grid_size, force_w, force_h);
    if !raster_res.error.is_empty() { 
        return ffi::RasterResultF32 { width: 0, height: 0, data: vec![], error: raster_res.error };
    }

    let width = raster_res.width as usize;
    let height = raster_res.height as usize;
    
    let mut alpha_coverage = Vec::with_capacity(width * height);
    for chunk in raster_res.data.chunks_exact(4) {
        alpha_coverage.push(chunk[3]); 
    }

    let sdf_floats = esdt::compute_sdf(width, height, &alpha_coverage);

    let final_data: Vec<f32> = sdf_floats.into_iter().map(|f| f as f32).collect();

    ffi::RasterResultF32 { 
        width: width as u32, 
        height: height as u32, 
        data: final_data, 
        error: String::new() 
    }
}

pub fn render_typst_to_rgba_esdt(code: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32, spread: f32) -> ffi::RasterResult {
    let svg_res = render_typst_to_svg(code);
    if !svg_res.error.is_empty() {
        return ffi::RasterResult { width: 0, height: 0, data: vec![], error: svg_res.error };
    }

    let raster_res = rasterize_svg(&svg_res.svg, scale, grid_size, force_w, force_h);
    if !raster_res.error.is_empty() {
        return ffi::RasterResult { width: 0, height: 0, data: vec![], error: raster_res.error };
    }

    compute_esdt_rgba8(raster_res.width, raster_res.height, &raster_res.data, spread)
}

pub fn render_typst_to_rgba_esdt_f32(code: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> ffi::RasterResultF32 {
    let svg_res = render_typst_to_svg(code);
    if !svg_res.error.is_empty() {
        return ffi::RasterResultF32 { width: 0, height: 0, data: vec![], error: svg_res.error };
    }

    let raster_res = rasterize_svg(&svg_res.svg, scale, grid_size, force_w, force_h);
    if !raster_res.error.is_empty() {
        return ffi::RasterResultF32 { width: 0, height: 0, data: vec![], error: raster_res.error };
    }

    compute_esdt_rgba32f(raster_res.width, raster_res.height, &raster_res.data)
}

pub fn compute_esdt_alpha(width: u32, height: u32, alpha: &[u8]) -> ffi::RasterResultF32 {
    if alpha.len() != (width * height) as usize {
        return ffi::RasterResultF32 { width: 0, height: 0, data: vec![], error: "Alpha buffer size mismatch".into() };
    }
    let sdf = esdt::compute_sdf(width as usize, height as usize, alpha);
    ffi::RasterResultF32 { width, height, data: sdf, error: String::new() }
}

pub fn compute_esdt_rgba8(width: u32, height: u32, rgba: &[u8], spread: f32) -> ffi::RasterResult {
    if rgba.len() != (width * height * 4) as usize {
        return ffi::RasterResult { width: 0, height: 0, data: vec![], error: "RGBA buffer size mismatch".into() };
    }
    let data = esdt::compute_rgba_esdt(width as usize, height as usize, rgba, spread);
    ffi::RasterResult { width, height, data, error: String::new() }
}

pub fn compute_esdt_rgba32f(width: u32, height: u32, rgba: &[u8]) -> ffi::RasterResultF32 {
    if rgba.len() != (width * height * 4) as usize {
        return ffi::RasterResultF32 { width: 0, height: 0, data: vec![], error: "RGBA buffer size mismatch".into() };
    }
    let data = esdt::compute_rgba_esdt_f32(width as usize, height as usize, rgba);
    ffi::RasterResultF32 { width, height, data, error: String::new() }
}


struct DummyProgress;
impl Progress for DummyProgress {
    fn print_start(&mut self) {}
    fn print_progress(&mut self, _: &DownloadState) {}
    fn print_finish(&mut self, _: &DownloadState) {}
}

fn json_to_value(v: serde_json::Value) -> Value {
    match v {
        serde_json::Value::Null => Value::None,
        serde_json::Value::Bool(b) => Value::Bool(b),
        serde_json::Value::Number(n) => {
            if let Some(i) = n.as_i64() {
                Value::Int(i)
            } else if let Some(f) = n.as_f64() {
                Value::Float(f)
            } else {
                Value::None
            }
        }
        serde_json::Value::String(s) => Value::Str(Str::from(s)),
        serde_json::Value::Array(a) => {
            let vec: Vec<Value> = a.into_iter().map(json_to_value).collect();
            Value::Array(typst::foundations::Array::from_iter(vec))
        }
        serde_json::Value::Object(map) => {
            let mut dict = Dict::new();
            for (k, v) in map {
                dict.insert(Str::from(k), json_to_value(v));
            }
            Value::Dict(dict)
        }
    }
}

fn json_to_dict(json_str: &str) -> Result<Dict, String> {
    let parsed: serde_json::Value = serde_json::from_str(json_str).map_err(|e| e.to_string())?;
    if let serde_json::Value::Object(map) = parsed {
        let mut dict = Dict::new();
        for (k, v) in map {
            dict.insert(Str::from(k), json_to_value(v));
        }
        Ok(dict)
    } else {
        Err("Root of parameters must be a JSON object map".to_string())
    }
}

fn get_fonts() -> &'static (FontBook, Vec<FontSlot>) {
    static FONTS: OnceLock<(FontBook, Vec<FontSlot>)> = OnceLock::new();
    FONTS.get_or_init(|| {
        let fonts = typst_kit::fonts::FontSearcher::new().search();
        (fonts.book, fonts.fonts)
    })
}

struct MemoryWorld {
    library: LazyHash<Library>,
    book: LazyHash<FontBook>,
    fonts: &'static [FontSlot],
    source: Source,
    main: FileId,
    package_storage: PackageStorage,
}

impl MemoryWorld {
    fn new(code: &str, inputs: Option<Dict>) -> Self {
        let mut lib_builder = Library::builder();
        if let Some(dict) = inputs {
            lib_builder = lib_builder.with_inputs(dict);
        }
        let library = LazyHash::new(lib_builder.build());

        let fonts_data = get_fonts();

        let main_id = FileId::new(None, VirtualPath::new("main.typ"));
        let source = Source::new(main_id, code.to_string());

        Self {
            library,
            book: LazyHash::new(fonts_data.0.clone()),
            fonts: fonts_data.1.as_slice(),
            source,
            main: main_id,
            package_storage: PackageStorage::new(None, None, Downloader::new("typst-cxx-bridge")),
        }
    }
}

impl World for MemoryWorld {
    fn library(&self) -> &LazyHash<Library> {
        &self.library
    }

    fn book(&self) -> &LazyHash<FontBook> {
        &self.book
    }

    fn main(&self) -> FileId {
        self.main
    }

    fn source(&self, id: FileId) -> FileResult<Source> {
        if id == self.main {
            Ok(self.source.clone())
        } else {
            let bytes = self.file(id)?;
            let text = std::str::from_utf8(&bytes).map_err(|_| FileError::InvalidUtf8)?;
            Ok(Source::new(id, text.to_string()))
        }
    }

    fn file(&self, id: FileId) -> FileResult<Bytes> {
        if let Some(package) = id.package() {
            let mut progress = DummyProgress;
            let package_dir = self
                .package_storage
                .prepare_package(package, &mut progress)
                .map_err(|err| FileError::Other(Some(err.to_string().into())))?;

            let path = package_dir.join(id.vpath().as_rootless_path());
            let data = std::fs::read(&path).map_err(|err| match err.kind() {
                std::io::ErrorKind::NotFound => FileError::NotFound(path.into()),
                std::io::ErrorKind::PermissionDenied => FileError::AccessDenied,
                _ => FileError::Other(Some(err.to_string().into())),
            })?;

            return Ok(Bytes::new(data));
        }

        Err(FileError::NotFound(id.vpath().as_rootless_path().into()))
    }

    fn today(&self, _offset: Option<i64>) -> Option<Datetime> {
        let now = Local::now();
        Datetime::from_ymd(now.year(), now.month() as u8, now.day() as u8)
    }

    fn font(&self, index: usize) -> Option<Font> {
        self.fonts.get(index).and_then(|slot| slot.get())
    }
}

pub fn render_typst_to_svg(code: &str) -> ffi::RenderResult {
    let world = MemoryWorld::new(code, None);

    match typst::compile::<typst::layout::PagedDocument>(&world).output {
        Ok(doc) => {
            if doc.pages.is_empty() {
                return ffi::RenderResult {
                    svg: String::new(),
                    error: String::new(),
                };
            }
            let svg = typst_svg::svg_frame(&doc.pages[0].frame);
            ffi::RenderResult {
                svg,
                error: String::new(),
            }
        }
        Err(errs) => ffi::RenderResult {
            svg: String::new(),
            error: format!("{:?}", errs),
        },
    }
}

pub fn render_typst_sequence(template_code: &str, params_json: &str) -> ffi::SvgSequenceResult {
    let inputs = match json_to_dict(params_json) {
        Ok(d) => d,
        Err(e) => {
            return ffi::SvgSequenceResult {
                svgs: vec![],
                error: e,
            }
        }
    };

    let world = MemoryWorld::new(template_code, Some(inputs));

    match typst::compile::<typst::layout::PagedDocument>(&world).output {
        Ok(doc) => {
            let svgs: Vec<String> = doc.pages.iter().map(|page| typst_svg::svg_frame(&page.frame)).collect();
            ffi::SvgSequenceResult {
                svgs,
                error: String::new(),
            }
        }
        Err(errs) => ffi::SvgSequenceResult {
            svgs: vec![],
            error: format!("{:?}", errs),
        },
    }
}