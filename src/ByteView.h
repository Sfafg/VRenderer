#pragma once

#include <type_traits>
class ByteView {
  public:
    constexpr ByteView() noexcept : ptr(nullptr), size(0) {}
    constexpr ByteView(const void *ptr, uint size) noexcept : ptr(ptr), size(size) {}

    template <typename T> constexpr ByteView(const T &value) noexcept : ptr(&value), size(sizeof(T)) {
        // static_assert(std::is_trivially_copyable<T>::value, "Data requires trivially copyable types");
    }

    // template <typename T>
    // constexpr ByteView(T &&value) noexcept
    //     requires std::is_rvalue_reference_v<T &&>
    //     : ptr(&value), size(sizeof(T)) {
    //     // static_assert(std::is_trivially_copyable<T>::value, "Data requires trivially copyable types");
    // }

    constexpr const void *Ptr() const noexcept { return ptr; }
    constexpr std::size_t Size() const noexcept { return size; }

  private:
    const void *ptr;
    uint size;
};
