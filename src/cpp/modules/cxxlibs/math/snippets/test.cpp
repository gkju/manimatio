import math;
#include <print>

int main() {
  {
    math::Param<float> t = 1.0f;
    auto w = t + 3;
    auto z = w * 2 - t / 4;

    for (int i = 0; i < 100; i++) {
      std::print("t = {}, w = t+3 = {}, z = w*2-t/4 = {}\n", t.get(),
                 w.evaluate(), z.evaluate());
      t++;
    }
  }
  math::Param<float> t(0.5f);

  auto y = math::sin(t) + math::cos(t);
  auto z = math::pow(t, math::Expr<float>(2.0f));
  auto w = math::exp(-t * t);

  auto custom = math::apply([](float a, float b, float c) { return a * b + c; },
                            t, t + 1.0f, 3.0f);

  float scale = 2.5f;
  auto scaled = math::apply([scale](float x) { return x * scale; }, t);

  t = 1.0f;
  std::print("t = {}, sin(t) + cos(t) = {}, pow(t, 2) = {}, exp(-t*t) = {}, "
             "custom = {}, scaled = {}\n",
             t.get(), y.evaluate(), z.evaluate(), w.evaluate(),
             custom.evaluate(), scaled.evaluate());
  math::Param<float> mass = 5.5f;
  math::Param<int> count = 10;
  // Heterogenous reduction with custom callable
  auto total_mass = math::apply(
      [](float m, int c) { return static_cast<double>(m * c); }, mass, count);

  for (int i = 0; i < 5; i++) {
    std::print("mass = {}, count = {}, total_mass = {}\n", mass.evaluate(),
               count.evaluate(), total_mass.evaluate());
    mass++;
    count++;
  }
}