//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_IMPL_ARRAY_HPP
#define BOOST_JSON_IMPL_ARRAY_HPP

#include <boost/json/value.hpp>
#include <boost/json/detail/except.hpp>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

BOOST_JSON_NS_BEGIN

class array::undo_construct
{
    array& self_;

public:
    bool commit = false;

    BOOST_JSON_DECL
    ~undo_construct();

    explicit
    undo_construct(
        array& self) noexcept
        : self_(self)
    {
    }
};

//----------------------------------------------------------

class array::undo_insert
{
    array& self_;
    std::size_t const n_;

public:
    value* it;
    std::size_t const pos;
    bool commit = false;

    BOOST_JSON_DECL
    undo_insert(
        value const* pos_,
        std::size_t n,
        array& self);

    BOOST_JSON_DECL
    ~undo_insert();

    template<class Arg>
    void
    emplace(Arg&& arg)
    {
        ::new(it) value(
            std::forward<Arg>(arg),
            self_.sp_);
        ++it;
    }
};

auto
array::
header::
allocate(
    std::uint32_t n,
    const storage_ptr& sp) ->
        header*
{
    BOOST_STATIC_ASSERT(
        sizeof(header) <= sizeof(value));
    return ::new(sp->allocate(
        (n + 1) * sizeof(value))) header(n);
}

void
array::
header::
deallocate(
    const storage_ptr& sp) noexcept
{
    if(! sp.is_not_counted_and_deallocate_is_trivial())
        sp->deallocate(this, (capacity + 1) * sizeof(value));
}

value*
array::
header::
data() noexcept
{
    return reinterpret_cast<
        value*>(this) + 1;
}

constexpr
std::size_t
array::
max_size() noexcept
{
    // max_size depends on the address model
    using min = std::integral_constant<std::size_t,
        (std::size_t(-1) - sizeof(value)) / sizeof(value)>;
    return min::value < BOOST_JSON_MAX_STRUCTURED_SIZE ?
        min::value : BOOST_JSON_MAX_STRUCTURED_SIZE;
}

//----------------------------------------------------------
//
// Element access
//
//----------------------------------------------------------

value&
array::
at(std::size_t pos)
{
    if(! ptr_ || pos >= ptr_->size)
        detail::throw_out_of_range(
            BOOST_CURRENT_LOCATION);
    return ptr_->data()[pos];
}

value const&
array::
at(std::size_t pos) const
{
    if(! ptr_ || pos >= ptr_->size)
        detail::throw_out_of_range(
            BOOST_CURRENT_LOCATION);
    return ptr_->data()[pos];
}

value&
array::
operator[](std::size_t pos) noexcept
{
    return ptr_->data()[pos];
}

value const&
array::
operator[](std::size_t pos) const noexcept
{
    return ptr_->data()[pos];
}

value&
array::
front() noexcept
{
    BOOST_ASSERT(ptr_);
    return *ptr_->data();
}

value const&
array::
front() const noexcept
{
    BOOST_ASSERT(ptr_);
    return *ptr_->data();
}

value&
array::
back() noexcept
{
    BOOST_ASSERT(ptr_);
    return ptr_->data()[ptr_->size - 1];
}

value const&
array::
back() const noexcept
{
    BOOST_ASSERT(ptr_);
    return ptr_->data()[ptr_->size - 1];
}


value*
array::
data() noexcept
{
    return ptr_ ? ptr_->data() : nullptr;
}

value const*
array::
data() const noexcept
{
    return ptr_ ? ptr_->data() : nullptr;
}

value const*
array::
contains(std::size_t pos) const noexcept
{
    if(! ptr_ || pos >= ptr_->size)
        return nullptr;
    return ptr_->data() + pos;
}

value*
array::
contains(std::size_t pos) noexcept
{
    if(! ptr_ || pos >= ptr_->size)
        return nullptr;
    return ptr_->data() + pos;
}

//----------------------------------------------------------
//
// Iterators
//
//----------------------------------------------------------

auto
array::
begin() noexcept ->
    iterator
{
    return data();
}

auto
array::
begin() const noexcept ->
    const_iterator
{
    return data();
}

auto
array::
cbegin() const noexcept ->
    const_iterator
{
    return begin();
}

auto
array::
end() noexcept ->
    iterator
{
    return ptr_ ? ptr_->data() + 
        ptr_->size : nullptr;
}

auto
array::
end() const noexcept ->
    const_iterator
{
    return ptr_ ? ptr_->data() + 
        ptr_->size : nullptr;
}

auto
array::
cend() const noexcept ->
    const_iterator
{
    return end();
}

auto
array::
rbegin() noexcept ->
    reverse_iterator
{
    return reverse_iterator(end());
}

auto
array::
rbegin() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(end());
}

auto
array::
crbegin() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(cend());
}

auto
array::
rend() noexcept ->
    reverse_iterator
{
    return reverse_iterator(begin());
}

auto
array::
rend() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(begin());
}

auto
array::
crend() const noexcept ->
    const_reverse_iterator
{
    return const_reverse_iterator(cbegin());
}

//----------------------------------------------------------
//
// function templates
//
//----------------------------------------------------------

template<class InputIt, class>
array::
array(
    InputIt first, InputIt last,
    storage_ptr sp)
    : array(
        first, last,
        std::move(sp),
        iter_cat<InputIt>{})
{
    static_assert(
        std::is_constructible<value,
            decltype(*first)>::value,
        "json::value is not constructible from the iterator's value type");
}

template<class InputIt, class>
auto
array::
insert(
    const_iterator pos,
    InputIt first, InputIt last) ->
        iterator
{
    static_assert(
        std::is_constructible<value,
            decltype(*first)>::value,
        "json::value is not constructible from the iterator's value type");
    return insert(pos, first, last,
        iter_cat<InputIt>{});
}

template<class Arg>
auto
array::
emplace(
    const_iterator pos,
    Arg&& arg) ->
        iterator
{
    BOOST_ASSERT(ptr_);
    const auto n = ptr_->size;
    const auto index = 
        pos - ptr_->data();
    if(n >= max_size())
        detail::throw_length_error(
            "array too large",
            BOOST_CURRENT_LOCATION);
    reserve(ptr_->size + 1);
    union U
    {
        value v;

        U() { }
        ~U() { }
    } u;
    ::new(&u.v) value(
        std::forward<Arg>(arg), sp_);
    const auto dest = ptr_->data() + index;
    const auto end = ptr_->data() + n;
    std::memmove(dest + 1, dest, 
        (end - dest) * sizeof(value));
    std::memcpy(dest, &u.v, sizeof(value));
    ++ptr_->size;
    return dest;
}

template<class Arg>
value&
array::
emplace_back(Arg&& arg)
{
    reserve(size() + 1);
    auto& v = *::new(
        ptr_->data() + ptr_->size++) value(
            std::forward<Arg>(arg), sp_);
    return v;
}

//----------------------------------------------------------
//
// implementaiton
//
//----------------------------------------------------------

template<class InputIt>
array::
array(
    InputIt first, InputIt last,
    storage_ptr sp,
    std::input_iterator_tag)
    : sp_(std::move(sp))
{
    undo_construct u(*this);
    while(first != last)
    {
        if(size() >= capacity())
            reserve(size() + 1);
        ::new(data() + size()) value(
            *first++, sp_);
        ++ptr_->size;
    }
    u.commit = true;
}

template<class InputIt>
array::
array(
    InputIt first, InputIt last,
    storage_ptr sp,
    std::forward_iterator_tag)
    : sp_(std::move(sp))
{
    undo_construct u(*this);
    auto const n =
        static_cast<std::size_t>(
            std::distance(first, last));
    if(n)
    {
        if(n > max_size())
            detail::throw_length_error(
                "array too large",
                BOOST_CURRENT_LOCATION);
        ptr_ = header::allocate(n, sp_);
        ptr_->size = 0;
        while(size() < n)
        {
            ::new(data() + size()) 
                value(*first++, sp_);
            ++ptr_->size;
        }
        u.commit = true;
    }
}

template<class InputIt>
auto
array::
insert(
    const_iterator pos,
    InputIt first, InputIt last,
    std::input_iterator_tag) ->
        iterator
{
    if(first == last)
        return const_cast<iterator>(pos);
    array tmp(first, last, sp_);
    undo_insert u(
        pos, tmp.size(), *this);
    relocate(u.it,
        tmp.data(), tmp.size());
    // don't destroy values in tmp
    if(! tmp.empty())
        tmp.ptr_->size = 0;
    u.commit = true;
    return data() + u.pos;
}

template<class InputIt>
auto
array::
insert(
    const_iterator pos,
    InputIt first, InputIt last,
    std::forward_iterator_tag) ->
        iterator
{
    auto const n =
        static_cast<std::size_t>(
            std::distance(first, last));
    if(n > max_size())
        detail::throw_length_error(
            "array too large",
            BOOST_CURRENT_LOCATION);
    undo_insert u(pos, static_cast<
        std::size_t>(n), *this);
    while(first != last)
        u.emplace(*first++);
    u.commit = true;
    return data() + u.pos;
}

BOOST_JSON_NS_END

#endif
