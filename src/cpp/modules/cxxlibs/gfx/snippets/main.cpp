#include <cstring>
#include <print>

#include "core/SkCanvas.h"
#include "core/SkImageInfo.h"
#include "core/SkStream.h"
#include "core/SkSurface.h"
#include "modules/svg/include/SkSVGDOM.h"

#include "rust_bridge/lib.h"

int main() {

  auto res = render_typst_to_svg("Test");

  const char *svg_text = res.svg.c_str();

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

  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);

  svg_dom->setContainerSize(SkSize::Make(100, 100));
  svg_dom->render(canvas);

  std::print(
      "✅ SkSVGDOM Success: SVG successfully rendered to the Skia canvas!\n");

  return 0;
}