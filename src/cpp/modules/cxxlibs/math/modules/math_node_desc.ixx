module;

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module math:node_desc;
import :ast;

export namespace math {
enum class ScalarKind { Bool, I32, I64, F32, F64 };

struct StaticDim {
  std::int64_t value;
};

struct DynamicDim {
  std::uint32_t id;
};

struct SymbolicDim {
  std::string name;
};

using Dim = std::variant<StaticDim, DynamicDim, SymbolicDim>;

struct TensorType {
  ScalarKind element;
  std::vector<Dim> shape;
};

using TypeDesc = std::variant<TensorType>;

struct ConstantDesc {

  TypeDesc type;

  // LiteralValue value;
};

struct ParamDesc {

  TypeDesc type;

  // ParamId id;
};

struct VarDesc {

  TypeDesc type;

  // VarId id;
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

struct IntrinsicDesc {

  IntrinsicOpType op;

  TypeDesc result_type;

  std::vector<TypeDesc> operand_types;
};

struct OpaqueFuncDesc {

  TypeDesc result_type;

  std::vector<TypeDesc> operand_types;

  // CallableId callable;

  // LoweringCapability capability;
};

struct UnsupportedDesc {
  NodeType node_type;
  std::string cpp_type_name;
  std::string reason;
};

using NodeDesc = std::variant<ConstantDesc, ParamDesc, VarDesc, IntrinsicDesc,
                              OpaqueFuncDesc, UnsupportedDesc>;
};