#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
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
    // Generate two distinct shapes to morph between
    std::string typst_A = 
        "#set page(width: auto, height: auto, margin: 10pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[Rust]";
        
    std::string typst_B = 
        "#set page(width: auto, height: auto, margin: 10pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[C++]";

    float scale = 8.0f;
    
    // It must be large enough to bridge the physical pixel gap between shapes.
    float sdf_radius = 256.0f; 
    
    uint32_t grid_size = 2048; 
    uint32_t force_w = 0;
    uint32_t force_h = 0;

    std::cout << "1. Generating SDF A..." << std::endl;
    auto sdf_res_A = render_typst_to_sdf(typst_A, scale, sdf_radius, grid_size, force_w, force_h);
    if (!sdf_res_A.error.empty()) { std::cerr << "SDF A Error: " << std::string(sdf_res_A.error) << std::endl; return 1; }

    std::cout << "2. Generating SDF B..." << std::endl;
    auto sdf_res_B = render_typst_to_sdf(typst_B, scale, sdf_radius, grid_size, force_w, force_h);
    if (!sdf_res_B.error.empty()) { std::cerr << "SDF B Error: " << std::string(sdf_res_B.error) << std::endl; return 1; }

    if (sdf_res_A.width != sdf_res_B.width || sdf_res_A.height != sdf_res_B.height) {
        std::cerr << "Mismatch! A is " << sdf_res_A.width << "x" << sdf_res_A.height 
                  << " but B is " << sdf_res_B.width << "x" << sdf_res_B.height << std::endl;
        return 1;
    }

    std::cout << "3. Interpolating High-DPI Morph Sequence (" << sdf_res_A.width << "x" << sdf_res_A.height << ")...\n";
    
    int num_frames = 60;
    size_t np = sdf_res_A.width * sdf_res_A.height;
    std::vector<uint8_t> frame_buffer(np);

    float threshold = 191.25f; 
    
    float edge_aa_width = 2.0f * scale; 

    for (int i = 0; i < num_frames; ++i) {
        float t = i / (float)(num_frames - 1);
        
        float smooth_t = t * t * (3.0f - 2.0f * t);

        float cutoff = 0.25f;
        float crisp_aa_width = 1.0f; 

        for (size_t p = 0; p < np; ++p) {
            float valA = sdf_res_A.data[p] / 255.0f;
            float valB = sdf_res_B.data[p] / 255.0f;
            
            float d_A = sdf_radius * (1.0f - cutoff - valA);
            float d_B = sdf_radius * (1.0f - cutoff - valB);
            
            float d_blend = d_A * (1.0f - smooth_t) + d_B * smooth_t;
            
            float alpha = 0.5f - (d_blend / (2.0f * crisp_aa_width));
            
            alpha = std::max(0.0f, std::min(1.0f, alpha)); 
            
            frame_buffer[p] = static_cast<uint8_t>(alpha * 255.0f);
        }

        std::ostringstream ss;
        ss << "frame_" << std::setw(2) << std::setfill('0') << i << ".png";
        
        save_image(ss.str(), sdf_res_A.width, sdf_res_A.height, 1, frame_buffer.data());
        std::cout << "\rSaved " << ss.str() << std::flush;
    }
    
    std::cout << "\nHigh-DPI Demo completed successfully!" << std::endl;
    return 0;
}