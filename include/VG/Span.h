#pragma once
#include <span>

template <typename T> class Span : public std::span<T> {
  public:
    using std::span<T>::span;

    Span(std::initializer_list<T> list)
        requires std::is_const_v<T>
        : std::span<T>(list.begin(), list.size()) {}

    Span(T &element) : std::span<T>(&element, 1) {}

    Span(T &&element)
        requires std::is_const_v<T>
        : std::span<T>(&element, 1) {}
};

#include <vector>
#include <iostream>
template <typename T> struct Vector : public std::vector<T> {
    template <typename... Args>
    Vector(T &&arg, Args &&...args)
        requires(std::same_as<T, Args> && ...)
    {
        this->reserve(sizeof...(Args) + 1);
        this->emplace_back(std::move(arg));
        (this->emplace_back(std::move(args)), ...);
    }
};
