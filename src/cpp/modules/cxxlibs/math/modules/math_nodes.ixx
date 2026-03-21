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

template <typename R, typename... Args>
class IntrinsicNode : public ComputationNode<R> {
  IntrinsicOpType op_type;
  std::tuple<std::shared_ptr<ComputationNode<Args>>...> args;

public:
  IntrinsicNode(IntrinsicOpType op, std::shared_ptr<ComputationNode<Args>>... a)
      : op_type(op), args(std::move(a)...) {
    std::apply([this](auto &...child) { (child->add_dependent(this), ...); },
               args);
  }

  ~IntrinsicNode() override {
    std::apply([this](auto &...child) { (child->remove_dependent(this), ...); },
               args);
  }

  IntrinsicOpType get_op_type() const { return op_type; }

  R compute() const override {
    using namespace std; // Resolve ops to std by default
    if constexpr (sizeof...(Args) == 1) {

      auto x = std::get<0>(args)->evaluate();
      switch (op_type) {
      case IntrinsicOpType::Sin:
        return sin(x);
      case IntrinsicOpType::Cos:
        return cos(x);
      case IntrinsicOpType::Tan:
        return tan(x);
      case IntrinsicOpType::Asin:
        return asin(x);
      case IntrinsicOpType::Acos:
        return acos(x);
      case IntrinsicOpType::Atan:
        return atan(x);
      case IntrinsicOpType::Exp:
        return exp(x);
      case IntrinsicOpType::Log:
        return log(x);
      case IntrinsicOpType::Sqrt:
        return sqrt(x);
      case IntrinsicOpType::Abs:
        return abs(x);
      case IntrinsicOpType::Negate:
        return -x;
      default:
        break;
      }
    } else if constexpr (sizeof...(Args) == 2) {
      auto x = std::get<0>(args)->evaluate();
      auto y = std::get<1>(args)->evaluate();
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
        return atan2(x, y);
      case IntrinsicOpType::Pow:
        return pow(x, y);
      case IntrinsicOpType::Min:
        return min(x, y);
      case IntrinsicOpType::Max:
        return max(x, y);
      default:
        break;
      }
    }
    return R{}; // Fallback
  }

  std::vector<std::shared_ptr<const GraphNode>> get_children() const override {
    std::vector<std::shared_ptr<const GraphNode>> children;
    std::apply([&](const auto &...a) { (children.push_back(a), ...); }, args);
    return children;
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