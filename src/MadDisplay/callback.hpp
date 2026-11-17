#pragma once
#include <new>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

namespace mad { namespace detail {
// AVR cores do not provide the usual <type_traits>/<functional> headers.
template<class T> struct RemoveReference { using Type = T; };
template<class T> struct RemoveReference<T&> { using Type = T; };
template<class T> struct RemoveReference<T&&> { using Type = T; };
template<class T> struct RemoveConst { using Type = T; };
template<class T> struct RemoveConst<const T> { using Type = T; };
template<class T> struct Decay : RemoveConst<T> {};
template<class Result,class... Args> struct Decay<Result(Args...)> { using Type = Result (*)(Args...); };
template<class T> using Stored = typename Decay<typename RemoveReference<T>::Type>::Type;
template<class T> struct IsLvalue { static constexpr bool value = false; };
template<class T> struct IsLvalue<T&> { static constexpr bool value = true; };
template<class A,class B> struct IsSame { static constexpr bool value = false; };
template<class T> struct IsSame<T,T> { static constexpr bool value = true; };
template<bool, class T = void> struct EnableIf {};
template<class T> struct EnableIf<true,T> { using Type = T; };
template<class T> T&& forward(typename RemoveReference<T>::Type& value) { return static_cast<T&&>(value); }
template<class T> T&& forward(typename RemoveReference<T>::Type&& value) { return static_cast<T&&>(value); }

template<class T> struct IsInteger { static constexpr bool value = false; };
#define MAD_DETAIL_INTEGER(Type) template<> struct IsInteger<Type> { static constexpr bool value = true; };
MAD_DETAIL_INTEGER(bool)
MAD_DETAIL_INTEGER(char)
MAD_DETAIL_INTEGER(signed char)
MAD_DETAIL_INTEGER(unsigned char)
MAD_DETAIL_INTEGER(short)
MAD_DETAIL_INTEGER(unsigned short)
MAD_DETAIL_INTEGER(int)
MAD_DETAIL_INTEGER(unsigned int)
MAD_DETAIL_INTEGER(long)
MAD_DETAIL_INTEGER(unsigned long)
MAD_DETAIL_INTEGER(long long)
MAD_DETAIL_INTEGER(unsigned long long)
#undef MAD_DETAIL_INTEGER

struct SharedStorage {
  size_t references = 1;
  virtual ~SharedStorage() = default;
};
template<class T> struct SharedValue final : SharedStorage {
  template<class U> explicit SharedValue(U&& source) : value(forward<U>(source)) {}
  T value;
};

// Only owned temporaries/captures allocate. Copies retain the same storage.
class SharedOwner {
 public:
  SharedOwner() = default;
  SharedOwner(const SharedOwner& other) : storage_(other.storage_) { if (storage_) ++storage_->references; }
  SharedOwner& operator=(const SharedOwner& other) {
    if (this != &other) { if (other.storage_) ++other.storage_->references; reset(); storage_ = other.storage_; }
    return *this;
  }
  ~SharedOwner() { reset(); }
  void reset(SharedStorage* storage = nullptr) {
    if (storage_ && !--storage_->references) delete storage_;
    storage_ = storage;
  }
 private:
  SharedStorage* storage_ = nullptr;
};

class UseCallback {
 public:
  using Handler = void (*)(void*);
  void set(Handler handler,void* context) { owner_.reset(); handler_ = handler; context_ = context; }
  template<class F> auto set(F&& function) -> decltype(function(),bool()) {
    using Value = SharedValue<Stored<F>>;
    Value* storage = new (std::nothrow) Value(forward<F>(function));
    if (!storage) return false;
    owner_.reset(storage); context_ = &storage->value;
    handler_ = [](void* context) { (*static_cast<Stored<F>*>(context))(); };
    return true;
  }
  void operator()() const {
    const SharedOwner keepAlive = owner_;
    if (handler_) handler_(context_);
  }
 private:
  Handler handler_ = nullptr;
  void* context_ = nullptr;
  SharedOwner owner_;
};

class TextValue {
 public:
  TextValue() = default;
  explicit TextValue(const char* value) { assign(value); }
  TextValue(const TextValue& other) { assign(other.c_str()); }
  TextValue(TextValue&& other) noexcept : data_(other.data_),capacity_(other.capacity_),valid_(other.valid_) {
    other.data_ = nullptr; other.capacity_ = 0; other.valid_ = true;
  }
  TextValue& operator=(TextValue&& other) noexcept {
    if (this != &other) {
      free(data_); data_ = other.data_; capacity_ = other.capacity_; valid_ = other.valid_;
      other.data_ = nullptr; other.capacity_ = 0; other.valid_ = true;
    }
    return *this;
  }
  TextValue& operator=(const TextValue& other) { if (this != &other) assign(other.c_str()); return *this; }
  ~TextValue() { free(data_); }
  const char* c_str() const { return data_ ? data_ : ""; }
  bool valid() const { return valid_; }
  bool assign(const char* value) {
    if (!value) value = "";
    const size_t length = strlen(value);
    if (!length && !data_) { valid_ = true; return true; }
    if (length+1 > capacity_) {
      char* next = static_cast<char*>(realloc(data_,length+1));
      if (!next) { valid_ = false; return false; }
      data_ = next; capacity_ = length+1;
    }
    memmove(data_,value,length+1); valid_ = true; return true;
  }
  bool number(uint64_t value,bool negative = false) {
    char buffer[22]; char* end = buffer+sizeof(buffer); *--end = '\0';
    do { *--end = char('0'+value%10); value /= 10; } while (value);
    if (negative) *--end = '-';
    return assign(end);
  }
  bool decimal(double value,uint8_t decimals) {
    if (decimals > 6) decimals = 6;
    char buffer[48];
#if defined(__AVR__)
    if (value > 1e15 || value < -1e15) dtostre(value,buffer,decimals,0);
    else dtostrf(value,0,decimals,buffer);
#else
    if (value > 1e15 || value < -1e15) snprintf(buffer,sizeof(buffer),"%.*g",int(decimals+1),value);
    else snprintf(buffer,sizeof(buffer),"%.*f",int(decimals),value);
#endif
    return assign(buffer);
  }
  bool signedNumber(int64_t value) {
    return number(value < 0 ? uint64_t(-(value+1))+1 : uint64_t(value),value < 0);
  }
 private:
  char* data_ = nullptr;
  size_t capacity_ = 0;
  bool valid_ = true;
};
}} // namespace mad::detail

#define MAD_DETAIL_CHAIN_METHOD(Type, Base, Name, Argument) \
  Type& Name(Argument value) & { Base::Name(value); return *this; } \
  Type&& Name(Argument value) && { Base::Name(value); return static_cast<Type&&>(*this); }
#define MAD_DETAIL_COMPONENT_CHAIN(Type) \
  MAD_DETAIL_CHAIN_METHOD(Type, Component, setWidth, Length) \
  MAD_DETAIL_CHAIN_METHOD(Type, Component, setHeight, Length) \
  MAD_DETAIL_CHAIN_METHOD(Type, Component, setSize, SizeSpec) \
  MAD_DETAIL_CHAIN_METHOD(Type, Component, setFrame, Frame) \
  MAD_DETAIL_CHAIN_METHOD(Type, Component, setVisible, bool) \
  MAD_DETAIL_CHAIN_METHOD(Type, Component, setEnabled, bool)
#define MAD_DETAIL_USE_CALLBACK(Type) \
  Type& onUse(UseHandler handler,void* context = nullptr) & { callback_.set(handler,context); return *this; } \
  Type&& onUse(UseHandler handler,void* context = nullptr) && { callback_.set(handler,context); return static_cast<Type&&>(*this); } \
  template<class F> auto onUse(F&& function) & -> decltype(function(),*this) { \
    buildOK_ = callback_.set(detail::forward<F>(function)) && buildOK_; return *this; } \
  template<class F> auto onUse(F&& function) && -> decltype(function(),static_cast<Type&&>(*this)) { \
    buildOK_ = callback_.set(detail::forward<F>(function)) && buildOK_; return static_cast<Type&&>(*this); }

#define MAD_DETAIL_CLICK_ALIAS(Type) \
  template<class... Args> Type& onClick(Args&&... args) & { return onUse(detail::forward<Args>(args)...); } \
  template<class... Args> Type&& onClick(Args&&... args) && { return static_cast<Type&&>(*this).onUse(detail::forward<Args>(args)...); }
