module;

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module math:nodes;
import :ast;

export namespace math {

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

enum class IntrinsicOpType {
  Add,
  Sub,
  Mul,
  Div,
  Sin,
  Cos,
  Tan,
  Asin,
  Acos,
  Atan,
  Atan2,
  Exp,
  Log,
  Sqrt,
  Abs,
  Pow,
  Min,
  Max,
  Negate
};

template <typename T> class IntrinsicNode : public ComputationNode<T> {
  IntrinsicOpType op_type;
  std::vector<std::shared_ptr<ComputationNode<T>>> args;

public:
  IntrinsicNode(IntrinsicOpType op,
                std::vector<std::shared_ptr<ComputationNode<T>>> a)
      : op_type(op), args(std::move(a)) {
    for (const auto &arg : args) {
      arg->add_dependent(this);
    }
  }

  ~IntrinsicNode() override {
    for (const auto &arg : args) {
      arg->remove_dependent(this);
    }
  }

  IntrinsicOpType get_op_type() const { return op_type; }
  const std::vector<std::shared_ptr<ComputationNode<T>>> &get_args() const {
    return args;
  }

  T compute() const override {
    if (args.size() == 1) {
      T x = args[0]->evaluate();
      switch (op_type) {
      case IntrinsicOpType::Sin:
        return std::sin(x);
      case IntrinsicOpType::Cos:
        return std::cos(x);
      case IntrinsicOpType::Tan:
        return std::tan(x);
      case IntrinsicOpType::Asin:
        return std::asin(x);
      case IntrinsicOpType::Acos:
        return std::acos(x);
      case IntrinsicOpType::Atan:
        return std::atan(x);
      case IntrinsicOpType::Exp:
        return std::exp(x);
      case IntrinsicOpType::Log:
        return std::log(x);
      case IntrinsicOpType::Sqrt:
        return std::sqrt(x);
      case IntrinsicOpType::Abs:
        return std::abs(x);
      case IntrinsicOpType::Negate:
        return -x;
      default:
        break;
      }
    } else if (args.size() == 2) {
      T x = args[0]->evaluate();
      T y = args[1]->evaluate();
      switch (op_type) {
      case IntrinsicOpType::Add:
        return x + y;
      case IntrinsicOpType::Sub:
        return x - y;
      case IntrinsicOpType::Mul:
        return x * y;
      case IntrinsicOpType::Div:
        return x / y;
      case IntrinsicOpType::Atan2:
        return std::atan2(x, y);
      case IntrinsicOpType::Pow:
        return std::pow(x, y);
      case IntrinsicOpType::Min:
        return std::min(x, y);
      case IntrinsicOpType::Max:
        return std::max(x, y);
      default:
        break;
      }
    }
    return T{}; // Fallback
  }

  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    return {args.begin(), args.end()};
  }

  NodeType get_node_type() const override { return NodeType::IntrinsicOp; }
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
} // namespace math