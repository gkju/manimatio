module;

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module math:exprops;
import :ast;
import :nodes;
import :expression;

export namespace math {

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
  return make_unary_builtin<T>(IntrinsicOpType::Sin, e);
}
template <typename T> Expr<T> cos(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Cos, e);
}
template <typename T> Expr<T> tan(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Tan, e);
}
template <typename T> Expr<T> atan2(const Expr<T> &a, const Expr<T> &b) {
  return make_binary_builtin<T>(IntrinsicOpType::Atan2, a, b);
}

template <typename T> Expr<T> pow(const Expr<T> &a, const Expr<T> &b) {
  return make_binary_builtin<T>(IntrinsicOpType::Pow, a, b);
}

template <typename T> Expr<T> min(const Expr<T> &a, const Expr<T> &b) {
  return make_binary_builtin<T>(IntrinsicOpType::Min, a, b);
}

template <typename T> Expr<T> max(const Expr<T> &a, const Expr<T> &b) {
  return make_binary_builtin<T>(IntrinsicOpType::Max, a, b);
}

template <typename T> Expr<T> operator-(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Negate, e);
}
template <typename T> Expr<T> asin(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Asin, e);
}
template <typename T> Expr<T> acos(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Acos, e);
}
template <typename T> Expr<T> atan(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Atan, e);
}
template <typename T> Expr<T> exp(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Exp, e);
}
template <typename T> Expr<T> log(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Log, e);
}
template <typename T> Expr<T> sqrt(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Sqrt, e);
}
template <typename T> Expr<T> abs(const Expr<T> &e) {
  return make_unary_builtin<T>(IntrinsicOpType::Abs, e);
}

} // namespace math