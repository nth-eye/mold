#ifndef MOLD_TYPES_VECTOR_H
#define MOLD_TYPES_VECTOR_H

/**
 * @file
 * @brief Fixed-capacity dynamic-size vector with JSON/CBOR/MessagePack serialization.
 */

#include <cassert>
#include <span>
#include <limits>
#include <memory>
#include <compare>
#include <algorithm>
#include "mold/refl/spec.h"
#include "mold/util/container.h"

namespace mold {
namespace detail {

#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 14)

/**
 * @brief Uninitialized storage for N elements of type T (GCC >= 14 workaround).
 *
 * GCC 14+ rejects unions with non-trivial members in constexpr context.
 * This partial specialization uses a struct with value-initialized array
 * for trivially constructible types and an uninitialized array otherwise.
 *
 * @tparam T Element type
 * @tparam N Number of elements
 * @tparam IsTriviallyConstructible Whether T is trivially constructible
 */
template<class T, size_t N, bool IsTriviallyConstructible>
struct storage_impl;

/**
 * @brief Trivially constructible specialization with value-initialized array.
 *
 * @tparam T Trivially constructible element type
 * @tparam N Number of elements
 */
template<class T, size_t N>
struct storage_impl<T, N, true> {
    static_assert(N);
    constexpr storage_impl() noexcept {}
    constexpr ~storage_impl() noexcept {}
    constexpr operator T*()             { return buf; }
    constexpr operator const T*() const { return buf; }
private:
    T buf[N]{};
};

/**
 * @brief Non-trivially constructible specialization with uninitialized array.
 *
 * @tparam T Non-trivially constructible element type
 * @tparam N Number of elements
 */
template<class T, size_t N>
struct storage_impl<T, N, false> {
    static_assert(N);
    constexpr storage_impl() noexcept {}
    constexpr ~storage_impl() noexcept {}
    constexpr operator T*()             { return buf; }
    constexpr operator const T*() const { return buf; }
private:
    T buf[N];
};

/**
 * @brief Alias selecting the appropriate storage implementation.
 *
 * @tparam T Element type
 * @tparam N Number of elements
 */
template<class T, size_t N>
using storage = storage_impl<T, N, std::is_trivially_constructible_v<T>>;

#else

/**
 * @brief Union-based uninitialized storage for constexpr-compatible placement construction.
 *
 * The union's trivial default constructor leaves the buffer uninitialized,
 * allowing elements to be placement-constructed individually.
 *
 * @tparam T Element type
 * @tparam N Number of elements
 */
template<class T, size_t N>
union storage {
    static_assert(N);
    constexpr storage() noexcept {}
    constexpr ~storage() noexcept {}
    constexpr operator T*()             { return buf; }
    constexpr operator const T*() const { return buf; }
private:
    T buf[N];
};

#endif

/**
 * @brief Placement-construct a single element.
 *
 * @tparam T Element type
 * @tparam Args Constructor argument types
 *
 * @param p Pointer to uninitialized storage
 * @param args Constructor arguments
 * @return Pointer to the constructed element
 */
template<class T, class... Args>
constexpr T* ctor(T* p, Args&&... args)
{
    return std::construct_at(p, std::forward<Args>(args)...);
}

/**
 * @brief Placement-construct n elements with the same arguments.
 *
 * Arguments are taken by const reference to prevent moved-from state
 * when constructing multiple elements.
 *
 * @tparam T Element type
 * @tparam Args Constructor argument types
 *
 * @param p Pointer to the first uninitialized slot
 * @param n Number of elements to construct
 * @param args Constructor arguments (copied for each element)
 */
template<class T, class... Args>
constexpr void ctor_n(T* p, size_t n, const Args&... args)
{
    for (size_t i = 0; i < n; ++i)
        ctor(p + i, args...);
}

/**
 * @brief Destroy n consecutive elements starting at the given iterator.
 *
 * @param it Iterator to the first element
 * @param n Number of elements to destroy
 */
constexpr void dtor_n(std::forward_iterator auto it, size_t n)
{
    std::destroy_n(it, n);
}

/**
 * @brief Destroy elements in the range [head, tail).
 *
 * @tparam It Forward iterator type
 *
 * @param head Iterator to the first element
 * @param tail Iterator past the last element
 */
template<std::forward_iterator It>
constexpr void dtor(It head, It tail)
{
    std::destroy(head, tail);
}

/**
 * @brief Destroy a single element at the given iterator.
 *
 * @param it Iterator to the element to destroy
 */
constexpr void dtor(std::forward_iterator auto it)
{
    std::destroy_at(std::addressof(*it));
}

/**
 * @brief Move-construct elements forward into uninitialized storage.
 *
 * @tparam InIt Source iterator type
 * @tparam OutIt Destination iterator type
 *
 * @param first Source begin
 * @param last Source end
 * @param d_first Destination begin (uninitialized)
 * @return Iterator past the last constructed element
 */
template<std::forward_iterator InIt, std::forward_iterator OutIt>
constexpr OutIt move_create_forward(InIt first, InIt last, OutIt d_first)
{
    while (first != last)
        ctor(d_first++, std::move(*first++));
    return d_first;
}

/**
 * @brief Move-assign elements backward into live storage.
 *
 * @tparam InIt Source bidirectional iterator type
 * @tparam OutIt Destination bidirectional iterator type
 *
 * @param first Source begin
 * @param last Source end
 * @param d_last Destination past-the-end
 * @return Iterator to the first assigned destination element
 */
template<std::bidirectional_iterator InIt, std::bidirectional_iterator OutIt>
constexpr OutIt move_assign_backward(InIt first, InIt last, OutIt d_last)
{
    while (first != last)
        *--d_last = std::move(*--last);
    return d_last;
}

/**
 * @brief Move-assign elements forward into live storage.
 *
 * @tparam InIt Source iterator type
 * @tparam OutIt Destination iterator type
 *
 * @param first Source begin
 * @param last Source end
 * @param d_first Destination begin (must be live)
 * @return Iterator past the last assigned destination element
 */
template<std::forward_iterator InIt, std::forward_iterator OutIt>
constexpr OutIt move_assign_forward(InIt first, InIt last, OutIt d_first)
{
    while (first != last)
        *d_first++ = std::move(*first++);
    return d_first;
}

/**
 * @brief Copy-assign elements forward into live storage.
 *
 * @tparam InIt Source iterator type
 * @tparam OutIt Destination iterator type
 *
 * @param first Source begin
 * @param last Source end
 * @param d_first Destination begin (must be live)
 * @return Iterator past the last assigned destination element
 */
template<std::forward_iterator InIt, std::forward_iterator OutIt>
constexpr OutIt copy_assign_forward(InIt first, InIt last, OutIt d_first)
{
    while (first != last)
        *d_first++ = *first++;
    return d_first;
}

/**
 * @brief Copy-construct elements forward into uninitialized storage.
 *
 * @tparam InIt Source iterator type
 * @tparam OutIt Destination iterator type
 *
 * @param first Source begin
 * @param last Source end
 * @param d_first Destination begin (uninitialized)
 * @return Iterator past the last constructed element
 */
template<std::forward_iterator InIt, std::forward_iterator OutIt>
constexpr OutIt copy_create_forward(InIt first, InIt last, OutIt d_first)
{
    while (first != last)
        ctor(d_first++, *first++);
    return d_first;
}

} // namespace detail

/**
 * @brief Fixed-capacity dynamic-size vector.
 *
 * Pre-allocates storage for N elements without default-constructing them.
 * Serializes as an array in JSON, CBOR, and MessagePack.
 *
 * @tparam T Element type
 * @tparam N Maximum number of elements
 */
template<class T, size_t N>
struct vector_t {

    using value_type             = T;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using pointer                = value_type*;
    using const_pointer          = const value_type*;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /**
     * @brief Default constructor, creates an empty vector.
     *
     */
    constexpr vector_t() noexcept = default;

    /**
     * @brief Copy constructor.
     *
     * @param other Source vector to copy from
     */
    constexpr vector_t(const vector_t& other)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
        : len{other.size()}
    {
        detail::copy_create_forward(other.begin(), other.end(), begin());
    }

    /**
     * @brief Move constructor.
     *
     * @param other Source vector to move from
     */
    constexpr vector_t(vector_t&& other)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : len{other.size()}
    {
        detail::move_create_forward(other.begin(), other.end(), begin());
    }

    /**
     * @brief Construct with n value-initialized elements.
     *
     * @param n Number of elements
     */
    constexpr explicit vector_t(size_type n) : len{n}
    {
        assert(capacity() >= len);
        detail::ctor_n(begin(), len);
    }

    /**
     * @brief Construct with n copies of x.
     *
     * @param n Number of elements
     * @param x Value to copy
     */
    constexpr explicit vector_t(size_type n, const_reference x) : len{n}
    {
        assert(capacity() >= len);
        detail::ctor_n(begin(), len, x);
    }

    /**
     * @brief Construct from iterator range [first, last).
     *
     * @tparam It Forward iterator type
     *
     * @param first Begin iterator
     * @param last End iterator
     */
    template<std::forward_iterator It>
    constexpr vector_t(It first, It last) : len{size_type(last - first)}
    {
        assert(capacity() >= len);
        detail::copy_create_forward(first, last, begin());
    }

    /**
     * @brief Construct from a span.
     *
     * @param s Source span
     */
    constexpr vector_t(std::span<const T> s) : len{s.size()}
    {
        assert(capacity() >= len);
        detail::copy_create_forward(s.begin(), s.end(), begin());
    }

    /**
     * @brief Construct from an initializer list.
     *
     * @param il Initializer list
     */
    constexpr vector_t(std::initializer_list<T> il) : len{il.size()}
    {
        assert(capacity() >= len);
        detail::copy_create_forward(il.begin(), il.end(), begin());
    }

    /**
     * @brief Destructor. Destroys all live elements.
     *
     */
    constexpr ~vector_t() noexcept
    {
        clear();
    }

    /**
     * @brief Copy assignment. Reuses existing element storage where possible.
     *
     * @param other Source vector
     * @return Reference to this vector
     */
    constexpr vector_t& operator=(const vector_t& other)
        noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        if (&other != this) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    /**
     * @brief Move assignment. Reuses existing element storage where possible.
     *
     * @param other Source vector
     * @return Reference to this vector
     */
    constexpr vector_t& operator=(vector_t&& other)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (&other != this) {
            impl_assign_move(other.begin(), other.end(), other.len);
        }
        return *this;
    }

    /**
     * @brief Initializer list assignment.
     *
     * @param il Initializer list
     * @return Reference to this vector
     */
    constexpr vector_t& operator=(std::initializer_list<T> il)
    {
        assign(il);
        return *this;
    }

    /**
     * @brief Replace contents with n copies of x.
     *
     * Assigns over existing elements in the overlap region, constructs or
     * destroys the remainder.
     *
     * @param n Number of elements
     * @param x Value to fill with
     */
    constexpr void assign(size_type n, const_reference x)
    {
        assert(capacity() >= n);
        size_type overlap = std::min(len, n);
        std::fill_n(begin(), overlap, x);
        if (n > len)
            detail::ctor_n(begin() + len, n - len, x);
        else
            detail::dtor_n(begin() + n, len - n);
        len = n;
    }

    /**
     * @brief Replace contents from iterator range [first, last).
     *
     * @tparam It Forward iterator type
     *
     * @param first Begin iterator
     * @param last End iterator
     */
    template<std::forward_iterator It>
    constexpr void assign(It first, It last)
    {
        impl_assign_copy(first, last, size_type(last - first));
    }

    /**
     * @brief Replace contents from a span.
     *
     * @param s Source span
     */
    constexpr void assign(std::span<const T> s)
    {
        impl_assign_copy(s.begin(), s.end(), s.size());
    }

    /**
     * @brief Replace contents from an initializer list.
     *
     * @param il Initializer list
     */
    constexpr void assign(std::initializer_list<T> il)
    {
        impl_assign_copy(il.begin(), il.end(), il.size());
    }

    /** @name Capacity */
    /** @{ */
    static constexpr size_type capacity()       { return N; }
    static constexpr size_type max_size()       { return N; }
    constexpr size_type size() const noexcept   { return len; }
    constexpr bool empty() const noexcept       { return len == 0; }
    constexpr bool full() const noexcept        { return len == capacity(); }
    /** @} */

    /** @name Iterators */
    /** @{ */
    constexpr iterator begin() noexcept              { return buf; }
    constexpr const_iterator begin() const noexcept  { return buf; }
    constexpr const_iterator cbegin() const noexcept { return begin(); }
    constexpr iterator end() noexcept                { return buf + len; }
    constexpr const_iterator end() const noexcept    { return buf + len; }
    constexpr const_iterator cend() const noexcept   { return end(); }

    constexpr reverse_iterator rbegin() noexcept              { return std::reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept  { return std::reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    constexpr reverse_iterator rend() noexcept                { return std::reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept    { return std::reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const noexcept   { return rend(); }
    /** @} */

    /** @name Element access */
    /** @{ */
    constexpr reference operator[](size_type i)             { assert(i < len); return buf[i]; }
    constexpr const_reference operator[](size_type i) const { assert(i < len); return buf[i]; }
    constexpr reference front()                             { assert(!empty()); return buf[0]; }
    constexpr const_reference front() const                 { assert(!empty()); return buf[0]; }
    constexpr reference back()                              { assert(!empty()); return buf[len - 1]; }
    constexpr const_reference back() const                  { assert(!empty()); return buf[len - 1]; }
    constexpr pointer data() noexcept                       { return buf; }
    constexpr const_pointer data() const noexcept           { return buf; }
    /** @} */

    /**
     * @brief Resize to n elements, value-initializing new ones.
     *
     * @param n New size
     */
    constexpr void resize(size_type n)
    {
        impl_resize(n);
    }

    /**
     * @brief Resize to n elements, filling new ones with x.
     *
     * @param n New size
     * @param x Value for new elements
     */
    constexpr void resize(size_type n, const_reference x)
    {
        impl_resize(n, x);
    }

    /**
     * @brief Destroy all elements and set size to 0.
     *
     */
    constexpr void clear() noexcept
    {
        detail::dtor_n(begin(), size());
        len = 0;
    }

    /**
     * @brief Append a copy of x.
     *
     * @param x Value to append
     */
    constexpr void push_back(const_reference x)
    {
        emplace_back(x);
    }

    /**
     * @brief Append by moving x.
     *
     * @param x Value to move-append
     */
    constexpr void push_back(T&& x)
    {
        emplace_back(std::move(x));
    }

    /**
     * @brief Construct an element in-place at the end.
     *
     * @tparam Args Constructor argument types
     *
     * @param args Constructor arguments
     * @return Reference to the newly constructed element
     */
    template<class... Args>
    constexpr reference emplace_back(Args&&... args)
    {
        assert(!full());
        auto& ref = *detail::ctor(begin() + len, std::forward<Args>(args)...);
        ++len;
        return ref;
    }

    /**
     * @brief Construct an element in-place before pos.
     *
     * Shifts subsequent elements right by one position.
     *
     * @tparam Args Constructor argument types
     *
     * @param pos Position to insert before
     * @param args Constructor arguments
     * @return Iterator to the newly constructed element
     */
    template<class... Args>
    constexpr iterator emplace(const_iterator pos, Args&&... args)
    {
        assert(!full());
        assert(begin() <= pos && pos <= end());
        auto ret = cast_itr(pos);
        if (ret != end()) {
            detail::ctor(end(), std::move(back()));
            detail::move_assign_backward(ret, end() - 1, end());
            detail::dtor(ret);
        }
        auto result = detail::ctor(ret, std::forward<Args>(args)...);
        ++len;
        return result;
    }

    /**
     * @brief Insert a copy of x before pos.
     *
     * @param pos Position to insert before
     * @param x Value to insert
     * @return Iterator to the inserted element
     */
    constexpr iterator insert(const_iterator pos, const_reference x)
    {
        return emplace(pos, x);
    }

    /**
     * @brief Insert by moving x before pos.
     *
     * @param pos Position to insert before
     * @param x Value to move-insert
     * @return Iterator to the inserted element
     */
    constexpr iterator insert(const_iterator pos, T&& x)
    {
        return emplace(pos, std::move(x));
    }

    /**
     * @brief Insert n copies of x before pos.
     *
     * @param pos Position to insert before
     * @param n Number of copies
     * @param x Value to insert
     * @return Iterator to the first inserted element
     */
    constexpr iterator insert(const_iterator pos, size_type n, const_reference x)
    {
        auto ret = impl_insert_move(pos, n);
        detail::ctor_n(ret, n, x);
        return ret;
    }

    /**
     * @brief Insert elements from range [first, last) before pos.
     *
     * @tparam InputIt Forward iterator type
     *
     * @param pos Position to insert before
     * @param first Begin iterator
     * @param last End iterator
     * @return Iterator to the first inserted element
     */
    template<std::forward_iterator InputIt>
    constexpr iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        auto ret = impl_insert_move(pos, last - first);
        detail::copy_create_forward(first, last, ret);
        return ret;
    }

    /**
     * @brief Insert elements from an initializer list before pos.
     *
     * @param pos Position to insert before
     * @param ilist Initializer list
     * @return Iterator to the first inserted element
     */
    constexpr iterator insert(const_iterator pos, std::initializer_list<T> ilist)
    {
        return insert(pos, ilist.begin(), ilist.end());
    }

    /**
     * @brief Remove the last element.
     *
     */
    constexpr void pop_back()
    {
        assert(!empty());
        detail::dtor(begin() + --len);
    }

    /**
     * @brief Unstable erase by index. O(1) by swapping with the last element.
     *
     * Does not preserve element order.
     *
     * @param pos Index of the element to remove
     */
    constexpr void pop_idx(size_type pos)
    {
        assert(size() > pos);
        if (pos != size() - 1)
            buf[pos] = std::move(back());
        detail::dtor(begin() + --len);
    }

    /**
     * @brief Unstable erase by iterator. O(1) by swapping with the last element.
     *
     * Does not preserve element order.
     *
     * @param pos Iterator to the element to remove
     */
    constexpr void pop(const_iterator pos)
    {
        pop_idx(pos - begin());
    }

    /**
     * @brief Unstable erase of range [first, last). Fills the gap from the tail.
     *
     * Does not preserve element order. Moves up to n elements from the
     * tail of the vector into the erased range, then destroys the remainder.
     *
     * @param first Begin of range to erase
     * @param last End of range to erase
     */
    constexpr void pop(const_iterator first, const_iterator last)
    {
        assert(begin() <= first && last <= end());
        size_type n = last - first;
        size_type to_move = std::min(size_type(end() - last), n);
        detail::move_assign_forward(end() - to_move, end(), cast_itr(first));
        detail::dtor_n(cast_itr(first) + to_move, n);
        len -= n;
    }

    /**
     * @brief Stable erase of a single element. Preserves element order.
     *
     * @param pos Iterator to the element to erase
     * @return Iterator to the element following the erased one
     */
    constexpr iterator erase(const_iterator pos)
    {
        return erase(pos, pos + 1);
    }

    /**
     * @brief Stable erase of range [first, last). Preserves element order.
     *
     * Shifts subsequent elements left, then destroys the vacated tail.
     *
     * @param first Begin of range to erase
     * @param last End of range to erase
     * @return Iterator to the element following the last erased one
     */
    constexpr iterator erase(const_iterator first, const_iterator last)
    {
        assert(begin() <= first && last <= end());
        auto dst = cast_itr(first);
        detail::move_assign_forward(cast_itr(last), end(), dst);
        auto new_end = end() - (last - first);
        detail::dtor_n(new_end, last - first);
        len -= last - first;
        return dst;
    }

    /**
     * @brief Swap contents with another vector of the same type and capacity.
     *
     * @param other Vector to swap with
     */
    constexpr void swap(vector_t& other) noexcept(std::is_nothrow_swappable_v<T>)
    {
        size_type common = std::min(len, other.len);
        for (size_type i = 0; i < common; ++i)
            std::swap(buf[i], other.buf[i]);
        if (len > other.len) {
            detail::move_create_forward(begin() + common, end(), other.begin() + common);
            detail::dtor_n(begin() + common, len - common);
        } else if (other.len > len) {
            detail::move_create_forward(other.begin() + common, other.end(), begin() + common);
            detail::dtor_n(other.begin() + common, other.len - common);
        }
        std::swap(len, other.len);
    }

    /**
     * @brief Element-wise equality comparison.
     *
     * @param o Vector to compare with
     * @return True if both vectors have the same size and equal elements
     */
    constexpr bool operator==(const vector_t& o) const
    {
        if (len != o.len)
            return false;
        return std::equal(begin(), end(), o.begin());
    }

    /**
     * @brief Lexicographic three-way comparison.
     *
     * @param o Vector to compare with
     * @return Ordering result
     */
    constexpr auto operator<=>(const vector_t& o) const
    {
        return std::lexicographical_compare_three_way(begin(), end(), o.begin(), o.end());
    }

    /**
     * @brief ADL-findable swap.
     *
     * @param a First vector
     * @param b Second vector
     */
    friend constexpr void swap(vector_t& a, vector_t& b) noexcept(std::is_nothrow_swappable_v<T>)
    {
        a.swap(b);
    }

private:
    /**
     * @brief Cast const_iterator to iterator (used internally where mutability is safe).
     *
     * @param itr Const iterator to cast
     * @return Mutable iterator
     */
    static constexpr iterator cast_itr(const_iterator itr)
    {
        return const_cast<iterator>(itr);
    }

    /**
     * @brief Resize implementation. Constructs or destroys elements as needed.
     *
     * @tparam Args Constructor argument types for new elements
     *
     * @param n Target size
     * @param args Arguments forwarded to element constructors
     */
    template<class... Args>
    constexpr void impl_resize(size_type n, Args&&... args)
    {
        assert(n <= capacity());
        if (n > len)
            detail::ctor_n(begin() + len, n - len, std::forward<Args>(args)...);
        else
            detail::dtor_n(begin() + n, len - n);
        len = n;
    }

    /**
     * @brief Move existing elements to make room for n insertions at pos.
     *
     * Splits the tail into a "move-create" region (into uninitialized storage)
     * and a "move-assign" region (into live slots), then destroys the source
     * slots that are now part of the insertion gap.
     *
     * @param pos Insertion point
     * @param n Number of slots to open
     * @return Iterator to the first opened slot (uninitialized)
     */
    constexpr iterator impl_insert_move(const_iterator pos, size_type n)
    {
        assert(size() + n <= capacity());
        assert(begin() <= pos && pos <= end());

        if (!n)
            return cast_itr(pos);

        auto insert_head = cast_itr(pos);
        auto insert_tail = insert_head + n;

        size_type till_end = end() - pos;
        size_type to_move_create;
        size_type to_move_assign;
        iterator create_dst_head;

        if (insert_tail > end()) {
            to_move_assign = 0;
            to_move_create = till_end;
            create_dst_head = insert_tail;
        } else {
            to_move_assign = till_end - n;
            to_move_create = n;
            create_dst_head = end();
        }
        detail::move_create_forward(
            end() - to_move_create,
            end(),
            create_dst_head);
        detail::move_assign_backward(
            insert_head,
            insert_head + to_move_assign,
            insert_tail + to_move_assign);
        detail::dtor_n(insert_head, to_move_create);
        len += n;
        return insert_head;
    }

    /**
     * @brief Copy-assign over the overlap region, then construct or destroy the rest.
     *
     * Follows the std::vector strategy: reuses live element storage via
     * copy-assignment where both old and new elements exist, avoiding
     * unnecessary destroy+construct round-trips.
     *
     * @tparam It Forward iterator type
     *
     * @param first Source begin
     * @param last Source end
     * @param n Number of source elements
     */
    template<std::forward_iterator It>
    constexpr void impl_assign_copy(It first, It last, size_type n)
    {
        assert(capacity() >= n);
        size_type overlap = std::min(len, n);
        detail::copy_assign_forward(first, first + overlap, begin());
        if (n > len)
            detail::copy_create_forward(first + overlap, last, begin() + len);
        else
            detail::dtor_n(begin() + n, len - n);
        len = n;
    }

    /**
     * @brief Move-assign over the overlap region, then construct or destroy the rest.
     *
     * Move variant of impl_assign_copy.
     *
     * @param first Source begin
     * @param last Source end
     * @param n Number of source elements
     */
    constexpr void impl_assign_move(iterator first, iterator last, size_type n)
    {
        assert(capacity() >= n);
        size_type overlap = std::min(len, n);
        detail::move_assign_forward(first, first + overlap, begin());
        if (n > len)
            detail::move_create_forward(first + overlap, last, begin() + len);
        else
            detail::dtor_n(begin() + n, len - n);
        len = n;
    }

    detail::storage<T, N> buf;  ///< Uninitialized backing storage.
    size_type len = 0;          ///< Number of live elements.
};

/**
 * @brief Container traits for `vector_t` (homogenous, dynamic size up to N).
 *
 * @tparam T Element type
 * @tparam N Maximum capacity
 */
template<class T, size_t N>
struct container_traits_t<vector_t<T, N>, void> {
    using element_type = T;
    static constexpr uint32_t element_count = std::numeric_limits<uint32_t>::max();
};

/**
 * @brief Serialization for `vector_t` (serialized as array).
 *
 * @tparam T Element type
 * @tparam N Maximum capacity
 */
template<class T, size_t N>
struct spec_t<vector_t<T, N>> {

    static constexpr json_type_t json_type = json_type_t::array;
    static constexpr cbor_type_t cbor_type = cbor_type_t::array;

    /**
     * @brief Prepare the next slot for deserialization by emplacing a default element.
     *
     * @param c Target vector
     * @param slot_idx Unused slot index
     * @return Pointer to the new element, or nullptr if full
     */
    static void* prepare(vector_t<T, N>& c, size_t /*slot_idx*/)
    {
        if (c.full()) {
            return nullptr;
        }
        return &c.emplace_back();
    }

    /**
     * @brief Advance to the next element for serialization.
     *
     * @param c Source vector
     * @param prev Pointer to the previous element, or nullptr to start
     * @return Pointer to the next element, or nullptr if done
     */
    static void* next(const vector_t<T, N>& c, const void* prev)
    {
        const T* b = c.data();
        const T* e = b + c.size();
        const T* n = prev ? static_cast<const T*>(prev) + 1 : b;
        return const_cast<T*>((n >= b && n < e) ? n : nullptr);
    }
};

}

#endif
