#pragma once
#include <cassert>
#include <initializer_list>
#include <utility>


#ifndef _DEBUG
    #include <vector>
    template <typename T>
    using Vector = std::vector<T>;
#else
#ifdef USE_SMALL_VECTOR
    using vecSizeType = u32;  // Use u32 for smaller vectors
#else
    using vecSizeType = size_t; // Default to size_t for larger vectors
#endif

/**
 * @brief A simple dynamic array, similar to vector.
 *
 */
template <typename T>
class Vector
{
protected:
    vecSizeType m_size = 0;       ///< Current number of elements.
    vecSizeType m_capacity = 0;   ///< Allocated capacity.
    T*          m_data = nullptr; ///< Pointer to the allocated storage.

    /**
     * @brief Allocates raw memory for the given capacity.
     *
     * @param capacity The number of elements to allocate memory for.
     * @return Pointer to the allocated raw memory.
     */
    static T* allocate(const vecSizeType capacity)
    {
        return static_cast<T*>(operator new[](capacity * sizeof(T)));
    }

    /**
     * @brief Frees the raw memory.
     *
     * @param data Pointer to the memory to deallocate.
     */
    static void deallocate(T* data)
    {
        operator delete[](data);
    }

public:
    /**
     * @brief Default constructor.
     *
     * Initializes the vector with a default capacity of 10.
     */
    Vector()
    {
        m_capacity = 10;
        m_data     = allocate(m_capacity);
    }

    /**
     * @brief Constructs a vector with a given size and initial value.
     *
     * @param n The initial number of elements.
     * @param value The value to initialize each element with (defaults to T()).
     */
    explicit Vector(const vecSizeType n, const T& value = T())
    {
        m_size     = n;
        m_capacity = n;
        m_data     = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; i++)
        {
            new(m_data + i) T(value);
        }
    }
    /**
     * @brief Constructs a vector from an initializer list.
     *
     * Initializes the vector with the elements provided in the initializer list.
     * The internal capacity is set to twice the size of the list to allow for efficient growth.
     *
     * @param list The initializer list containing the elements to populate the vector with.
     */
    Vector(std::initializer_list<T> list)
    {
        m_size     = static_cast<vecSizeType>(list.size());
        m_capacity = m_size;
        m_data     = allocate(m_capacity);

        vecSizeType i = 0;
        for (const T& item : list)
        {
            new(m_data + i++) T(item);
        }
    }

    Vector(const T* first, const T* last)
    {
        m_size     = static_cast<vecSizeType>(last - first);
        m_capacity = m_size;
        m_data     = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; ++i)
        {
            new (m_data + i) T(first[i]);
        }
    }

    /**
	 * @brief Constructs a vector with reserved capacity, uninitialized.
	 *
	 * This constructor allocates memory but does not construct any elements.
	 * Use this when you intend to manually fill the vector later.
	 *
	 * @param capacity The amount of space to reserve (uninitialized).
	 */
    explicit Vector(vecSizeType capacity, nullptr_t)
    {
	    m_size     = 0;
	    m_capacity = capacity;
	    m_data     = allocate(m_capacity);
    }

    Vector(const T* src, vecSizeType n)
    {
        m_size = n;
        m_capacity = n;
        m_data = allocate(m_capacity);

        for (vecSizeType i = 0; i < m_size; ++i)
            new (m_data + i) T(src[i]);
    }

    /**
     * @brief Copy constructor.
     *
     * Creates a new vector as a copy of another.
     *
     * @param other The vector to copy from.
     */
    Vector(const Vector& other)
    {
        m_size     = other.m_size;
        m_capacity = other.m_capacity;
        m_data     = allocate(m_capacity);
        for (vecSizeType i = 0; i < m_size; i++)
        {
            new(m_data + i) T(other.m_data[i]);
        }
    }

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of resources from another vector.
     *
     * @param other The vector to move from.
     */
    Vector(Vector&& other) noexcept
        : m_size(other.m_size), m_capacity(other.m_capacity), m_data(other.m_data)
    {
        other.m_data     = nullptr;
        other.m_size     = 0;
        other.m_capacity = 0;
    }


    /**
     * @brief Copy assignment operator.
     *
     * Replaces the contents of this vector with a copy of another.
     *
     * @param other The vector to copy from.
     * @return Reference to this vector.
     */
    Vector& operator=(const Vector& other)
    {
        if (this != &other)
        {
            clear();
            deallocate(m_data);

            m_size     = other.m_size;
            m_capacity = other.m_capacity;
            m_data     = allocate(m_capacity);
            for (vecSizeType i = 0; i < m_size; i++)
            {
                new(m_data + i) T(other.m_data[i]);
            }
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     *
     * Transfers resources from another vector.
     *
     * @param other The vector to move from.
     * @return Reference to this vector.
     */
    Vector& operator=(Vector&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            deallocate(m_data);
            m_data     = other.m_data;
            m_size     = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data     = nullptr;
            other.m_size     = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    /**
     * @brief Destructor.
     *
     * Destroys all elements and frees allocated memory.
     */
    ~Vector()
    {
        clear();
        deallocate(m_data);
    }

    T* data() { return m_data; }
    const T* data() const { return m_data; }

    T* begin() { return m_data; }
    const T* begin() const { return m_data; }

    T* end() { return m_data + m_size; }
    const T* end() const { return m_data + m_size; }

    T& back() { assert(m_size > 0); return m_data[m_size - 1]; }
    const T& back() const { assert(m_size > 0); return m_data[m_size - 1]; }

    [[nodiscard]] vecSizeType size() const { return m_size; }
    [[nodiscard]] vecSizeType size_bytes() const { return (sizeof(T) * m_size); }
    [[nodiscard]] vecSizeType capacity() const { return m_capacity; }
    [[nodiscard]] bool empty() const { return m_size == 0; }

    T& operator[](vecSizeType idx)
    {
    	assert(idx < m_size && "Vector::operator[] out-of-bounds");
        return m_data[idx];
    }
    const T& operator[](vecSizeType idx) const
    {
    	assert(idx < m_size && "Vector::operator[] const out-of-bounds");
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


    /**
     * @brief Adds an element to the end of the vector.
     *
     * @param object The element to add.
     */
    void push_back(const T& object)
    {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (m_data + m_size++) T(object);
    }

    /**
     * @brief Adds an element to the end of the vector by moving it.
     *
     * @param object The element to add.
     */
    void push_back(T&& object)
    {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (m_data + m_size++) T(std::move(object));
    }

    /**
     * @brief Constructs an element in place at the end of the vector.
     *
     * Forwards the arguments to the element's constructor.
     *
     * @tparam Args The types of constructor arguments.
     * @param args The arguments to forward.
     */
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


    /**
     * @brief Checks whether the vector contains the given element.
     *
     * @param object The element to search for.
     * @return True if the element is found; false otherwise.
     */
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

    /**
     * @brief Removes the last element from the vector.
     *
     * If the vector becomes less than one quarter full, the capacity is reduced.
     */
    void pop_back()
    {
        if (m_size > 0)
        {
            m_size--;
            m_data[m_size].~T();
        }
    }

    /**
     * @brief Erases the element at the specified index.
     *
     * Shifts subsequent elements to fill the gap.
     *
     * @param position The index of the element to remove.
     */
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


    /**
     * @brief Inserts an element at the specified index.
     *
     * Shifts existing elements to make room for the new element.
     *
     * @param idx The index at which to insert.
     * @param object The element to insert.
     */
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

    /**
     * @brief Removes all elements from the vector.
     *
     * Calls the destructor on each element but retains the allocated memory.
     */
    void clear()
    {
        for (vecSizeType i = 0; i < m_size; ++i)
        {
            m_data[i].~T();
        }
        m_size = 0;
    }
};
#endif