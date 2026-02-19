#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

import math;

using namespace math;

TEST_CASE("Basic Arithmetic Operations", "[math][expr]") {
    Expr<double> a = 5.0;
    Expr<double> b = 2.0;

    SECTION("Addition") {
        REQUIRE((a + b).evaluate() == Catch::Approx(7.0));
    }
    SECTION("Subtraction") {
        REQUIRE((a - b).evaluate() == Catch::Approx(3.0));
    }
    SECTION("Multiplication") {
        REQUIRE((a * b).evaluate() == Catch::Approx(10.0));
    }
    SECTION("Division") {
        REQUIRE((a / b).evaluate() == Catch::Approx(2.5));
    }
}

TEST_CASE("Parameters and Lazy Evaluation", "[math][param]") {
    Param<double> x(10.0);
    auto y = x * 2.0;

    SECTION("Initial evaluation propagates correctly") {
        REQUIRE(y.evaluate() == Catch::Approx(20.0));
    }

    SECTION("Parameter updates propagate down the DAG") {
        x = 5.0; 
        REQUIRE(y.evaluate() == Catch::Approx(10.0));
    }

    SECTION("Compound assignment operators (+=, -=, *=, /=)") {
        x += 3.0; // x = 13.0
        REQUIRE(y.evaluate() == Catch::Approx(26.0));
        
        x -= 5.0; // x = 8.0
        REQUIRE(y.evaluate() == Catch::Approx(16.0));

        x *= 2.0; // x = 16.0
        REQUIRE(y.evaluate() == Catch::Approx(32.0));

        x /= 4.0; // x = 4.0
        REQUIRE(y.evaluate() == Catch::Approx(8.0));
    }

    SECTION("Unary Increment Operators") {
        Param<int> counter(5);
        auto neg = -counter;

        auto old_val = counter++; // Evaluates to snapshot (5)
        
        REQUIRE(old_val.evaluate() == 5);
        REQUIRE(counter.evaluate() == 6);
        REQUIRE(neg.evaluate() == -6); // Negation tracks updated counter
    }
}

TEST_CASE("Intrinsic Math Functions", "[math][intrinsics]") {
    Param<double> val(0.0);

    SECTION("Trigonometric functions (sin, cos, tan)") {
        REQUIRE(sin(val).evaluate() == Catch::Approx(0.0));
        REQUIRE(cos(val).evaluate() == Catch::Approx(1.0));
        REQUIRE(tan(val).evaluate() == Catch::Approx(0.0));

        val = M_PI / 2.0;
        REQUIRE(sin(val).evaluate() == Catch::Approx(1.0));
        REQUIRE(cos(val).evaluate() == Catch::Approx(0.0).margin(1e-9));
    }

    SECTION("Inverse Trigonometric functions (asin, acos, atan, atan2)") {
        val = 1.0;
        REQUIRE(asin(val).evaluate() == Catch::Approx(M_PI / 2.0));
        REQUIRE(acos(val).evaluate() == Catch::Approx(0.0));
        REQUIRE(atan(val).evaluate() == Catch::Approx(M_PI / 4.0));

        Expr<double> y = 10.0;
        Expr<double> x = 10.0;
        REQUIRE(atan2(y, x).evaluate() == Catch::Approx(M_PI / 4.0));
    }

    SECTION("Exponential and Logarithmic functions (exp, log)") {
        val = 1.0;
        REQUIRE(exp(val).evaluate() == Catch::Approx(M_E));
        
        val = M_E;
        REQUIRE(log(val).evaluate() == Catch::Approx(1.0));
    }

    SECTION("Power and Roots (pow, sqrt)") {
        val = 4.0;
        REQUIRE(sqrt(val).evaluate() == Catch::Approx(2.0));
        
        Expr<double> exponent = 3.0;
        REQUIRE(pow(val, exponent).evaluate() == Catch::Approx(64.0));
    }

    SECTION("Min, Max, Abs, and Negation") {
        val = -5.5;
        REQUIRE(abs(val).evaluate() == Catch::Approx(5.5));
        REQUIRE((-val).evaluate() == Catch::Approx(5.5));

        Expr<double> other = 2.0;
        REQUIRE(min(val, other).evaluate() == Catch::Approx(-5.5));
        REQUIRE(max(val, other).evaluate() == Catch::Approx(2.0));
    }
}

TEST_CASE("Complex DAG Evaluation (Diamond Dependency)", "[math][dag]") {
    Param<double> x(2.0);

    // Diamond dependency structure
    auto branch1 = x * 3.0;        
    auto branch2 = x + 4.0;        
    auto root = branch1 * branch2; 

    SECTION("Initial diamond evaluation") {
        // branch1 = 6.0, branch2 = 6.0, root = 36.0
        REQUIRE(root.evaluate() == Catch::Approx(36.0));
    }

    SECTION("Mutating root parameter updates both branches up to root") {
        x = 3.0;
        // branch1 = 9.0, branch2 = 7.0, root = 63.0
        REQUIRE(root.evaluate() == Catch::Approx(63.0));
    }
}

TEST_CASE("Custom functions using math::apply", "[math][apply]") {
    SECTION("Homogeneous arguments") {
        Param<float> t(2.0f);
        auto custom = apply([](float a, float b, float c) { return a * b + c; },
                            t, t + 1.0f, 3.0f);
        
        // 2.0 * 3.0 + 3.0 = 9.0
        REQUIRE(custom.evaluate() == Catch::Approx(9.0f));

        t = 3.0f;
        // 3.0 * 4.0 + 3.0 = 15.0
        REQUIRE(custom.evaluate() == Catch::Approx(15.0f));
    }

    SECTION("Heterogeneous arguments and reductions") {
        Param<float> mass(5.5f);
        Param<int> count(10);
        
        auto total_mass = apply(
            [](float m, int c) { return static_cast<double>(m * c); }, 
            mass, count
        );

        REQUIRE(total_mass.evaluate() == Catch::Approx(55.0));

        mass++;
        count++;
        // mass = 6.5, count = 11 -> 6.5 * 11 = 71.5
        REQUIRE(total_mass.evaluate() == Catch::Approx(71.5));
    }

    SECTION("Capturing external state in lambda") {
        Param<float> t(1.0f);
        float scale = 2.5f;
        auto scaled = apply([scale](float x) { return x * scale; }, t);
        
        REQUIRE(scaled.evaluate() == Catch::Approx(2.5f));
    }
}