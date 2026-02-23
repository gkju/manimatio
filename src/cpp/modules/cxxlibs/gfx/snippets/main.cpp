#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include "rust_bridge/lib.h"
#include <OpenImageIO/imageio.h>

using namespace OIIO;
namespace fs = std::filesystem;

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
    fs::create_directories("frames/mono");
    fs::create_directories("frames/color");

    float scale = 8.0f;
    uint32_t grid_size = 2048; 
    uint32_t force_w = 0;
    uint32_t force_h = 0;
    int num_frames = 60;
    float crisp_aa_width = 1.0f; 

    // ========================================================================
    // Grayscale/Alpha SDF Morphing
    // ========================================================================
    std::cout << "--- Generating Mono SDF Morphing ---" << std::endl;
    std::string typst_A_mono = 
        "#set page(width: auto, height: auto, margin: 10pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[Rust]";
        
    std::string typst_B_mono = 
        "#set page(width: auto, height: auto, margin: 10pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[C++]";

    auto sdf_mono_A = render_typst_to_sdf_f32(typst_A_mono, scale, grid_size, force_w, force_h);
    if (!sdf_mono_A.error.empty()) { std::cerr << "SDF A Error: " << std::string(sdf_mono_A.error) << std::endl; return 1; }

    auto sdf_mono_B = render_typst_to_sdf_f32(typst_B_mono, scale, grid_size, force_w, force_h);
    if (!sdf_mono_B.error.empty()) { std::cerr << "SDF B Error: " << std::string(sdf_mono_B.error) << std::endl; return 1; }

    if (sdf_mono_A.width != sdf_mono_B.width || sdf_mono_A.height != sdf_mono_B.height) {
        std::cerr << "Grid mismatch!" << std::endl; return 1;
    }

    size_t np_mono = sdf_mono_A.width * sdf_mono_A.height;
    std::vector<uint8_t> frame_buffer_mono(np_mono);

    for (int i = 0; i < num_frames; ++i) {
        float t = i / (float)(num_frames - 1);
        float smooth_t = t * t * (3.0f - 2.0f * t);

        for (size_t p = 0; p < np_mono; ++p) {
            float dist_A = sdf_mono_A.data[p];
            float dist_B = sdf_mono_B.data[p];
            
            float d_blend = dist_A * (1.0f - smooth_t) + dist_B * smooth_t;
            float alpha = 0.5f - (d_blend / crisp_aa_width);
            
            alpha = std::max(0.0f, std::min(1.0f, alpha)); 
            frame_buffer_mono[p] = static_cast<uint8_t>(alpha * 255.0f);
        }

        std::ostringstream ss;
        ss << "frames/mono/frame_" << std::setw(2) << std::setfill('0') << i << ".png";
        save_image(ss.str(), sdf_mono_A.width, sdf_mono_A.height, 1, frame_buffer_mono.data());
        std::cout << "\rSaved " << ss.str() << std::flush;
    }
    std::cout << std::endl;

    // ========================================================================
    // RGBA Color-Propagated SDF Morphing
    // ========================================================================
    std::cout << "\n--- Generating RGBA Color SDF Morphing ---" << std::endl;
    
    // Testing the package downloader by fetching cetz and drawing an orange circle
    std::string typst_A_color = R"(
#import "@preview/cetz:0.3.2"
#set page(width: auto, height: auto, margin: 10pt)
#cetz.canvas({
  import cetz.draw: *
  circle((0,0), radius: 2, fill: rgb("F74C00"), stroke: none)
})
)";
        
    // Drawing a blue square with the exact same 4x4 intrinsic bounds
    std::string typst_B_color = R"(
#import "@preview/cetz:0.3.2"
#set page(width: auto, height: auto, margin: 10pt)
#cetz.canvas({
  import cetz.draw: *
  rect((-2, -2), (2, 2), fill: rgb("00599C"), stroke: none)
})
)";

    auto sdf_color_A = render_typst_to_rgba_esdt_f32(typst_A_color, scale, grid_size, force_w, force_h);
    if (!sdf_color_A.error.empty()) { std::cerr << "Color SDF A Error: " << std::string(sdf_color_A.error) << std::endl; return 1; }

    auto sdf_color_B = render_typst_to_rgba_esdt_f32(typst_B_color, scale, grid_size, force_w, force_h);
    if (!sdf_color_B.error.empty()) { std::cerr << "Color SDF B Error: " << std::string(sdf_color_B.error) << std::endl; return 1; }

    size_t np_color = sdf_color_A.width * sdf_color_A.height;
    std::vector<uint8_t> frame_buffer_color(np_color * 4); // RGBA interleaved

    for (int i = 0; i < num_frames; ++i) {
        float t = i / (float)(num_frames - 1);
        float smooth_t = t * t * (3.0f - 2.0f * t);

        for (size_t p = 0; p < np_color; ++p) {
            size_t p4 = p * 4;

            // Structure returned by render_typst_to_rgba_esdt_f32: 
            // [R(float), G(float), B(float), SDF_Distance(float)]
            float r_A = sdf_color_A.data[p4 + 0];
            float g_A = sdf_color_A.data[p4 + 1];
            float b_A = sdf_color_A.data[p4 + 2];
            float d_A = sdf_color_A.data[p4 + 3];

            float r_B = sdf_color_B.data[p4 + 0];
            float g_B = sdf_color_B.data[p4 + 1];
            float b_B = sdf_color_B.data[p4 + 2];
            float d_B = sdf_color_B.data[p4 + 3];
            
            // Blend the physical distance map
            float d_blend = d_A * (1.0f - smooth_t) + d_B * smooth_t;
            float alpha = 0.5f - (d_blend / crisp_aa_width);
            alpha = std::max(0.0f, std::min(1.0f, alpha)); 

            // Blend the RGB colors cleanly 
            float r_blend = r_A * (1.0f - smooth_t) + r_B * smooth_t;
            float g_blend = g_A * (1.0f - smooth_t) + g_B * smooth_t;
            float b_blend = b_A * (1.0f - smooth_t) + b_B * smooth_t;

            // Write interleaved 8-bit RGBA out
            frame_buffer_color[p4 + 0] = static_cast<uint8_t>(std::clamp(r_blend * 255.0f, 0.0f, 255.0f));
            frame_buffer_color[p4 + 1] = static_cast<uint8_t>(std::clamp(g_blend * 255.0f, 0.0f, 255.0f));
            frame_buffer_color[p4 + 2] = static_cast<uint8_t>(std::clamp(b_blend * 255.0f, 0.0f, 255.0f));
            frame_buffer_color[p4 + 3] = static_cast<uint8_t>(alpha * 255.0f);
        }

        std::ostringstream ss;
        ss << "frames/color/frame_" << std::setw(2) << std::setfill('0') << i << ".png";
        save_image(ss.str(), sdf_color_A.width, sdf_color_A.height, 4, frame_buffer_color.data());
        std::cout << "\rSaved " << ss.str() << std::flush;
    }
    
    std::cout << "\nDemo completed!" << std::endl;
    return 0;
}