module;

#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

export module math:expression;

export namespace math {

template <typename T> class ComputationNode {
public:
  virtual ~ComputationNode() = default;
  virtual T evaluate() const = 0;
};

template <typename T> class ValueNode : public ComputationNode<T> {
  T value;

public:
  ValueNode(T val) : value(val) {}
  T evaluate() const override { return value; }
  void set(T val) { value = val; }
  T get() const { return value; }
};

template <typename T> class BinOpNode : public ComputationNode<T> {
public:
  enum class Op { Add, Sub, Mul, Div };

private:
  Op op;
  std::shared_ptr<ComputationNode<T>> left;
  std::shared_ptr<ComputationNode<T>> right;

public:
  BinOpNode(Op op, std::shared_ptr<ComputationNode<T>> l,
            std::shared_ptr<ComputationNode<T>> r)
      : op(op), left(std::move(l)), right(std::move(r)) {}

  Op get_op() const { return op; }
  const std::shared_ptr<ComputationNode<T>> &lhs() const { return left; }
  const std::shared_ptr<ComputationNode<T>> &rhs() const { return right; }

  T evaluate() const override {
    T l = left->evaluate(), r = right->evaluate();
    switch (op) {
    case Op::Add:
      return l + r;
    case Op::Sub:
      return l - r;
    case Op::Mul:
      return l * r;
    case Op::Div:
      return l / r;
    }
    __builtin_unreachable();
  }
};

template <typename T, typename F, std::size_t... Is>
class FuncNode : public ComputationNode<T> {
  F func;
  std::array<std::shared_ptr<ComputationNode<T>>, sizeof...(Is)> args;

public:
  FuncNode(F f,
           std::array<std::shared_ptr<ComputationNode<T>>, sizeof...(Is)> a)
      : func(std::move(f)), args(std::move(a)) {}

  T evaluate() const override { return func(args[Is]->evaluate()...); }
};

template <typename T> class Expr;

template <typename T, typename F, std::size_t... Is>
Expr<T>
apply_impl(F &&f,
           std::array<std::shared_ptr<ComputationNode<T>>, sizeof...(Is)> args,
           std::index_sequence<Is...>) {
  return Expr<T>(std::make_shared<FuncNode<T, std::decay_t<F>, Is...>>(
      std::forward<F>(f), std::move(args)));
}

template <typename T> class Expr {
  std::shared_ptr<ComputationNode<T>> node;

  static Expr binop(typename BinOpNode<T>::Op op, const Expr &l,
                    const Expr &r) {
    return Expr(std::make_shared<BinOpNode<T>>(op, l.node, r.node));
  }

public:
  Expr(std::shared_ptr<ComputationNode<T>> n) : node(std::move(n)) {}
  Expr(T value) : node(std::make_shared<ValueNode<T>>(value)) {}
  T evaluate() const { return node->evaluate(); }

  friend Expr operator+(const Expr &l, const Expr &r) {
    return binop(BinOpNode<T>::Op::Add, l, r);
  }
  friend Expr operator-(const Expr &l, const Expr &r) {
    return binop(BinOpNode<T>::Op::Sub, l, r);
  }
  friend Expr operator*(const Expr &l, const Expr &r) {
    return binop(BinOpNode<T>::Op::Mul, l, r);
  }
  friend Expr operator/(const Expr &l, const Expr &r) {
    return binop(BinOpNode<T>::Op::Div, l, r);
  }
};

template <typename T> class Param : public Expr<T> {
  std::shared_ptr<ValueNode<T>> val;

  static std::shared_ptr<ValueNode<T>> make(T v) {
    return std::make_shared<ValueNode<T>>(v);
  }

  explicit Param(std::shared_ptr<ValueNode<T>> v)
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
  Param operator++(int) {
    Param temp = *this;
    val->set(val->get() + 1);
    return temp;
  }

  void set(T value) { val->set(value); }
  T get() const { return val->get(); }
};

template <typename T, typename F, typename... Exprs>
  requires(std::convertible_to<Exprs, Expr<T>> && ...)
Expr<T> apply(F &&f, const Exprs &...exprs) {
  constexpr auto N = sizeof...(Exprs);
  std::array<std::shared_ptr<ComputationNode<T>>, N> args{
      Expr<T>(exprs).get_node()...};
  return apply_impl<T>(std::forward<F>(f), std::move(args),
                       std::make_index_sequence<N>{});
}

template <typename T, typename F> Expr<T> apply(F &&f, const Expr<T> &e) {
  std::array<std::shared_ptr<ComputationNode<T>>, 1> args{e.get_node()};
  return apply_impl<T>(std::forward<F>(f), std::move(args),
                       std::make_index_sequence<1>{});
}

template <typename T, typename F>
Expr<T> apply(F &&f, const Expr<T> &a, const Expr<T> &b) {
  std::array<std::shared_ptr<ComputationNode<T>>, 2> args{a.get_node(),
                                                          b.get_node()};
  return apply_impl<T>(std::forward<F>(f), std::move(args),
                       std::make_index_sequence<2>{});
}

template <typename T> Expr<T> sin(const Expr<T> &e) {
  return apply([](T x) { return std::sin(x); }, e);
}
template <typename T> Expr<T> cos(const Expr<T> &e) {
  return apply([](T x) { return std::cos(x); }, e);
}
template <typename T> Expr<T> tan(const Expr<T> &e) {
  return apply([](T x) { return std::tan(x); }, e);
}
template <typename T> Expr<T> asin(const Expr<T> &e) {
  return apply([](T x) { return std::asin(x); }, e);
}
template <typename T> Expr<T> acos(const Expr<T> &e) {
  return apply([](T x) { return std::acos(x); }, e);
}
template <typename T> Expr<T> atan(const Expr<T> &e) {
  return apply([](T x) { return std::atan(x); }, e);
}
template <typename T> Expr<T> atan2(const Expr<T> &a, const Expr<T> &b) {
  return apply([](T y, T x) { return std::atan2(y, x); }, a, b);
}
template <typename T> Expr<T> exp(const Expr<T> &e) {
  return apply([](T x) { return std::exp(x); }, e);
}
template <typename T> Expr<T> log(const Expr<T> &e) {
  return apply([](T x) { return std::log(x); }, e);
}
template <typename T> Expr<T> sqrt(const Expr<T> &e) {
  return apply([](T x) { return std::sqrt(x); }, e);
}
template <typename T> Expr<T> abs(const Expr<T> &e) {
  return apply([](T x) { return std::abs(x); }, e);
}
template <typename T> Expr<T> pow(const Expr<T> &base, const Expr<T> &exp) {
  return apply([](T b, T e) { return std::pow(b, e); }, base, exp);
}
template <typename T> Expr<T> min(const Expr<T> &a, const Expr<T> &b) {
  return apply([](T x, T y) { return std::min(x, y); }, a, b);
}
template <typename T> Expr<T> max(const Expr<T> &a, const Expr<T> &b) {
  return apply([](T x, T y) { return std::max(x, y); }, a, b);
}
template <typename T> Expr<T> operator-(const Expr<T> &e) {
  return apply([](T x) { return -x; }, e);
}
} // namespace math
