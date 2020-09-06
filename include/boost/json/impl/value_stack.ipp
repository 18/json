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

std::size_t
value_stack::
size() const noexcept
{
    return top_ - base_;
}

//--------------------------------------

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
grow_one()
{
    //BOOST_ASSERT(chars_ == 0);
    std::size_t const capacity =
        end_ - base_;
    std::size_t new_cap = min_size;
    // VFALCO check overflow here
    while(new_cap < capacity + 1)
        new_cap <<= 1;
    auto const begin =
        reinterpret_cast<value*>(
            stack_sp_->allocate(
                new_cap * sizeof(value)));
    if(base_)
    {
        std::memcpy(
            reinterpret_cast<char*>(begin),
            reinterpret_cast<char*>(base_),
            size() * sizeof(value));
        if(base_ != temp_)
            stack_sp_->deallocate(base_,
                capacity * sizeof(value));
    }
    // book-keeping
    top_ = begin + (top_ - base_);
    end_ = begin + new_cap;
    base_ = begin;
}

// make room for nchars additional characters.
void
value_stack::
grow(
    std::size_t nchars,
    std::size_t total)
{
    // needed capacity in values
    std::size_t const needed =
        size() +
        1 +
        ((total +
            sizeof(value) - 1) /
                sizeof(value));
    std::size_t const capacity =
        end_ - base_;
    BOOST_ASSERT(
        needed > capacity);
    std::size_t new_cap = min_size;
    // VFALCO check overflow here
    while(new_cap < needed)
        new_cap <<= 1;
    auto const begin =
        reinterpret_cast<value*>(
            stack_sp_->allocate(
                new_cap * sizeof(value)));
    if(base_)
    {
        std::size_t amount =
            size() * sizeof(value);
        amount += sizeof(value) + nchars;
        std::memcpy(
            reinterpret_cast<char*>(begin),
            reinterpret_cast<char*>(base_),
            amount);
        if(base_ != temp_)
            stack_sp_->deallocate(base_,
                capacity * sizeof(value));
    }
    // book-keeping
    top_ = begin + (top_ - base_);
    end_ = begin + new_cap;
    base_ = begin;
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
    if(BOOST_JSON_UNLIKELY(top_ >= end_))
        grow_one();
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
    detail::value_access::
        construct_value(
            &jv.v, std::move(u));
    std::memcpy(
        reinterpret_cast<
            char*>(top_),
        &jv.v, sizeof(value));
    ++top_;
}

//----------------------------------------------------------

value_stack::
~value_stack()
{
    clear();
    if(base_ != temp_)
        stack_sp_->deallocate(base_,
            (end_ - base_) * sizeof(value));
}

value_stack::
value_stack(
    storage_ptr sp,
    void* temp_buffer,
    std::size_t temp_size) noexcept
    : stack_sp_(std::move(sp))
    , temp_(temp_buffer)
{
    if(temp_size >= min_size *
        sizeof(value))
    {
        base_ = reinterpret_cast<
            value*>(temp_buffer);
        top_ = base_;
        end_ = base_ +
            temp_size / sizeof(value);
    }
    else
    {
        base_ = nullptr;
        top_ = nullptr;
        end_ = nullptr;
    }
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
    BOOST_ASSERT(size() == 1);

    // give up shared ownership
    value_sp_ = {};

    return pilfer(*--top_);
}

//----------------------------------------------------------

void
value_stack::
push_array(std::size_t n)
{
    // we already have room if n > 0
    // n is checked first because it's
    // already in a register
    if(BOOST_JSON_UNLIKELY(
        n == 0 && top_ >= end_))
        grow_one();
    BOOST_ASSERT(n <= size());
    detail::unchecked_array ua(
        top_ -= n, n, value_sp_);
    exchange(std::move(ua));
}

void
value_stack::
push_object(std::size_t n)
{
    // we already have room if n > 0
    // n is checked first because it's
    // already in a register
    if(BOOST_JSON_UNLIKELY(
        n == 0 && top_ >= end_))
        grow_one();
    detail::unchecked_object uo(
        top_ -= n * 2, n, value_sp_);
    exchange(std::move(uo));
}

void
value_stack::
push_chars(
    string_view s,
    std::size_t n)
{
    std::size_t const bytes_avail =
        reinterpret_cast<
            char const*>(end_) -
        reinterpret_cast<
            char const*>(top_);
    // make sure there is room for
    // pushing one more value without
    // clobbering the string.
    if(sizeof(value) + n > bytes_avail)
        grow(s.size(), n);

    // copy the new piece
    std::memcpy(
        reinterpret_cast<char*>(
            top_ + 1) + (n - s.size()),
        s.data(), s.size());

    // ensure a pushed value cannot
    // clobber the released string.
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
    if(BOOST_JSON_LIKELY(
        s.size() == n))
    {
        char* dest = nullptr;
        push(&dest, n);
        std::memcpy(dest, s.data(), n);
        return;
    }
    BOOST_ASSERT(n > s.size());
    string_view part = 
        release_string(n - s.size());
    char* dest = nullptr;
    push(&dest, part.size() + s.size());
    std::memcpy(dest, part.data(), part.size());
    std::memcpy(dest + part.size(),
        s.data(), s.size());
}

void
value_stack::
push_string(
    string_view s,
    std::size_t n)
{
    if(BOOST_JSON_LIKELY(
        s.size() == n))
    {
        push(s);
        return;
    }
    // VFALCO We could add a special
    // private ctor to string that just
    // creates uninitialized space,
    // to reduce member function calls.
    BOOST_ASSERT(n > s.size());
    string_view part = 
        release_string(n - s.size());
    string& str = 
        push(string_kind).get_string();
    str.reserve(part.size() + s.size());
    std::memcpy(str.data(),
        part.data(), part.size());
    std::memcpy(str.data() + part.size(),
        s.data(), s.size());
    str.grow(part.size() + s.size());
}

void
value_stack::
push_int64(int64_t i)
{
    push(i);
}

void
value_stack::
push_uint64(uint64_t u)
{
    push(u);
}

void
value_stack::
push_double(double d)
{
    push(d);
}

void
value_stack::
push_bool(bool b)
{
    push(b);
}

void
value_stack::
push_null()
{
    push(nullptr);
}

BOOST_JSON_NS_END

#endif
