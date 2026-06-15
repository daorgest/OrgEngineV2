#pragma once
#include <cassert>

template <typename T>
struct Span;

using vecSizeType = size_t;

template <typename T>
struct Vector
{
protected:
    T* m_data = nullptr;
    vecSizeType m_size = 0;
    vecSizeType m_capacity = 0;


    static T* allocate(const vecSizeType capacity)
    {
        return static_cast<T*>(::operator new[](capacity * sizeof(T)));
    }

    static void deallocate(T* data)
    {
        ::operator delete[](data);
    }

public:

    Vector() = default;

    explicit Vector(vecSizeType count)
    {
        reserve(count);
        for (vecSizeType i = 0; i < count; ++i)
        {
            new(m_data + i) T();
        }
        m_size = count;
    }

    explicit Vector(Span<const T> items)
    {
        vecSizeType items_size = static_cast<vecSizeType>(items.size());
        reserve(items_size);
        for (vecSizeType i = 0; i < items_size; ++i)
        {
            new(m_data + i) T(items[i]);
        }
        m_size = items_size;
    }

    Vector(std::initializer_list<T> list)
    {
        vecSizeType list_size = static_cast<vecSizeType>(list.size());
        reserve(list_size);
        vecSizeType i = 0;
        for (const T& item : list)
        {
            new(m_data + i) T(item);
            i++;
        }
        m_size = list_size;
    }

    Vector(Vector&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity)
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    ~Vector()
    {
        clear();
        if (m_data) deallocate(m_data);
    }

    Vector& operator=(Vector&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            if (m_data) deallocate(m_data);

            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    Vector(const Vector& other)
    {
        assign(other);
    }

    Vector& operator=(const Vector& other)
    {
        if (this != &other)
        {
            assign(other);
        }
        return *this;
    }

    [[nodiscard]] constexpr operator Span<T>() noexcept
    {
        return Span<T>(m_data, m_size);
    }

    [[nodiscard]] constexpr operator Span<const T>() const noexcept
    {
        return Span<const T>(m_data, m_size);
    }

    // Accessors
    [[nodiscard]] T* data() { return m_data; }
    [[nodiscard]] const T* data() const { return m_data; }

    [[nodiscard]] T* begin() { return m_data; }
    [[nodiscard]] const T* begin() const { return m_data; }

    [[nodiscard]] T* end() { return m_data + m_size; }
    [[nodiscard]] const T* end() const { return m_data + m_size; }

    [[nodiscard]] T& front()
    {
        assert(m_size > 0);
        return m_data[0];
    }

    [[nodiscard]] const T& front() const
    {
        assert(m_size > 0);
        return m_data[0];
    }

    [[nodiscard]] T& back()
    {
        assert(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] const T& back() const
    {
        assert(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] T& at(vecSizeType i) { return m_data[i]; }
    [[nodiscard]] const T& at(vecSizeType i) const { return m_data[i]; }

    [[nodiscard]] vecSizeType size() const { return m_size; }
    [[nodiscard]] vecSizeType size_bytes() const { return (sizeof(T) * m_size); }
    [[nodiscard]] vecSizeType capacity() const { return m_capacity; }
    [[nodiscard]] bool empty() const { return m_size == 0; }

    // Subscript operators
    T& operator[](vecSizeType idx)
    {
        assert(idx < m_size && "Vector::operator[] out-of-bounds");
        return m_data[idx];
    }

    const T& operator[](vecSizeType idx) const
    {
        assert(idx < m_size && "Vector::operator[] out-of-bounds");
        return m_data[idx];
    }

    void reserve(vecSizeType newCapacity)
    {
        if (newCapacity <= m_capacity) return;

        T* newData = allocate(newCapacity);

        if (m_data)
        {
            for (vecSizeType i = 0; i < m_size; ++i)
            {
                new(newData + i) T(std::move(m_data[i]));
                m_data[i].~T();
            }
            deallocate(m_data);
        }

        m_data = newData;
        m_capacity = newCapacity;
    }

    void resize(vecSizeType newSize)
    {
        if (newSize > m_capacity) reserve(newSize);

        if (newSize > m_size)
        {
            for (; m_size < newSize; ++m_size)
            {
                new(m_data + m_size) T();
            }
        }
        else if (newSize < m_size)
        {
            for (; m_size > newSize; --m_size)
            {
                m_data[m_size - 1].~T();
            }
        }
    }

    void resize_uninitialized(vecSizeType new_size)
    {
        if (new_size > m_capacity) reserve(new_size);
        m_size = new_size;
    }


    // --- Modifiers ---
    void assign(vecSizeType count, const T& value)
    {
        clear();
        reserve(count);
        for (vecSizeType i = 0; i < count; ++i)
        {
            new(m_data + i) T(value);
        }
        m_size = count;
    }

    void assign(Span<const T> items)
    {
        clear();
        vecSizeType items_size = static_cast<vecSizeType>(items.size());
        reserve(items_size);
        for (vecSizeType i = 0; i < items_size; ++i)
        {
            new(m_data + i) T(items[i]);
        }
        m_size = items_size;
    }

    void assign(std::initializer_list<T> list)
    {
        clear();
        vecSizeType list_size = static_cast<vecSizeType>(list.size());
        reserve(list_size);
        vecSizeType i = 0;
        for (const T& item : list)
        {
            new(m_data + i) T(item);
            i++;
        }
        m_size = list_size;
    }

    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        if (m_size == m_capacity)
            reserve(m_capacity == 0 ? 8 : m_capacity * 2);

        new(m_data + m_size++) T(std::forward<Args>(args)...);
    }

    void push_back(const T& object)
    {
        emplace_back(object);
    }

    void push_back(T&& object)
    {
        emplace_back(std::move(object));
    }

    void push_span(Span<const T> items)
    {
        vecSizeType items_size = static_cast<vecSizeType>(items.size());
        if (m_size + items_size > m_capacity)
        {
            reserve(m_size + items_size);
        }

        for (vecSizeType i = 0; i < items_size; ++i)
        {
            new(m_data + m_size + i) T(items[i]);
        }
        m_size += items_size;
    }

    void pop_back()
    {
        // assert(m_size > 0);
        m_data[m_size - 1].~T();
        m_size--;
    }

    void insert(vecSizeType index, const T& value)
    {
        // assert(index <= m_size);
        if (index == m_size)
        {
            push_back(value);
            return;
        }

        if (m_size >= m_capacity) reserve(m_capacity == 0 ? 8 : m_capacity * 2);

        new(m_data + m_size) T(std::move(m_data[m_size - 1]));

        for (vecSizeType i = m_size - 1; i > index; --i)
        {
            m_data[i] = std::move(m_data[i - 1]);
        }

        m_data[index] = value;
        m_size++;
    }

    void erase(vecSizeType index)
    {
        // assert(index < m_size);

        for (vecSizeType i = index; i < m_size - 1; ++i)
        {
            m_data[i] = std::move(m_data[i + 1]);
        }

        m_data[m_size - 1].~T();
        m_size--;
    }

    void erase(T* first, T* last)
    {
        if (first >= last) return;

        const vecSizeType startIdx = static_cast<vecSizeType>(first - m_data);
        const vecSizeType count = static_cast<vecSizeType>(last - first);

        // Shift valid elements down to overwrite the erased chunk
        for (vecSizeType i = startIdx; i < m_size - count; ++i)
        {
            m_data[i] = std::move(m_data[i + count]);
        }

        // Destruct the old tail
        for (vecSizeType i = m_size - count; i < m_size; ++i)
        {
            m_data[i].~T();
        }

        m_size -= count;
    }

    void erase_unordered(vecSizeType index)
    {
        // assert(index < m_size);

        if (index != m_size - 1)
        {
            // Move the very last element into the slot we want to erase
            m_data[index] = std::move(m_data[m_size - 1]);
        }

        // Destroy the old tail
        m_data[m_size - 1].~T();
        m_size--;
    }

    void clear()
    {
        for (vecSizeType i = 0; i < m_size; ++i)
        {
            m_data[i].~T();
        }
        m_size = 0;
    }


    // --- Queries & Searching ---
    bool contains(const T& object) const
    {
        for (vecSizeType i = 0; i < m_size; i++)
        {
            if (m_data[i] == object)
            {
                return true;
            }
        }
        return false;
    }


    // --- Operators & Friends ---
    bool operator==(const Vector& other) const
    {
        if (m_size != other.m_size)
            return false;

        for (vecSizeType i = 0; i < m_size; i++)
        {
            if (!(m_data[i] == other.m_data[i]))
                return false;
        }
        return true;
    }

    bool operator!=(const Vector& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Vector& other) const
    {
        return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
    }

    bool operator>(const Vector& other) const
    {
        return other < *this;
    }

    bool operator<=(const Vector& other) const
    {
        return !(*this > other);
    }

    bool operator>=(const Vector& other) const
    {
        return !(*this < other);
    }

    friend void swap(Vector& first, Vector& second) noexcept
    {
        vecSizeType tmp_size = first.m_size;
        first.m_size = second.m_size;
        second.m_size = tmp_size;

        vecSizeType tmp_cap = first.m_capacity;
        first.m_capacity = second.m_capacity;
        second.m_capacity = tmp_cap;

        T* tmp_data = first.m_data;
        first.m_data = second.m_data;
        second.m_data = tmp_data;
    }
};
