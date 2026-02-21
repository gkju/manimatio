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
use sdfer::{esdt, Image2d, Unorm8};

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

    extern "Rust" {
        fn render_typst_to_svg(code: &str) -> RenderResult;
        fn render_typst_sequence(template_code: &str, params_json: &str) -> SvgSequenceResult;
        fn rasterize_svg(svg: &str, scale: f32, grid_size: u32, force_w: u32, force_h: u32) -> RasterResult;
        fn render_typst_to_sdf(code: &str, scale: f32, radius: f32, grid_size: u32, force_w: u32, force_h: u32) -> RasterResult;
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

    ffi::RasterResult { width: w, height: h, data: pixmap.take(), error: String::new() }
}

pub fn render_typst_to_sdf(code: &str, scale: f32, radius: f32, grid_size: u32, force_w: u32, force_h: u32) -> ffi::RasterResult {
    let svg_res = render_typst_to_svg(code);
    if !svg_res.error.is_empty() {
        return ffi::RasterResult { width: 0, height: 0, data: vec![], error: svg_res.error };
    }

    let raster_res = rasterize_svg(&svg_res.svg, scale, grid_size, force_w, force_h);
    if !raster_res.error.is_empty() { return raster_res; }

    let width = raster_res.width as usize;
    let height = raster_res.height as usize;
    
    let mut coverage = Vec::with_capacity(width * height);
    for chunk in raster_res.data.chunks_exact(4) {
        coverage.push(Unorm8::from_bits(chunk[3])); 
    }

    let mut input_image = Image2d::from_storage(width, height, coverage);
    
    let params = esdt::Params { radius, pad: 0, ..Default::default() };
    let (sdf_image, _) = esdt::glyph_to_sdf(&mut input_image, params, None);

    let mut sdf_bytes = Vec::with_capacity(width * height);
    for y in 0..height {
        for x in 0..width {
            sdf_bytes.push(sdf_image[(x, y)].to_bits());
        }
    }

    ffi::RasterResult { width: width as u32, height: height as u32, data: sdf_bytes, error: String::new() }
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
