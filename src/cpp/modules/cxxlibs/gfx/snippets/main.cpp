#include <cstring>
#include <iostream>
#include <string>
#include "rust_bridge/lib.h"
#include <OpenImageIO/imageio.h>

using namespace OIIO;

bool save_image(const std::string& filename, int width, int height, int channels, const uint8_t* data) {
    auto out = ImageOutput::create(filename);
    if (!out) {
        std::cerr << "Could not create output file: " << filename << std::endl;
        return false;
    }
    
    ImageSpec spec(width, height, channels, TypeDesc::UINT8);
    out->open(filename, spec);
    out->write_image(TypeDesc::UINT8, data);
    out->close();
    
    return true;
}

int main() {
    std::string typst_code = 
        "= SDF Rendering Demo\n\n"
        "Testing Typst -> SVG -> PNG -> SDF via Rust and C++!";

    // Settings
    float scale = 2.0f;
    uint32_t canvas_w = 1920;
    uint32_t canvas_h = 1080;
    float sdf_radius = 12.0f;

    std::cout << "1. Converting Typst to SVG..." << std::endl;
    auto svg_res = render_typst_to_svg(typst_code);
    if (!svg_res.error.empty()) {
        std::cerr << "Typst to SVG Error: " << std::string(svg_res.error) << std::endl;
        return 1;
    }

    std::cout << "2. Rasterizing SVG to 4K Canvas (RGBA)..." << std::endl;
    auto raster_res = rasterize_svg(svg_res.svg, scale, canvas_w, canvas_h);
    if (!raster_res.error.empty()) {
        std::cerr << "Rasterize Error: " << std::string(raster_res.error) << std::endl;
        return 1;
    }
    
    if (save_image("output_color.png", raster_res.width, raster_res.height, 4, raster_res.data.data())) {
        std::cout << " -> Saved output_color.png" << std::endl;
    }

    std::cout << "3. Generating SDF Map (Grayscale)..." << std::endl;
    auto sdf_res = render_typst_to_sdf(typst_code, scale, sdf_radius, canvas_w, canvas_h);
    if (!sdf_res.error.empty()) {
        std::cerr << "SDF Error: " << std::string(sdf_res.error) << std::endl;
        return 1;
    }

    if (save_image("output_sdf.png", sdf_res.width, sdf_res.height, 1, sdf_res.data.data())) {
        std::cout << " -> Saved output_sdf.png" << std::endl;
    }

    std::cout << "Demo completed successfully." << std::endl;
    return 0;
}