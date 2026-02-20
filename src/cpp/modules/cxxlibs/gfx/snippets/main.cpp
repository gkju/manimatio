#include <print>
#include <cstring>

#include "core/SkCanvas.h"
#include "core/SkSurface.h"
#include "core/SkStream.h"
#include "core/SkImageInfo.h"

#include "modules/svg/include/SkSVGDOM.h"

int main() {
    const char* svg_text = R"(
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
            <circle cx="50" cy="50" r="40" stroke="green" stroke-width="4" fill="yellow" />
            <path d="M 30 50 L 70 50" stroke="blue" stroke-width="5" />
        </svg>
    )";

    SkMemoryStream stream(svg_text, std::strlen(svg_text));
    sk_sp<SkSVGDOM> svg_dom = SkSVGDOM::MakeFromStream(stream);

    if (!svg_dom) {
        std::print("❌ SkSVGDOM Failed: Could not parse the SVG string.\n");
        return 1;
    }
    std::print("✅ SkSVGDOM Success: SVG parsed into memory.\n");

    SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
    
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info); 
    
    if (!surface) {
        std::print("❌ SkSurface Failed: Could not create raster surface.\n");
        return 1;
    }

    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorWHITE);

    svg_dom->setContainerSize(SkSize::Make(100, 100));
    svg_dom->render(canvas);

    std::print("✅ SkSVGDOM Success: SVG successfully rendered to the Skia canvas!\n");

    return 0;
}