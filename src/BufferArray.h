#pragma once
#include <VG/VG.h>
#include <memory>
#include <vector>
#include <iostream>

template <typename T>
class BufferArray
{
public:
    BufferArray() : buffer(nullptr), m_size(0), m_begin(nullptr) {}

    BufferArray(int count, vg::Flags<vg::BufferUsage> usage)
        :buffer(new vg::Buffer(count * sizeof(T), usage)), usage(usage), m_size(0)
    {
        this->usage.Set(vg::BufferUsage::TransferDst);
        this->usage.Set(vg::BufferUsage::TransferSrc);
        vg::Allocate(buffer.get(), { vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent });
        m_begin = (T*) buffer.get()->MapMemory();
    }

    void reserve(uint32_t count)
    {
        if (capacity() == count)
            return;

        vg::Buffer* newBuffer = new vg::Buffer(count * sizeof(T), usage);
        vg::Allocate(newBuffer, { vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent });

        uint32_t minSize = newBuffer->GetSize();
        if (buffer->GetSize() < minSize)
            minSize = buffer->GetSize();
        memcpy(newBuffer->MapMemory(), this->data(), minSize);
        buffer.reset(newBuffer);
        m_begin = (T*) buffer.get()->MapMemory();
    }

    void resize(int count)
    {
        if (m_size == count)
            return;

        m_size = count;
        if (capacity() < m_size)
            reserve(m_size);
    }

    void shrink_to_fit()
    {
        reserve(m_size);
    }

    void push_back(const T& element)
    {
        m_size++;
        auto cap = capacity();
        if (cap < m_size)
            reserve((cap + 1) * 1.71f);
        data()[m_size - 1] = element;
    }
    void pop_back()
    {
        m_size--;
    }

    void erase(uint32_t index)
    {
        for (int i = index; i < m_size - 1; i++)
            data()[i] = data()[i + 1];
        pop_back();
    }

    void insert(const T& element, uint32_t index)
    {
        m_size++;
        if (capacity() < m_size)
            reserve((capacity() + 1) * 1.71f);

        for (int i = m_size; i > index; i--)
            data()[i] = data()[i - 1];

        data()[index] = element;
    }
    void insert_range(std::span<const T> elements, uint32_t index)
    {
        m_size += elements.size();
        if (capacity() < m_size)
            reserve((capacity() + elements.size()) * 1.71f);

        for (int i = m_size; i > index; i--)
            data()[i] = data()[i - elements.size()];

        for (int i = index; i < index + elements.size(); i++)
            data()[i] = elements[i - index];
    }

    T& operator [](uint32_t index) { return data()[index]; }
    T& operator [](uint32_t index) const { return data()[index]; }

    uint32_t capacity() const { if (!buffer) return 0; return buffer.get()->GetSize() / sizeof(T); }
    uint32_t size() const { return m_size; }
    uint32_t byte_size() const { return m_size * sizeof(T); }
    T* begin() { return m_begin; }
    T* begin() const { return m_begin; }
    T* end() { return m_begin + m_size; }
    T* end() const { return m_begin + m_size; }
    T* data() { return m_begin; }
    T* data() const { return m_begin; }

    operator vg::Buffer& () const { if (deviceBuffer)return *deviceBuffer; return *buffer; }
    operator vg::BufferHandle() const { if (deviceBuffer)return *deviceBuffer; return *buffer; }

private:
    std::unique_ptr<vg::Buffer> deviceBuffer;
    std::unique_ptr<vg::Buffer> buffer;
    uint32_t m_size;
    T* m_begin;
    vg::Flags<vg::BufferUsage> usage;
};