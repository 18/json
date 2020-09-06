//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_IMPL_VALUE_STACK_IPP
#define BOOST_JSON_IMPL_VALUE_STACK_IPP

#include <boost/json/value_stack.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

BOOST_JSON_NS_BEGIN

// destroy the values but
// not the stack allocation.
void
value_stack::
clear() noexcept
{
    if(top_ != base_)
    {
        if(! value_sp_.is_not_counted_and_deallocate_is_trivial())
            for(auto it = top_; it-- != base_;)
                it->~value();
        top_ = base_;
    }
}

// make room for at least one more value
void
value_stack::
grow()
{
    const std::size_t capacity =
        reinterpret_cast<const char*>(end_) -
        reinterpret_cast<const char*>(base_);
    const std::size_t size =
        reinterpret_cast<const char*>(top_) -
        reinterpret_cast<const char*>(base_);
    const std::size_t new_cap = base_ ? 
        capacity * 2 : min_capacity;
    value* const new_base = static_cast<value*>(
        stack_sp_->allocate(new_cap));
    if(BOOST_JSON_LIKELY(base_))
    {
        std::memcpy(reinterpret_cast<char*>(new_base),
            reinterpret_cast<char*>(base_), size);
        stack_sp_->deallocate(base_, capacity);
    }
    // book-keeping
    top_ = reinterpret_cast<value*>(
        reinterpret_cast<char*>(new_base) + size);
    end_ = reinterpret_cast<value*>(
        reinterpret_cast<char*>(new_base) + new_cap);
    base_ = new_base;
}

void
value_stack::
grow(
    std::size_t current,
    std::size_t total)
{
    const std::size_t capacity =
        reinterpret_cast<const char*>(end_) -
        reinterpret_cast<const char*>(base_);
    const std::size_t size =
        reinterpret_cast<const char*>(top_) -
        reinterpret_cast<const char*>(base_);
    const std::size_t needed = 
        size + total + sizeof(value);
    std::size_t new_cap = base_ ? 
        capacity : min_capacity;
    // VFALCO check overflow here
    while(new_cap < needed)
        new_cap *= 2;
    value* const new_base = static_cast<value*>(
        stack_sp_->allocate(new_cap));
    if(BOOST_JSON_LIKELY(base_))
    {
        std::size_t copy = size;
        if(current != 0)
            copy += current + sizeof(value);
        std::memcpy(reinterpret_cast<char*>(new_base),
            reinterpret_cast<char*>(base_), copy);
        stack_sp_->deallocate(base_, capacity);
    }
    // book-keeping
    top_ = reinterpret_cast<value*>(
        reinterpret_cast<char*>(new_base) + size);
    end_ = reinterpret_cast<value*>(
        reinterpret_cast<char*>(new_base) + new_cap);
    base_ = new_base;
}

//--------------------------------------

string_view
value_stack::
release_string(
    std::size_t n) noexcept
{
    // ensure a pushed value cannot
    // clobber the released string.
    BOOST_ASSERT(
        reinterpret_cast<char*>(
            top_ + 1) + n <=
        reinterpret_cast<char*>(
            end_));
    return { reinterpret_cast<
        char const*>(top_ + 1), n };
}

template<class... Args>
value&
value_stack::
push(Args&&... args)
{
    BOOST_ASSERT(top_ <= end_);
    if(BOOST_JSON_UNLIKELY(top_ == end_))
        grow();
    value& jv = detail::value_access::construct_value(
        top_, std::forward<Args>(args)..., value_sp_);
    ++top_;
    return jv;
}

template<class Unchecked>
void
value_stack::
exchange(Unchecked&& u)
{
    union U
    {
        value v;
        U() {}
        ~U() {}
    } jv;
    // construct value on the stack
    // to avoid clobbering top_[0],
    // which belongs to `u`.
    detail::value_access::construct_value(
        &jv.v, std::move(u));
    std::memcpy(reinterpret_cast<
        char*>(top_), &jv.v, sizeof(value));
    ++top_;
}

//----------------------------------------------------------

value_stack::
~value_stack()
{
    clear();
    if(base_)
        stack_sp_->deallocate(base_,
            reinterpret_cast<const char*>(end_) - 
            reinterpret_cast<const char*>(base_));
}

value_stack::
value_stack(storage_ptr sp) noexcept
    : stack_sp_(std::move(sp))
{
}

void
value_stack::
reset(storage_ptr sp) noexcept
{
    clear();
    value_sp_.~storage_ptr();
    ::new(&value_sp_) storage_ptr(
        pilfer(sp));
}

value
value_stack::
release() noexcept
{
    // This means the caller did not
    // cause a single top level element
    // to be produced.
    BOOST_ASSERT(top_ - base_ == 1);
    // give up shared ownership
    value_sp_ = {};
    return pilfer(*--top_);
}

//----------------------------------------------------------

void
value_stack::
push_array(std::size_t n)
{
    BOOST_ASSERT(top_ <= end_);
    // we already have room if n > 0
    // n is checked first because it's
    // already in a register
    if(BOOST_JSON_UNLIKELY(
        n == 0 && top_ == end_))
        grow();
    detail::unchecked_array ua(
        top_ -= n, n, value_sp_);
    exchange(std::move(ua));
}

void
value_stack::
push_object(std::size_t n)
{
    BOOST_ASSERT(top_ <= end_);
    // we already have room if n > 0
    // n is checked first because it's
    // already in a register
    if(BOOST_JSON_UNLIKELY(
        n == 0 && top_ == end_))
        grow();
    detail::unchecked_object uo(
        top_ -= n * 2, n, value_sp_);
    exchange(std::move(uo));
}

void
value_stack::
push_part(
    string_view s,
    std::size_t n)
{
    const std::size_t str_capacity =
        reinterpret_cast<char const*>(end_) -
        reinterpret_cast<char const*>(top_);
    // number of characters currently on the stack
    const std::size_t current = n - s.size();
    // make sure there is room for
    // pushing one more value and the string part
    if(n + sizeof(value) > str_capacity)
        grow(current, n);
    std::memcpy(reinterpret_cast<char*>(top_ + 1) + 
        current, s.data(), s.size());
    BOOST_ASSERT(
        reinterpret_cast<char*>(
            top_ + 1) + n <=
        reinterpret_cast<char*>(
            end_));
}

void
value_stack::
push_key(
    string_view s,
    std::size_t n)
{
    // fast path when s is the full key
    if(BOOST_JSON_UNLIKELY(top_ == end_))
        grow();
    if(BOOST_JSON_LIKELY(s.size() == n))
        detail::value_access::construct_value(
            top_, s, detail::key_tag(), value_sp_);
    else
         detail::value_access::construct_value(
            top_, release_string(n - s.size()), 
                s, detail::key_tag(), value_sp_);
    ++top_;
}

void
value_stack::
push_string(
    string_view s,
    std::size_t n)
{
    // fast path when s is the full string
    if(BOOST_JSON_UNLIKELY(top_ == end_))
        grow();
    if(BOOST_JSON_LIKELY(s.size() == n))
        detail::value_access::construct_value(
            top_, s, detail::string_tag(), value_sp_);
    else
        detail::value_access::construct_value(
            top_, release_string(n - s.size()), 
                s, detail::string_tag(), value_sp_);
    ++top_;
}

void
value_stack::
push_int64(int64_t i)
{
    return push(i);
}

void
value_stack::
push_uint64(uint64_t u)
{
    return push(u);
}

void
value_stack::
push_double(double d)
{
    return push(d);
}

void
value_stack::
push_bool(bool b)
{
    return push(b);
}

void
value_stack::
push_null()
{
    return push(nullptr);
}

BOOST_JSON_NS_END

#endif
