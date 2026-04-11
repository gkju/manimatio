module;

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module smodel:obj;

export namespace smodel {

template <typename T> class Obj {
private:
  std::shared_ptr<T> ptr;

public:
  Obj()
    requires std::default_initializable<T>
      : ptr(std::make_shared<T>()) {}
  template <typename... Args>
    requires std::constructible_from<T, Args...>
  Obj(Args &&...args) : ptr(std::make_shared<T>(std::forward<Args>(args)...)) {}

  Obj(const Obj &other) = default;
  Obj(Obj &&other) noexcept = default;
  Obj &operator=(const Obj &other) = default;
  Obj &operator=(Obj &&other) noexcept = default;

  T *operator->() { return ptr.get(); }
  T &operator*() { return *ptr; }

  std::shared_ptr<T> get_shared() const { return ptr; }
};
}; // namespace smodel