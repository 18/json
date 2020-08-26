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

//----------------------------------------------------------

value_builder::
stack::
~stack()
{
    clear();
    if(begin_)
        sp_->deallocate(begin_,
            sizeof(value) * capacity_);
}

value_builder::
stack::
stack(storage_ptr sp) noexcept
    : sp_(std::move(sp))
{
}

std::size_t
value_builder::
stack::
size() const noexcept
{
    return size_;
}

// destroy the values but
// not the stack allocation.
void
value_builder::
stack::
clear() noexcept
{
    if(size_ != 0)
    {
        // the storage used by the
        // values is different from sp_
        auto const sp = begin_->storage();
        if(! sp.is_not_counted_and_deallocate_is_null())
        {
            for(auto it = begin_,
                end = begin_ + size_;
                it != end; ++it)
                it->~value();
        }
        size_ = 0;
    }
}
void
value_builder::
stack::
grow_one()
{
    std::size_t new_cap;
    if(capacity_ < 32)
        new_cap = 32;
    else
        new_cap = capacity_ * 2;
    while(new_cap < size_ + 1)
        new_cap = new_cap * 2;
    auto p = sp_->allocate(
        sizeof(value) * new_cap);
    std::memcpy(p, begin_,
        sizeof(value) * size_);
    sp_->deallocate(begin_,
        sizeof(value) * capacity_);
    capacity_ = new_cap;
    begin_ = reinterpret_cast<
        value*>(p);
}

void
value_builder::
stack::
save(std::size_t n)
{
    if(size_ >= capacity_)
        grow_one();
    // use default storage here to
    // avoid needless refcounting
    ::new(begin_ + size_) value(n);
    ++size_;
}

void
value_builder::
stack::
restore(std::size_t* n) noexcept
{
    BOOST_ASSERT(size_ > 0);
    auto p = begin_ + (--size_);
    BOOST_ASSERT(p->is_uint64());
    *n = p->get_uint64();
    //p->~value(); // not needed
}

// transfer ownership of the top n
// elements of the stack to the caller
value*
value_builder::
stack::
release(std::size_t n)
{
    BOOST_ASSERT(n <= size_);
    size_ -= n;
    return begin_ + size_;
}

template<class... Args>
value&
value_builder::
stack::
push(Args&&... args)
{
    if(size_ >= capacity_)
        grow_one();
    value& jv = detail::value_access::
        construct_value(
            begin_ + size_,
            std::forward<Args>(args)...);
    ++size_;
    return jv;
}

//----------------------------------------------------------

enum class value_builder::state : char
{
    reset,      // reset needed
    begin,      // we have a storage_ptr

    // These states indicate what is
    // currently at top of the stack.

    top,        // top value, constructed if lev_.count==1
    arr,        // empty array value
    obj,        // empty object value
    key,        // complete key
};

value_builder::
~value_builder()
{
}

value_builder::
value_builder(
    storage_ptr sp) noexcept
    : st_(std::move(sp))
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
        // squelch the exception, per contract
    }
#endif
}

void
value_builder::
reset(storage_ptr sp) noexcept
{
    clear();
    sp_ = std::move(sp);
}

value
value_builder::
release()
{
    // give up shared ownership
    sp_ = {};

    if(st_.size() == 1)
    {
        auto p = st_.release(1);
        return pilfer(*p);
    }

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
    temp_.clear();
    top_ = 0;
}

//----------------------------------------------------------

void
value_builder::
begin_array()
{
    st_.save(top_);
    top_ = st_.size();
}

void
value_builder::
end_array()
{
    auto const n =
        st_.size() - top_;
    detail::unchecked_array ua(
        st_.release(n), n, sp_);
    st_.restore(&top_);
    st_.push(std::move(ua));
}

void
value_builder::
begin_object()
{
    st_.push(top_);
    top_ = st_.size();
}

void
value_builder::
end_object()
{
    // must be even
    BOOST_ASSERT(
        ((st_.size() - top_) & 1) == 0);

    auto const n =
        st_.size() - top_;
    detail::unchecked_object uo(
        st_.release(n), n/2, sp_);
    st_.restore(&top_);
    st_.push(std::move(uo));
}

void
value_builder::
insert_key_part(
    string_view s)
{
    temp_.append(s.data(), s.size());
}

void
value_builder::
insert_key(
    string_view s)
{
    if(temp_.empty())
    {
        // fast path
        char* dest;
        st_.push(&dest, s.size(), sp_);
        std::memcpy(
            dest, s.data(), s.size());
        return;
    }

    char* dest;
    st_.push(&dest,
        temp_.size() + s.size(), sp_);
    std::memcpy(dest,
        temp_.data(), temp_.size());
    std::memcpy(dest + temp_.size(),
        s.data(), s.size());
    temp_.clear();
}

void
value_builder::
insert_string_part(
    string_view s)
{
    temp_.append(s.data(), s.size());
}

void
value_builder::
insert_string(
    string_view s)
{
    if(temp_.empty())
    {
        // fast path
        st_.push(s, sp_);
        return;
    }

    auto& str = st_.push(
        string_kind, sp_).get_string();
    str.reserve(
        temp_.size() + s.size());
    std::memcpy(
        str.data(),
        temp_.data(), temp_.size());
    std::memcpy(
        str.data() + temp_.size(),
        s.data(), s.size());
    str.grow(temp_.size() + s.size());
    temp_.clear();
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
