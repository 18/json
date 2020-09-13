//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_IMPL_ARRAY_IPP
#define BOOST_JSON_IMPL_ARRAY_IPP

#include <boost/json/array.hpp>
#include <boost/json/pilfer.hpp>
#include <boost/json/detail/except.hpp>
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>

BOOST_JSON_NS_BEGIN

array::
undo_construct::
~undo_construct()
{
    if(! commit && self_.ptr_)
    {
        self_.destroy(self_.data(), self_.size());
        self_.ptr_->deallocate(self_.sp_);
    }
}

//----------------------------------------------------------

array::
undo_insert::
undo_insert(
    value const* pos_,
    std::size_t n,
    array& self)
    : self_(self)
    , n_(n)
    , pos(pos_ - self_.data())
{
    if(n > max_size())
        detail::throw_length_error(
            "array too large",
            BOOST_CURRENT_LOCATION);
    self_.reserve(
        self_.size() + n_);
    // (iterators invalidated now)
    it = self_.data() + pos;
    relocate(it + n_, it,
        self_.size() - pos);
    if(n && self_.ptr_)
        self_.ptr_->size += n;
}

array::
undo_insert::
~undo_insert()
{
    if(! commit)
    {
        auto const first =
            self_.data() + pos;
        self_.destroy(first, it);
        if(n_ && self_.ptr_)
            self_.ptr_->size -= n_;
        relocate(
            first, first + n_,
            self_.size() - pos);
    }
}

//----------------------------------------------------------
//
// Special Members
//
//----------------------------------------------------------

array::
array(detail::unchecked_array&& ua)
    : sp_(ua.storage())
{
    const auto n = ua.size();
    if(n)
    {
        ptr_ = header::allocate(n, sp_);
        ua.relocate(ptr_->data());
        ptr_->size = n;
    }
}

array::
array(storage_ptr sp) noexcept
    : sp_(std::move(sp))
    , ptr_(nullptr)
{
    // silence -Wunused-private-field
    k_ = kind::array;
}

array::
array(
    std::size_t count,
    value const& v,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(count)
    {
        undo_construct u(*this);
        ptr_ = header::allocate(count, sp_);
        ptr_->size = 0;
        while(ptr_->size < count)
        {
            ::new(ptr_->data() + ptr_->size) 
                value(v, sp_);
            ++ptr_->size;
        }
        u.commit = true;
        return;
    }
    ptr_ = nullptr;
}

array::
array(
    std::size_t count,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(count)
    {
        ptr_ = header::allocate(
            count, sp_);
        ptr_->size = count;
        auto first = ptr_->data();
        const auto last = first + count;
        while(first != last)
            ::new(first++) value(sp_);
        return;
    }
    ptr_ = nullptr;
}

array::
array(array const& other)
    : sp_(other.sp_)
{
    if(other.ptr_)
    {
        undo_construct u(*this);
        copy(other);
        u.commit = true;
    }
}

array::
array(
    array const& other,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(other.ptr_)
    {
        undo_construct u(*this);
        copy(other);
        u.commit = true;
    }
}

array::
array(pilfered<array> other) noexcept
    : sp_(detail::exchange(
        other.get().sp_, storage_ptr()))
    , ptr_(detail::exchange(
        other.get().ptr_, nullptr))
{
}

array::
array(array&& other) noexcept
    : sp_(other.sp_)
    , ptr_(detail::exchange(
        other.ptr_, nullptr))
{
}

array::
array(
    array&& other,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    if(*sp_ == *other.sp_)
    {
        ptr_ = detail::exchange(
            other.ptr_, nullptr);
    }
    else
    {
        undo_construct u(*this);
        copy(other);
        u.commit = true;
    }
}

array::
array(
    std::initializer_list<value_ref> init,
    storage_ptr sp)
    : sp_(std::move(sp))
{
    const auto n = init.size();
    if(n)
    {
        undo_construct u(*this);
        reserve(init.size());
        value_ref::write_array(
            data(), init, sp_);
        ptr_->size = n;
        u.commit = true;
        return;
    }
    ptr_ = nullptr;
}

//----------------------------------------------------------

array&
array::
operator=(array const& other)
{
    if(this == &other)
        return *this;
    array tmp(other, sp_);
    this->~array();
    ::new(this) array(pilfer(tmp));
    return *this;
}

array&
array::
operator=(array&& other)
{
    array tmp(std::move(other), sp_);
    this->~array();
    ::new(this) array(pilfer(tmp));
    return *this;
}

array&
array::
operator=(
    std::initializer_list<value_ref> init)
{
    array tmp(init, sp_);
    this->~array();
    ::new(this) array(pilfer(tmp));
    return *this;
}

auto
array::
get_allocator() const noexcept ->
    allocator_type
{
    return sp_.get();
}

//----------------------------------------------------------
//
// Capacity
//
//----------------------------------------------------------

void
array::
shrink_to_fit() noexcept
{
    if(capacity() <= size())
        return;
    if(ptr_->size == 0)
    {
        ptr_->deallocate(sp_);
        ptr_ = nullptr;
        return;
    }

#ifndef BOOST_NO_EXCEPTIONS
    try
    {
#endif
        const auto n = ptr_->size;
        auto ptr = header::allocate(n, sp_);
        std::memcpy(ptr->data(), ptr_->data(), 
            n * sizeof(value));
        ptr->size = ptr_->size;
        ptr_->deallocate(sp_);
        ptr_ = ptr;
#ifndef BOOST_NO_EXCEPTIONS
    }
    catch(...)
    {
        // eat the exception
        return;
    }
#endif
}

//----------------------------------------------------------
//
// Modifiers
//
//----------------------------------------------------------

void
array::
clear() noexcept
{
    if(ptr_)
    {
        if(! sp_.is_not_counted_and_deallocate_is_trivial())
            destroy(ptr_->data(), ptr_->size);
        ptr_->size = 0;
    }
}

auto
array::
insert(
    const_iterator pos,
    std::size_t count,
    value const& v) ->
        iterator
{
    undo_insert u(pos, count, *this);
    while(count--)
        u.emplace(v);
    u.commit = true;
    return data() + u.pos;
}

auto
array::
insert(
    const_iterator pos,
    std::initializer_list<
        value_ref> init) ->
    iterator
{
    undo_insert u(
        pos, init.size(), *this);
    value_ref::write_array(
        data() + u.pos, init, sp_);
    u.commit = true;
    return data() + u.pos;
}

auto
array::
erase(
    const_iterator pos) noexcept ->
    iterator
{
    const auto p = 
        const_cast<iterator>(pos);
    if(! sp_.is_not_counted_and_deallocate_is_trivial())
        p->~value();
    const auto n = end() - p;
    relocate(p, p + 1, n);
    --ptr_->size;
    return p;
}

auto
array::
erase(
    const_iterator first,
    const_iterator last) noexcept ->
        iterator
{
    auto const p =
        const_cast<iterator>(first);
    auto const e = 
        const_cast<iterator>(last);
    if(ptr_)
    {
        auto const n = static_cast<
            std::size_t>(e - p);
        destroy(p, e);
        relocate(p, e, ptr_->size - n);
        ptr_->size -= n;
    }
    return p;
}

void
array::
pop_back() noexcept
{
    if(! sp_.is_not_counted_and_deallocate_is_trivial())
        (ptr_->data() + ptr_->size)->~value();
    --ptr_->size;
}

void
array::
resize(std::size_t count)
{
    value* first;
    value* last;
    if(ptr_)
    {
        if(ptr_->size > count)
        {
            if(! sp_.is_not_counted_and_deallocate_is_trivial())
                destroy(ptr_->data() + count, count);
            ptr_->size = count;
            return;
        }
        first = ptr_->data() + ptr_->size;
        last = ptr_->data() + count;
        reserve(count);
    }
    else 
    {
        if(! count)
            return;
        ptr_ = header::allocate(count, sp_);
        first = ptr_->data();
        last = first + count;
    }
    while(first != last)
        ::new(first++) value(sp_);
    ptr_->size = count;
}

void
array::
resize(
    std::size_t count,
    value const& v)
{
    if(count <= size())
    {
        destroy(data() + count, end());
        
        if(ptr_)
            ptr_->size = count;
        return;
    }

    reserve(count);

    struct undo
    {
        array& self;
        value* it;
        value* const end;

        ~undo()
        {
            if(it)
                self.destroy(end, it);
        }
    };

    auto const end =
        data() + count;
    undo u{*this,
        data() + size(),
        data() + size()};
    do
    {
        ::new(u.it) value(v, sp_);
        ++u.it;
    }
    while(u.it != end);
    ptr_->size = count;
    u.it = nullptr;
}

void
array::
swap(array& other)
{
    BOOST_ASSERT(this != &other);
    if(*sp_ == *other.sp_)
    {
        std::swap(ptr_, other.ptr_);
        return;
    }
    array temp1(
        std::move(*this),
        other.storage());
    array temp2(
        std::move(other),
        this->storage());
    this->~array();
    ::new(this) array(pilfer(temp2));
    other.~array();
    ::new(&other) array(pilfer(temp1));
}

//----------------------------------------------------------

void
array::
destroy(
    value* first, value* last) noexcept
{
    if(sp_.is_not_counted_and_deallocate_is_trivial())
        return;
    while(last != first)
        (*--last).~value();
}

void
array::
destroy(
    value* ptr, std::size_t n) noexcept
{
    const auto last = ptr + n;
    while(ptr != last)
        (ptr++)->~value();
}

void
array::
copy(array const& other)
{
    reserve(other.size());
    for(auto const& v : other)
    {
        ::new(
            data() +
            size()) value(v, sp_);
        ++ptr_->size;
    }
}

void
array::
reserve_impl(std::size_t capacity)
{
    if(ptr_)
    {
        // 2x growth
        const auto old = 
            ptr_->capacity;
        auto const hint =
            old * 2;
        if(hint < old) // overflow
            capacity = max_size();
        else if(capacity < hint)
            capacity = hint;
    }
    header* ptr = header::allocate(
        capacity, sp_);
    if(ptr_)
    {
        relocate(ptr->data(),
            ptr_->data(), ptr_->size);
        ptr->size = ptr_->size;
        ptr_->deallocate(sp_);
    }
    else
    {
        ptr->size = 0;
    }
    ptr_ = ptr;
}

void
array::
relocate(
    value* dest,
    value* src,
    std::size_t n) noexcept
{
    if(n == 0)
        return;
    std::memmove(
        reinterpret_cast<
            void*>(dest),
        reinterpret_cast<
            void const*>(src),
        n * sizeof(value));
}

//----------------------------------------------------------

bool
array::
equal(
    array const& other) const noexcept
{
    if(size() != other.size())
        return false;
    for(std::size_t i = 0; i < size(); ++i)
        if((*this)[i] != other[i])
            return false;
    return true;
}

BOOST_JSON_NS_END

#endif
