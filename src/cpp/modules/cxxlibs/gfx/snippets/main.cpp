#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cmath>
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

enum class Align {
    Center,
    TopLeft
};

void calculate_offsets(int img_w, int img_h, int canvas_w, int canvas_h, Align align, int& ox, int& oy) {
    if (align == Align::Center) {
        ox = (canvas_w - img_w) / 2;
        oy = (canvas_h - img_h) / 2;
    } else {
        ox = 0;
        oy = 0;
    }
}

// Places a bounded mono SDF onto a fixed sized canvas.
// Out-of-bounds areas are given a large positive distance.
std::vector<float> align_mono_sdf(const RasterResultF32& res, int canvas_w, int canvas_h, Align align) {
    std::vector<float> canvas(canvas_w * canvas_h, 1000.0f);
    int img_w = res.width;
    int img_h = res.height;
    int ox, oy;
    calculate_offsets(img_w, img_h, canvas_w, canvas_h, align, ox, oy);

    for (int cy = 0; cy < canvas_h; ++cy) {
        for (int cx = 0; cx < canvas_w; ++cx) {
            int ix = cx - ox;
            int iy = cy - oy;
            
            if (ix >= 0 && ix < img_w && iy >= 0 && iy < img_h) {
                canvas[cy * canvas_w + cx] = res.data[iy * img_w + ix];
            }
        }
    }
    return canvas;
}

// Places a bounded color SDF onto a fixed sized canvas.
// Out-of-bounds areas are transparent with a large positive distance.
std::vector<float> align_color_sdf(const RasterResultF32& res, int canvas_w, int canvas_h, Align align) {
    std::vector<float> canvas(canvas_w * canvas_h * 4, 0.0f);
    int img_w = res.width;
    int img_h = res.height;
    int ox, oy;
    calculate_offsets(img_w, img_h, canvas_w, canvas_h, align, ox, oy);

    for (int i = 0; i < canvas_w * canvas_h; ++i) {
        canvas[i * 4 + 3] = 1000.0f;
    }

    for (int cy = 0; cy < canvas_h; ++cy) {
        for (int cx = 0; cx < canvas_w; ++cx) {
            int ix = cx - ox;
            int iy = cy - oy;
            
            if (ix >= 0 && ix < img_w && iy >= 0 && iy < img_h) {
                int img_idx = (iy * img_w + ix) * 4;
                int c_idx = (cy * canvas_w + cx) * 4;
                
                canvas[c_idx + 0] = res.data[img_idx + 0]; 
                canvas[c_idx + 1] = res.data[img_idx + 1]; 
                canvas[c_idx + 2] = res.data[img_idx + 2]; 
                canvas[c_idx + 3] = res.data[img_idx + 3]; 
            }
        }
    }
    return canvas;
}

int main() {
    fs::create_directories("frames/mono");
    fs::create_directories("frames/color");
    float target_dpi = 768.0f; 
    const int CANVAS_W = 1920; 
    const int CANVAS_H = 1080;
    
    int num_frames = 60;
    float crisp_aa_width = 1.0f; 

    // ========================================================================
    // Grayscale/Alpha SDF Morphing
    // ========================================================================
    std::cout << "--- Generating Mono SDF Morphing ---" << std::endl;
    std::string typst_A_mono = 
        "#set page(width: auto, height: auto, margin: 40pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[Rust]";
        
    std::string typst_B_mono = 
        "#set page(width: auto, height: auto, margin: 40pt)\n"
        "#text(size: 80pt, weight: \"bold\", fill: black)[C++]";

    auto sdf_raw_A = render_typst_to_sdf_f32(typst_A_mono, target_dpi);
    if (!sdf_raw_A.error.empty()) { std::cerr << "SDF A Error: " << std::string(sdf_raw_A.error) << std::endl; return 1; }

    auto sdf_raw_B = render_typst_to_sdf_f32(typst_B_mono, target_dpi);
    if (!sdf_raw_B.error.empty()) { std::cerr << "SDF B Error: " << std::string(sdf_raw_B.error) << std::endl; return 1; }

    auto sdf_mono_A = align_mono_sdf(sdf_raw_A, CANVAS_W, CANVAS_H, Align::Center);
    auto sdf_mono_B = align_mono_sdf(sdf_raw_B, CANVAS_W, CANVAS_H, Align::Center);

    size_t np_mono = CANVAS_W * CANVAS_H;
    std::vector<uint8_t> frame_buffer_mono(np_mono);

    for (int i = 0; i < num_frames; ++i) {
        float t = i / (float)(num_frames - 1);
        float smooth_t = t * t * (3.0f - 2.0f * t);

        for (size_t p = 0; p < np_mono; ++p) {
            float dist_A = sdf_mono_A[p];
            float dist_B = sdf_mono_B[p];
            
            float d_blend = dist_A * (1.0f - smooth_t) + dist_B * smooth_t;
            float alpha = 0.5f - (d_blend / crisp_aa_width);
            
            alpha = std::clamp(alpha, 0.0f, 1.0f); 
            frame_buffer_mono[p] = static_cast<uint8_t>(alpha * 255.0f);
        }

        std::ostringstream ss;
        ss << "frames/mono/frame_" << std::setw(2) << std::setfill('0') << i << ".png";
        save_image(ss.str(), CANVAS_W, CANVAS_H, 1, frame_buffer_mono.data());
        std::cout << "\rSaved " << ss.str() << std::flush;
    }
    std::cout << std::endl;

    // ========================================================================
    // RGBA Color-Propagated SDF Morphing
    // ========================================================================
    std::cout << "\n--- Generating RGBA Color SDF Morphing ---" << std::endl;
    
    std::string typst_A_color = R"(
#import "@preview/cetz:0.3.2"
#set page(width: auto, height: auto, margin: 40pt)
#cetz.canvas({
  import cetz.draw: *
  circle((0,0), radius: 1, fill: rgb("F74C00"), stroke: none)
})
)";
        
    std::string typst_B_color = R"(
#import "@preview/cetz:0.3.2"
#set page(width: auto, height: auto, margin: 40pt)
#cetz.canvas({
  import cetz.draw: *
  rect((-1, -1), (1, 1), fill: rgb("00599C"), stroke: none)
})
)";

    auto raw_color_A = render_typst_to_rgba_esdt_f32(typst_A_color, target_dpi);
    if (!raw_color_A.error.empty()) { std::cerr << "Color SDF A Error: " << std::string(raw_color_A.error) << std::endl; return 1; }

    auto raw_color_B = render_typst_to_rgba_esdt_f32(typst_B_color, target_dpi);
    if (!raw_color_B.error.empty()) { std::cerr << "Color SDF B Error: " << std::string(raw_color_B.error) << std::endl; return 1; }

    auto sdf_color_A = align_color_sdf(raw_color_A, CANVAS_W, CANVAS_H, Align::Center);
    auto sdf_color_B = align_color_sdf(raw_color_B, CANVAS_W, CANVAS_H, Align::Center);

    size_t np_color = CANVAS_W * CANVAS_H;
    std::vector<uint8_t> frame_buffer_color(np_color * 4); 

    for (int i = 0; i < num_frames; ++i) {
        float t = i / (float)(num_frames - 1);
        float smooth_t = t * t * (3.0f - 2.0f * t);

        for (size_t p = 0; p < np_color; ++p) {
            size_t p4 = p * 4;

            float r_A = sdf_color_A[p4 + 0];
            float g_A = sdf_color_A[p4 + 1];
            float b_A = sdf_color_A[p4 + 2];
            float d_A = sdf_color_A[p4 + 3];

            float r_B = sdf_color_B[p4 + 0];
            float g_B = sdf_color_B[p4 + 1];
            float b_B = sdf_color_B[p4 + 2];
            float d_B = sdf_color_B[p4 + 3];
            
            float d_blend = d_A * (1.0f - smooth_t) + d_B * smooth_t;
            float alpha = std::clamp(0.5f - (d_blend / crisp_aa_width), 0.0f, 1.0f);

            float r_blend = r_A * (1.0f - smooth_t) + r_B * smooth_t;
            float g_blend = g_A * (1.0f - smooth_t) + g_B * smooth_t;
            float b_blend = b_A * (1.0f - smooth_t) + b_B * smooth_t;

            float rp = r_blend * alpha;
            float gp = g_blend * alpha;
            float bp = b_blend * alpha;

            frame_buffer_color[p4 + 0] = static_cast<uint8_t>(std::clamp(rp * 255.0f, 0.0f, 255.0f));
            frame_buffer_color[p4 + 1] = static_cast<uint8_t>(std::clamp(gp * 255.0f, 0.0f, 255.0f));
            frame_buffer_color[p4 + 2] = static_cast<uint8_t>(std::clamp(bp * 255.0f, 0.0f, 255.0f));
            frame_buffer_color[p4 + 3] = static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f));
        }

        std::ostringstream ss;
        ss << "frames/color/frame_" << std::setw(2) << std::setfill('0') << i << ".png";
        save_image(ss.str(), CANVAS_W, CANVAS_H, 4, frame_buffer_color.data());
        std::cout << "\rSaved " << ss.str() << std::flush;
    }
    
    std::cout << "\nDemo completed!" << std::endl;
    return 0;
}