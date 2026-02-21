#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
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
    std::string typst_A = 
        "#set page(width: auto, height: auto, margin: 10pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[Rust]";
        
    std::string typst_B = 
        "#set page(width: auto, height: auto, margin: 10pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[C++]";

    float scale = 8.0f;
    uint32_t grid_size = 2048; 
    uint32_t force_w = 0;
    uint32_t force_h = 0;

    std::cout << "1. Generating SDF A (f32 precision)..." << std::endl;
    auto sdf_res_A = render_typst_to_sdf_f32(typst_A, scale, grid_size, force_w, force_h);
    if (!sdf_res_A.error.empty()) { std::cerr << "SDF A Error: " << std::string(sdf_res_A.error) << std::endl; return 1; }

    std::cout << "2. Generating SDF B (f32 precision)..." << std::endl;
    auto sdf_res_B = render_typst_to_sdf_f32(typst_B, scale, grid_size, force_w, force_h);
    if (!sdf_res_B.error.empty()) { std::cerr << "SDF B Error: " << std::string(sdf_res_B.error) << std::endl; return 1; }

    if (sdf_res_A.width != sdf_res_B.width || sdf_res_A.height != sdf_res_B.height) {
        std::cerr << "Grid mismatch!" << std::endl;
        return 1;
    }

    std::cout << "3. Interpolating High-Precision Morph Sequence..." << std::endl;
    
    int num_frames = 60;
    size_t np = sdf_res_A.width * sdf_res_A.height;
    std::vector<uint8_t> frame_buffer(np);

    float crisp_aa_width = 1.0f; 

    for (int i = 0; i < num_frames; ++i) {
        float t = i / (float)(num_frames - 1);
        float smooth_t = t * t * (3.0f - 2.0f * t);

        for (size_t p = 0; p < np; ++p) {
            float dist_A = sdf_res_A.data[p];
            float dist_B = sdf_res_B.data[p];
            
            float d_blend = dist_A * (1.0f - smooth_t) + dist_B * smooth_t;
            
            float alpha = 0.5f - (d_blend / crisp_aa_width);
            
            alpha = std::max(0.0f, std::min(1.0f, alpha)); 
            
            frame_buffer[p] = static_cast<uint8_t>(alpha * 255.0f);
        }

        std::ostringstream ss;
        ss << "frame_" << std::setw(2) << std::setfill('0') << i << ".png";
        save_image(ss.str(), sdf_res_A.width, sdf_res_A.height, 1, frame_buffer.data());
        std::cout << "\rSaved " << ss.str() << std::flush;
    }
    
    std::cout << "\nDemo completed!" << std::endl;
    return 0;
}