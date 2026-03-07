module;

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module math:ast;

export namespace math {

enum class NodeType { Constant, Param, Var, IntrinsicOp, OpaqueFunc };

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

} // namespace math