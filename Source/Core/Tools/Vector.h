#pragma once
#include <cassert>
#include <initializer_list>
#include <utility>

#ifdef USE_SMALL_VECTOR
    using vecSizeType = u32;
#else
    using vecSizeType = size_t;
#endif

template <typename T>
class Vector
{
protected:
    vecSizeType m_size = 0;
    vecSizeType m_capacity = 0;
    T* m_data = nullptr;

    static T* allocate(const vecSizeType capacity)
    {
        return static_cast<T*>(operator new[](capacity * sizeof(T)));
    }

    static void deallocate(T* data)
    {
        operator delete[](data);
    }

public:
    // Constructors
    Vector()
    {
        m_capacity = 10;
        m_data = allocate(m_capacity);
    }

    explicit Vector(const vecSizeType n, const T& value = T())
    {
        m_size = n;
        m_capacity = n;
        m_data = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; i++)
        {
            new(m_data + i) T(value);
        }
    }

    Vector(std::initializer_list<T> list)
    {
        m_size = static_cast<vecSizeType>(list.size());
        m_capacity = m_size;
        m_data = allocate(m_capacity);

        vecSizeType i = 0;
        for (const T& item : list)
        {
            new(m_data + i++) T(item);
        }
    }

    Vector(const T* first, const T* last)
    {
        m_size = static_cast<vecSizeType>(last - first);
        m_capacity = m_size;
        m_data = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; ++i)
        {
            new(m_data + i) T(first[i]);
        }
    }

    // Reserve-only constructor (uninitialized)
    explicit Vector(vecSizeType capacity, nullptr_t)
    {
        m_size = 0;
        m_capacity = capacity;
        m_data = allocate(m_capacity);
    }

    Vector(const T* src, vecSizeType n)
    {
        m_size = n;
        m_capacity = n;
        m_data = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; ++i)
        {
            new(m_data + i) T(src[i]);
        }
    }

    // Copy constructor
    Vector(const Vector& other)
    {
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_data = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; i++)
        {
            new(m_data + i) T(other.m_data[i]);
        }
    }

    // Move constructor
    Vector(Vector&& other) noexcept
        : m_size(other.m_size), m_capacity(other.m_capacity), m_data(other.m_data)
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }


    // Copy assignment
    Vector& operator=(const Vector& other)
    {
        if (this != &other)
        {
            clear();
            deallocate(m_data);

            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_data = allocate(m_capacity);
            for (vecSizeType i = 0; i < m_size; i++)
            {
                new(m_data + i) T(other.m_data[i]);
            }
        }
        return *this;
    }

    // Move assignment
    Vector& operator=(Vector&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            deallocate(m_data);
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    ~Vector()
    {
        clear();
        deallocate(m_data);
    }

    // Accessors
    T* data() { return m_data; }
    const T* data() const { return m_data; }

    T* begin() { return m_data; }
    const T* begin() const { return m_data; }

    T* end() { return m_data + m_size; }
    const T* end() const { return m_data + m_size; }

    T& front() { assert(m_size > 0); return m_data[0]; }
    const T& front() const { assert(m_size > 0); return m_data[0]; }

    T& back() { assert(m_size > 0); return m_data[m_size - 1]; }
    const T& back() const { assert(m_size > 0); return m_data[m_size - 1]; }

    T& at(vecSizeType i) { return m_data[i]; }
    const T& at(vecSizeType i) const { return m_data[i]; }

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
        for (vecSizeType i = 0; i < m_size; i++)
        {
            new(newData + i) T(std::move(m_data[i]));
            m_data[i].~T();
        }

        deallocate(m_data);
        m_data     = newData;
        m_capacity = newCapacity;
    }

    void resize(vecSizeType newSize)
    {
        if (newSize > m_capacity)
            reserve(newSize);

        // Construct new elements (default-construct)
        for (vecSizeType i = m_size; i < newSize; i++)
        {
            new(m_data + i) T();
        }

        // Destroy excess elements if shrinking
        for (vecSizeType i = newSize; i < m_size; i++)
        {
            m_data[i].~T();
        }

        m_size = newSize;
    }


    // Modifiers
    void push_back(const T& object)
    {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (m_data + m_size++) T(object);
    }

    void push_back(T&& object)
    {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (m_data + m_size++) T(std::move(object));
    }

    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        if (m_size == m_capacity)
        {
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        }
        new(m_data + m_size) T(std::forward<Args>(args)...);
        m_size++;
    }

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

    void pop_back()
    {
        if (m_size > 0)
        {
            m_size--;
            m_data[m_size].~T();
        }
    }

    void erase(const vecSizeType position)
    {
        if (position >= m_size)
            return;
        m_data[position].~T();
        for (vecSizeType i = position; i < m_size - 1; i++)
        {
            new(m_data + i) T(std::move(m_data[i + 1]));
            m_data[i + 1].~T();
        }
        m_size--;
    }

    T* erase(T* pos)
    {
        if (pos < m_data || pos >= m_data + m_size)
            return end();

        const auto position = static_cast<vecSizeType>(pos - m_data);
        erase(position);
        return m_data + position;
    }

    void insert(vecSizeType idx, const T& object)
    {
        if (idx > m_size)
            return;

        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);

        for (vecSizeType i = m_size; i > idx; i--)
        {
            new (m_data + i) T(std::move(m_data[i - 1]));
            m_data[i - 1].~T();
        }

        new (m_data + idx) T(object);
        m_size++;
    }

    void insert(vecSizeType idx, const T* src, vecSizeType count)
    {
        if (idx > m_size || count == 0)
            return;

        // Grow if needed
        if (m_size + count > m_capacity)
            reserve((m_capacity == 0) ? count : std::max(m_capacity * 2, m_size + count));

        // Move existing elements upward
        for (vecSizeType i = m_size; i > idx; --i)
        {
            new (m_data + i + count - 1) T(std::move(m_data[i - 1]));
            m_data[i - 1].~T();
        }

        // Copy new elements into place
        for (vecSizeType i = 0; i < count; ++i)
            new (m_data + idx + i) T(src[i]);

        m_size += count;
    }


    template<typename It>
    void assign(It first, It last)
    {
        clear();
        while (first != last)
        {
            push_back(*first);
            ++first;
        }
    }

    void clear()
    {
        for (vecSizeType i = 0; i < m_size; ++i)
        {
            m_data[i].~T();
        }
        m_size = 0;
    }
};