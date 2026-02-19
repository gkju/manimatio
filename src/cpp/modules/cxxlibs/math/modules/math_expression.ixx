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

export namespace math {

enum class NodeType { Constant, Param, Var, BinOp, BuiltinOp, OpaqueFunc };

class GraphNode {
protected:
  mutable bool is_dirty = true;
  mutable std::vector<const GraphNode *> dependents;

public:
  virtual ~GraphNode() = default;
  virtual std::vector<std::shared_ptr<const GraphNode>>
  get_children() const = 0;
  virtual NodeType get_node_type() const = 0;

  void add_dependent(const GraphNode *dep) const { dependents.push_back(dep); }

  void remove_dependent(const GraphNode *dep) const {
    std::erase(dependents, dep);
  }

  void mark_dirty() const {
    if (is_dirty)
      return;

    is_dirty = true;
    for (const auto *parent : dependents) {
      parent->mark_dirty();
    }
  }
};

template <typename T> class ComputationNode : public GraphNode {
protected:
  mutable T cached_value{};

  virtual T compute() const = 0;

public:
  T evaluate() const {
    if (this->is_dirty) {
      cached_value = compute();
      this->is_dirty = false;
    }
    return cached_value;
  }
};

template <typename T> class ConstantNode : public ComputationNode<T> {
  const T value;

public:
  explicit ConstantNode(T val) : value(val) {}
  T compute() const override { return value; }
  NodeType get_node_type() const override { return NodeType::Constant; }
  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    return {};
  }
};

template <typename T> class ValueNode : public ComputationNode<T> {
  T value;

public:
  ValueNode(T val) : value(val) {}
  T compute() const override { return value; }
  void set(T val) {
    value = val;
    this->cached_value = val;
    this->mark_dirty();
    this->is_dirty = false;
  }
  T get() const { return value; }

  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    return {};
  }
};

template <typename T> class ParamNode : public ValueNode<T> {
public:
  explicit ParamNode(T value) : ValueNode<T>(value) {}
  NodeType get_node_type() const override { return NodeType::Param; }
};

template <typename T> class VarNode : public ValueNode<T> {
public:
  explicit VarNode(T val) : ValueNode<T>(val) {}

  NodeType get_node_type() const override { return NodeType::Var; }
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
      : op(op), left(std::move(l)), right(std::move(r)) {
    left->add_dependent(this);
    right->add_dependent(this);
  }

  ~BinOpNode() override {
    left->remove_dependent(this);
    right->remove_dependent(this);
  }

  Op get_op() const { return op; }
  const std::shared_ptr<ComputationNode<T>> &lhs() const { return left; }
  const std::shared_ptr<ComputationNode<T>> &rhs() const { return right; }

  T compute() const override {
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
      : func(std::move(f)), args(std::move(a)) {
    std::apply(
        [this](const auto &...child) { (child->add_dependent(this), ...); },
        args);
  }

  ~FuncNode() override {
    std::apply(
        [this](const auto &...child) { (child->remove_dependent(this), ...); },
        args);
  }

  R compute() const override {
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
protected:
  std::shared_ptr<ComputationNode<T>> node;

  static Expr binop(typename BinOpNode<T>::Op op, const Expr &l,
                    const Expr &r) {
    return Expr(std::make_shared<BinOpNode<T>>(op, l.node, r.node));
  }

public:
  using value_type = T;

  Expr(std::shared_ptr<ComputationNode<T>> n) : node(std::move(n)) {}
  Expr(T value) : node(std::make_shared<ConstantNode<T>>(value)) {}
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

template <typename T>
concept IsExpr = requires(const std::remove_cvref_t<T> &t) {
  typename std::remove_cvref_t<T>::value_type;
  { t.get_node() } -> std::convertible_to<std::shared_ptr<GraphNode>>;
};

template <typename T> struct extract_value_type {
  using type = std::remove_cvref_t<T>;
};

template <IsExpr T> struct extract_value_type<T> {
  using type = typename std::remove_cvref_t<T>::value_type;
};

template <typename T> using expr_value_t = typename extract_value_type<T>::type;

template <typename T> auto get_node_from(const T &v) {
  if constexpr (IsExpr<T>) {
    return v.get_node();
  } else {
    return Expr<expr_value_t<T>>(v).get_node();
  }
}

template <typename F, typename... Args> auto apply(F &&f, const Args &...args) {
  using R = std::invoke_result_t<F, expr_value_t<Args>...>;
  auto child_tuple = std::make_tuple(get_node_from(args)...);

  auto node =
      std::make_shared<FuncNode<R, std::decay_t<F>, expr_value_t<Args>...>>(
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