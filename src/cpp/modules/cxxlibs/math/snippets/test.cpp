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

  // Standard math functions
  auto y = math::sin(t) + math::cos(t);
  auto z = math::pow(t, math::Expr<float>(2.0f)); // t²
  auto w = math::exp(-t * t);                     // gaussian

  // Custom lambda — any arity
  auto custom = math::apply([](float a, float b, float c) { return a * b + c; },
                            t, t + 1.0f, 3.0f);

  // Even captures work
  float scale = 2.5f;
  auto scaled = math::apply([scale](float x) { return x * scale; }, t);

  t = 1.0f;
  y.evaluate();      // sin(1) + cos(1)
  custom.evaluate(); // 1 * 2 + 3 = 5
  return 0;
}