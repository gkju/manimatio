module;

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module math:expression;
import :ast;
import :nodes;

export namespace math {

template <typename T> class Expr;
template <typename T>
Expr<T> make_unary_builtin(IntrinsicOpType op, const Expr<T> &e) {
  return Expr<T>(std::make_shared<IntrinsicNode<T, T>>(op, e.get_node()));
}

template <typename T1, typename T2>
auto make_binary_builtin(IntrinsicOpType op, const Expr<T1> &a, const Expr<T2> &b) {
  using R = decltype(std::declval<T1>() * std::declval<T2>());
  return Expr<R>(std::make_shared<IntrinsicNode<R, T1, T2>>(op, a.get_node(), b.get_node()));
}

template <typename T> class Expr {
protected:
  std::shared_ptr<ComputationNode<T>> node;

public:
  using value_type = T;

  Expr(std::shared_ptr<ComputationNode<T>> n) : node(std::move(n)) {}
  Expr(T value) : node(std::make_shared<ConstantNode<T>>(value)) {}
  T evaluate() const { return node->evaluate(); }

  std::shared_ptr<ComputationNode<T>> get_node() const { return node; }

  friend Expr operator+(const Expr &l, const Expr &r) {
    return make_binary_builtin(IntrinsicOpType::Add, l, r);
  }

  friend Expr operator-(const Expr &l, const Expr &r) {
    return make_binary_builtin(IntrinsicOpType::Sub, l, r);
  }

  friend Expr operator*(const Expr &l, const Expr &r) {
    return make_binary_builtin(IntrinsicOpType::Mul, l, r);
  }

  friend Expr operator/(const Expr &l, const Expr &r) {
    return make_binary_builtin(IntrinsicOpType::Div, l, r);
  }
};

template <typename T> class Param : public Expr<T> {
  std::shared_ptr<ParamNode<T>> val;

  static std::shared_ptr<ParamNode<T>> make(T v) {
    return std::make_shared<ParamNode<T>>(v);
  }

  explicit Param(std::shared_ptr<ParamNode<T>> v)
      : Expr<T>(v), val(std::move(v)) {}

public:
  Param(T value = T{}) : Param(make(value)) {}

  Param &operator=(T value) {
    val->set(value);
    return *this;
  }
  Param &operator+=(T value) {
    val->set(val->get() + value);
    return *this;
  }
  Param &operator-=(T value) {
    val->set(val->get() - value);
    return *this;
  }
  Param &operator*=(T value) {
    val->set(val->get() * value);
    return *this;
  }
  Param &operator/=(T value) {
    val->set(val->get() / value);
    return *this;
  }
  Param &operator++() {
    val->set(val->get() + 1);
    return *this;
  }
  Expr<T> operator++(int) {
    T current_val = val->get();
    val->set(current_val + 1);
    return Expr<T>(current_val);
  }

  void set(T value) { val->set(value); }
  T get() const { return val->get(); }
};
} // namespace math