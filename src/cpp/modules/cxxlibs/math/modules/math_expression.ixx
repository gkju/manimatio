module;

#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module math:expression;

export namespace math {

enum class NodeType { Value, Param, BinOp, BuiltinOp, OpaqueFunc };

class GraphNode {
public:
  virtual ~GraphNode() = default;
  virtual std::vector<std::shared_ptr<const GraphNode>>
  get_children() const = 0;
  virtual NodeType get_node_type() const = 0;
};

template <typename T> class ComputationNode : public GraphNode {
public:
  virtual T evaluate() const = 0;
};

template <typename T> class ValueNode : public ComputationNode<T> {
  T value;

public:
  ValueNode(T val) : value(val) {}
  T evaluate() const override { return value; }
  void set(T val) { value = val; }
  T get() const { return value; }

  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    return {};
  }
  NodeType get_node_type() const override { return NodeType::Value; }
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

  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    return {left, right};
  }
  NodeType get_node_type() const override { return NodeType::BinOp; }
};

template <typename R, typename F, typename... Args>
class FuncNode : public ComputationNode<R> {
  F func;
  std::tuple<std::shared_ptr<ComputationNode<Args>>...> args;

public:
  FuncNode(F f, std::tuple<std::shared_ptr<ComputationNode<Args>>...> a)
      : func(std::move(f)), args(std::move(a)) {}

  R evaluate() const override {
    return std::apply([&](const auto &...a) { return func(a->evaluate()...); },
                      args);
  }

  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    std::vector<std::shared_ptr<const GraphNode>> children;
    std::apply([&](const auto &...a) { (children.push_back(a), ...); }, args);
    return children;
  }

  NodeType get_node_type() const override { return NodeType::OpaqueFunc; }
};

template <typename T> class Expr {
  std::shared_ptr<ComputationNode<T>> node;

  static Expr binop(typename BinOpNode<T>::Op op, const Expr &l,
                    const Expr &r) {
    return Expr(std::make_shared<BinOpNode<T>>(op, l.node, r.node));
  }

public:
  using value_type = T;

  Expr(std::shared_ptr<ComputationNode<T>> n) : node(std::move(n)) {}
  Expr(T value) : node(std::make_shared<ValueNode<T>>(value)) {}
  T evaluate() const { return node->evaluate(); }

  std::shared_ptr<ComputationNode<T>> get_node() const { return node; }

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

template <typename T>
concept IsExpr = requires(T t) {
  typename std::decay_t<T>::value_type;
  t.get_node();
};

template <typename T> struct NodeTraits {
  using value_type = std::decay_t<T>;
  static auto get(const T &v) { return Expr<value_type>(v).get_node(); }
};

template <IsExpr T> struct NodeTraits<T> {
  using value_type = typename std::decay_t<T>::value_type;
  static auto get(const T &e) { return e.get_node(); }
};

template <typename F, typename... Args> auto apply(F &&f, const Args &...args) {
  using R = std::invoke_result_t<F, typename NodeTraits<Args>::value_type...>;

  auto child_tuple = std::make_tuple(NodeTraits<Args>::get(args)...);

  auto node = std::make_shared<
      FuncNode<R, std::decay_t<F>, typename NodeTraits<Args>::value_type...>>(
      std::forward<F>(f), std::move(child_tuple));

  return Expr<R>(node);
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