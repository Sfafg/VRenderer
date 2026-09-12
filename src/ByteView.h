#pragma once
#include <cstddef>

template <typename T>
concept ContiguousRange = requires(const T &value) {
    value.data();
    value.size();
};

class TableByteView {
  public:
    constexpr TableByteView() noexcept : ptr(nullptr), count(0), size(0) {}

    template <ContiguousRange R>
    constexpr TableByteView(const R &range) noexcept
        : ptr(range.data()), count(static_cast<uint>(range.size())),
          size(sizeof(std::remove_cvref_t<decltype(*range.data())>)) {}

    constexpr const void *Ptr() const noexcept { return ptr; }

    constexpr uint Count() const noexcept { return count; }

    constexpr uint Size() const noexcept { return size; }

    constexpr uint TotalSize() const noexcept { return count * size; }

  private:
    const void *ptr;
    uint count;
    uint size;
};

class ByteView {
  public:
    constexpr ByteView() noexcept : ptr(nullptr), size(0) {}
    constexpr ByteView(const void *ptr, uint size) noexcept : ptr(ptr), size(size) {}
    constexpr ByteView(TableByteView table) noexcept : ptr(table.Ptr()), size(table.TotalSize()) {}

    template <typename T> constexpr ByteView(const T &value) noexcept : ptr(&value), size(sizeof(T)) {}

    constexpr const void *Ptr() const noexcept { return ptr; }
    constexpr std::size_t Size() const noexcept { return size; }

  private:
    const void *ptr;
    uint size;
};
