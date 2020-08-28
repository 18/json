//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_IMPL_VALUE_BUILDER_IPP
#define BOOST_JSON_IMPL_VALUE_BUILDER_IPP

#include <boost/json/value_builder.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace boost {
namespace json {

value_builder::
stack::
~stack()
{
    clear();
    if(begin_ != temp_)
        sp_->deallocate(
            begin_,
            (end_ - begin_) *
                sizeof(value));
}

value_builder::
stack::
stack(
    storage_ptr sp,
    void* temp,
    std::size_t size) noexcept
    : sp_(std::move(sp))
    , temp_(temp)
{
    if(size >= min_size_ *
        sizeof(value))
    {
        begin_ = reinterpret_cast<
            value*>(temp);
        top_ = begin_;
        end_ = begin_ +
            size / sizeof(value);
    }
    else
    {
        begin_ = nullptr;
        top_ = nullptr;
        end_ = nullptr;
    }
}

void
value_builder::
stack::
run_dtors(bool b) noexcept
{
    run_dtors_ = b;
}

std::size_t
value_builder::
stack::
size() const noexcept
{
    return top_ - begin_;
}

bool
value_builder::
stack::
has_part()
{
    return chars_ != 0;
}

//---

void
value_builder::
stack::
prepare()
{
    if(begin_)
        return;
    begin_ = reinterpret_cast<
        value*>(sp_->allocate(
            min_size_ * sizeof(value)));
    top_ = begin_;
    end_ = begin_ + min_size_;
}

// destroy the values but
// not the stack allocation.
void
value_builder::
stack::
clear() noexcept
{
    if(top_ != begin_)
    {
        if(run_dtors_)
            for(auto it = top_;
                it-- != begin_;)
                it->~value();
        top_ = begin_;
    }
    chars_ = 0;
}

void
value_builder::
stack::
maybe_grow()
{
    if(top_ >= end_)
        grow_one();
}

// make room for at least one more value
void
value_builder::
stack::
grow_one()
{
    BOOST_ASSERT(begin_);
    BOOST_ASSERT(chars_ == 0);
    std::size_t const capacity =
        end_ - begin_;
    // VFALCO check overflow here
    std::size_t new_cap = 2 * capacity;
    BOOST_ASSERT(
        new_cap - capacity >= 1);
    auto const begin =
        reinterpret_cast<value*>(
            sp_->allocate(
                new_cap * sizeof(value)));
    std::memcpy(
        reinterpret_cast<char*>(begin),
        reinterpret_cast<char*>(begin_),
        size() * sizeof(value));
    if(begin_ != temp_)
        sp_->deallocate(begin_,
            capacity * sizeof(value));
    // book-keeping
    top_ = begin + (top_ - begin_);
    end_ = begin + new_cap;
    begin_ = begin;
}

// make room for nchars additional characters.
void
value_builder::
stack::
grow(std::size_t nchars)
{
    BOOST_ASSERT(begin_);
    // needed capacity in values
    std::size_t const needed_cap =
        size() +
        1 +
        ((chars_ + nchars +
            sizeof(value) - 1) /
                sizeof(value));
    std::size_t const capacity =
        end_ - begin_;
    BOOST_ASSERT(
        needed_cap > capacity);
    // VFALCO check overflow here
    std::size_t new_cap = capacity;
    while(new_cap < needed_cap)
        new_cap <<= 1;
    auto const begin = reinterpret_cast<
        value*>(sp_->allocate(
            new_cap * sizeof(value)));
    std::size_t amount =
        size() * sizeof(value);
    if(chars_ > 0)
        amount += sizeof(value) + chars_;
    std::memcpy(
        reinterpret_cast<char*>(begin),
        reinterpret_cast<char*>(begin_),
        amount);
    if(begin_ != temp_)
        sp_->deallocate(begin_,
            capacity * sizeof(value));
    // book-keeping
    top_ = begin + (top_ - begin_);
    end_ = begin + new_cap;
    begin_ = begin;
}

void
value_builder::
stack::
append(string_view s)
{
    std::size_t const bytes_avail =
        reinterpret_cast<
            char const*>(end_) -
        reinterpret_cast<
            char const*>(top_);
    // make sure there is room for
    // pushing one more value without
    // clobbering the string.
    if(sizeof(value) + chars_ +
            s.size() > bytes_avail)
        grow(s.size());

    // copy the new piece
    std::memcpy(
        reinterpret_cast<char*>(
            top_ + 1) + chars_,
        s.data(), s.size());
    chars_ += s.size();

    // ensure a pushed value cannot
    // clobber the released string.
    BOOST_ASSERT(
        reinterpret_cast<char*>(
            top_ + 1) + chars_ <=
        reinterpret_cast<char*>(
            end_));
}

template<class... Args>
value&
value_builder::
stack::
push(Args&&... args)
{
    BOOST_ASSERT(chars_ == 0);
    if(top_ >= end_)
        grow_one();
    value& jv = detail::value_access::
        construct_value(top_,
            std::forward<Args>(args)...);
    ++top_;
    return jv;
}

template<class... Args>
void
value_builder::
stack::
push_value(Args&&... args)
{
    BOOST_ASSERT(chars_ == 0);
    union U
    {
        value v;

        U() { }
        ~U() { }
    } u;
    detail::value_access::
        construct_value(&u.v,
            std::forward<Args>(args)...);
    std::memcpy(top_, &u.v, sizeof(value));
    ++top_;
}


//---

string_view
value_builder::
stack::
release_string() noexcept
{
    // ensure a pushed value cannot
    // clobber the released string.
    BOOST_ASSERT(
        reinterpret_cast<char*>(
            top_ + 1) + chars_ <=
        reinterpret_cast<char*>(
            end_));
    auto const n = chars_;
    chars_ = 0;
    return { reinterpret_cast<
        char const*>(top_ + 1), n };
}

// transfer ownership of the top n
// elements of the stack to the caller
value*
value_builder::
stack::
release(std::size_t n) noexcept
{
    BOOST_ASSERT(n <= size());
    BOOST_ASSERT(chars_ == 0);
    top_ -= n;
    return top_;
}

//----------------------------------------------------------

value_builder::
~value_builder()
{
}

value_builder::
value_builder(
    storage_ptr sp,
    void* temp_buffer,
    std::size_t temp_size) noexcept
    : st_(
        std::move(sp),
        temp_buffer,
        temp_size)
{

}

void
value_builder::
reserve(std::size_t n)
{
#ifndef BOOST_NO_EXCEPTIONS
    try
    {
#endif
        // VFALCO TODO
        // st_.reserve(n);
        (void)n;
#ifndef BOOST_NO_EXCEPTIONS
    }
    catch(std::bad_alloc const&)
    {
        // silence the exception, per contract
    }
#endif
}

void
value_builder::
reset(storage_ptr sp) noexcept
{
    clear();
    sp_ = std::move(sp);
    st_.prepare();

    // `stack` needs this
    // to clean up correctly
    st_.run_dtors(
        ! sp_.is_not_counted_and_deallocate_is_null());
}

value
value_builder::
release()
{
    // give up shared ownership
    sp_ = {};

    if(st_.size() == 1)
        return pilfer(*st_.release(1));

    // This means the caller did not
    // cause a single top level element
    // to be produced.
    throw std::logic_error("no value");
}

void
value_builder::
clear() noexcept
{
    // give up shared ownership
    sp_ = {};

    st_.clear();
    top_ = 0;
}

//----------------------------------------------------------


void
value_builder::
push_array(std::size_t n)
{
    // we already have room if n > 0
    if(BOOST_JSON_UNLIKELY(n == 0))
        st_.maybe_grow();
    detail::unchecked_array ua(
        st_.release(n), n, sp_);
    st_.push_value(std::move(ua));
}

void
value_builder::
push_object(std::size_t n)
{
    // we already have room if n > 0
    if(BOOST_JSON_UNLIKELY(n == 0))
        st_.maybe_grow();
    detail::unchecked_object uo(
        st_.release(n * 2), n, sp_);
    st_.push_value(std::move(uo));
}

void
value_builder::
insert_key_part(
    string_view s)
{
    st_.append(s);
}

void
value_builder::
insert_key(
    string_view s)
{
    if(! st_.has_part())
    {
        // fast path
        char* dest = nullptr;
        st_.push(&dest, s.size(), sp_);
        std::memcpy(
            dest, s.data(), s.size());
        return;
    }

    auto part = st_.release_string();
    char* dest = nullptr;
    st_.push(&dest,
        part.size() + s.size(), sp_);
    std::memcpy(dest,
        part.data(), part.size());
    std::memcpy(dest + part.size(),
        s.data(), s.size());
}

void
value_builder::
insert_string_part(
    string_view s)
{
    st_.append(s);
}

void
value_builder::
insert_string(
    string_view s)
{
    if(! st_.has_part())
    {
        // fast path
        st_.push(s, sp_);
        return;
    }

    // VFALCO We could add a special
    // private ctor to string that just
    // creates uninitialized space,
    // to reduce member function calls.
    auto part = st_.release_string();
    auto& str = st_.push(
        string_kind, sp_).get_string();
    str.reserve(
        part.size() + s.size());
    std::memcpy(
        str.data(),
        part.data(), part.size());
    std::memcpy(
        str.data() + part.size(),
        s.data(), s.size());
    str.grow(part.size() + s.size());
}

void
value_builder::
insert_int64(
    int64_t i)
{
    st_.push(i, sp_);
}

void
value_builder::
insert_uint64(
    uint64_t u)
{
    st_.push(u, sp_);
}

void
value_builder::
insert_double(
    double d)
{
    st_.push(d, sp_);
}

void
value_builder::
insert_bool(
    bool b)
{
    st_.push(b, sp_);
}

void
value_builder::
insert_null()
{
    st_.push(nullptr, sp_);
}

} // json
} // boost

#endif
