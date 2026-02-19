#include <cassert>
#include <cmath>
#include <iostream>

import math;

using namespace math;

bool almost_equal(double a, double b, double epsilon = 1e-9) {
  return std::abs(a - b) < epsilon;
}

int main() {
  std::cout << "Running Math Expression DAG unit tests...\n\n";

  {
    std::cout << "1. Testing Basic Arithmetic... ";
    Expr<double> a = 5.0;
    Expr<double> b = 2.0;

    auto add = a + b;
    auto sub = a - b;
    auto mul = a * b;
    auto div = a / b;

    assert(almost_equal(add.evaluate(), 7.0));
    assert(almost_equal(sub.evaluate(), 3.0));
    assert(almost_equal(mul.evaluate(), 10.0));
    assert(almost_equal(div.evaluate(), 2.5));
    std::cout << "Passed!\n";
  }

  {
    std::cout << "2. Testing Parameter Updates & Lazy Evaluation... ";
    Param<double> x(10.0);
    auto y = x * 2.0;

    assert(almost_equal(y.evaluate(), 20.0));

    x = 5.0; // Update param
    assert(almost_equal(y.evaluate(), 10.0));

    x += 3.0; // x is now 8.0
    assert(almost_equal(y.evaluate(), 16.0));
    std::cout << "Passed!\n";
  }

  {
    std::cout << "3. Testing Built-in Math Functions... ";
    Param<double> angle(0.0);

    auto s = sin(angle);
    auto c = cos(angle);

    assert(almost_equal(s.evaluate(), 0.0));
    assert(almost_equal(c.evaluate(), 1.0));

    angle = M_PI / 2.0;

    assert(almost_equal(s.evaluate(), 1.0));
    assert(almost_equal(c.evaluate(), 0.0));
    std::cout << "Passed!\n";
  }

  {
    std::cout << "4. Testing Complex DAG (Diamond Dependency)... ";
    Param<double> x(2.0);

    auto branch1 = x * 3.0;        // 2.0 * 3.0 = 6.0
    auto branch2 = x + 4.0;        // 2.0 + 4.0 = 6.0
    auto root = branch1 * branch2; // 6.0 * 6.0 = 36.0

    assert(almost_equal(root.evaluate(), 36.0));

    // Mutating x should propagate through both branches up to the root
    x = 3.0;
    // branch1 = 9.0, branch2 = 7.0, root = 63.0
    assert(almost_equal(root.evaluate(), 63.0));
    std::cout << "Passed!\n";
  }

  {
    std::cout << "5. Testing Unary & Increment Operators... ";
    Param<int> counter(5);

    auto neg = -counter;
    assert(neg.evaluate() == -5);

    auto old_val = counter++; // Evaluates to the snapshot: 5

    assert(old_val.evaluate() == 5);
    assert(counter.evaluate() == 6);
    assert(neg.evaluate() == -6); // Negation tracks updated counter
    std::cout << "Passed!\n";
  }

  {
    std::cout << "6. Testing Variadic Apply (pow, max)... ";
    Param<double> base(2.0);
    Param<double> exponent(3.0);

    auto p = pow(base, exponent);
    assert(almost_equal(p.evaluate(), 8.0));

    base = 3.0;
    exponent = 2.0;
    assert(almost_equal(p.evaluate(), 9.0));

    auto m = max(base, exponent);
    assert(almost_equal(m.evaluate(), 3.0));
    std::cout << "Passed!\n";
  }

  std::cout << "\nAll DAG tests passed successfully!\n";
  return 0;
}