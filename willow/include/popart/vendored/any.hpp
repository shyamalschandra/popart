#ifndef GUARD_NEURALNET_ANY_HPP
#define GUARD_NEURALNET_ANY_HPP

#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

// Code from:
// https://codereview.stackexchange.com/questions/15269/boostany-replacement-with-stdunique-ptr-support
// licensed under a Creative Commons Attribution-ShareAlike license:
// https://stackoverflow.com/help/licensing
// Licence: https://creativecommons.org/licenses/by-sa/3.0/legalcode
namespace popart {

class any {
public:
  any() noexcept : content(nullptr) {}

  any(any const &other)
      : content(other.content ? other.content->clone() : nullptr) {}

  any(any &&other) noexcept { *this = std::move(other); }

  template <typename ValueType,
            typename = typename std::enable_if<!std::is_same<
                any,
                typename std::decay<ValueType>::type>::value>::type>
  any(ValueType &&value)
      : content(new holder<typename std::remove_reference<ValueType>::type>(
            std::forward<ValueType>(value))) {}

  ~any() { delete content; }

public: // modifiers
  void swap(any &other) noexcept { std::swap(content, other.content); }

  any &operator=(any const &rhs) { return *this = any(rhs); }

  any &operator=(any &&rhs) noexcept {
    content     = rhs.content;
    rhs.content = nullptr;

    return *this;
  }

  template <typename ValueType,
            typename = typename std::enable_if<!std::is_same<
                any,
                typename std::remove_const<typename std::remove_reference<
                    ValueType>::type>::type>::value>::type>
  any &operator=(ValueType &&rhs) {
    return *this = any(std::forward<ValueType>(rhs));
  }

public: // queries
  explicit operator bool() const noexcept { return content; }

  std::type_info const &type() const noexcept {
    return content ? content->type() : typeid(void);
  }

private: // types
  struct placeholder {
    placeholder() = default;

    virtual ~placeholder() noexcept {}

    virtual placeholder *clone() const = 0;

    virtual std::type_info const &type() const = 0;
  };

  template <typename ValueType, typename = void>
  struct holder : public placeholder {
  public: // constructor
    template <class T> holder(T &&value) : held(std::forward<T>(value)) {}

    holder &operator=(holder const &) = delete;

    placeholder *clone() const final { throw std::invalid_argument(""); }

  public: // queries
    std::type_info const &type() const noexcept { return typeid(ValueType); }

  public:
    ValueType held;
  };

  template <typename ValueType>
  struct holder<ValueType,
                typename std::enable_if<
                    std::is_copy_constructible<ValueType>::value>::type>
      : public placeholder {
  public: // constructor
    template <class T> holder(T &&value) : held(std::forward<T>(value)) {}

    placeholder *clone() const final { return new holder<ValueType>(held); }

  public: // queries
    std::type_info const &type() const noexcept { return typeid(ValueType); }

  public:
    ValueType held;
  };

private: // representation
  template <typename ValueType> friend ValueType *any_cast(any *) noexcept;

  template <typename ValueType>
  friend ValueType *unsafe_any_cast(any *) noexcept;

  placeholder *content;
};

template <typename ValueType>
inline ValueType *unsafe_any_cast(any *const operand) noexcept {
  return &static_cast<any::holder<ValueType> *>(operand->content)->held;
}

template <typename ValueType>
inline ValueType const *unsafe_any_cast(any const *const operand) noexcept {
  return unsafe_any_cast<ValueType>(const_cast<any *>(operand));
}

template <typename ValueType>
inline ValueType *any_cast(any *const operand) noexcept {
  return operand && (operand->type() == typeid(ValueType))
             ? &static_cast<any::holder<ValueType> *>(operand->content)->held
             : nullptr;
}

template <typename ValueType>
inline ValueType const *any_cast(any const *const operand) noexcept {
  return any_cast<ValueType>(const_cast<any *>(operand));
}

template <typename ValueType> inline ValueType any_cast(any &operand) {
  typedef typename std::remove_reference<ValueType>::type nonref;

#ifndef NDEBUG
  nonref *const result(any_cast<nonref>(&operand));

  if (!result) {
    throw std::bad_cast();
  }
  // else do nothing

  return *result;
#else
  return *unsafe_any_cast<nonref>(&operand);
#endif // NDEBUG
}

template <typename ValueType> inline ValueType any_cast(any const &operand) {
  typedef typename std::remove_reference<ValueType>::type nonref;

  return any_cast<nonref const &>(const_cast<any &>(operand));
}

} // namespace popart

#endif // GUARD_NEURALNET_ANY_HPP
