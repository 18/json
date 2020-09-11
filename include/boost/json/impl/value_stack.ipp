//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
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

template<class... Args>
void
value_stack::
push(Args&&... args)
{
    BOOST_ASSERT(top_ <= end_);
    if(BOOST_JSON_UNLIKELY(top_ == end_))
        grow();
    access::construct_value(
        top_, std::forward<Args>(args)..., value_sp_);
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
    using impl = detail::array_impl;
    impl::table* tab;
    if(BOOST_JSON_UNLIKELY(n == 0))
    {
        if(BOOST_JSON_UNLIKELY(top_ == end_))
            grow();
        tab = nullptr;
    }
    else
    {
        // allocate up-front so we don't have to
        // deal with memory leaks. if this throws,
        // value_stack will clean up for us.
        tab = ::new(value_sp_->allocate(
            impl::allocation_size(n))) impl::table{
                std::uint32_t(n), std::uint32_t(n)};
        std::memcpy(tab->data(), reinterpret_cast<
            const char*>(top_ -= n), n * sizeof(value));
    }
    access::construct_value(
        top_++, tab, value_sp_);
}

void
value_stack::
push_object(std::size_t n)
{
    BOOST_ASSERT(top_ <= end_);
    using impl = detail::object_impl;
    impl::table* tab;
    if(BOOST_JSON_UNLIKELY(n == 0))
    {
        if(BOOST_JSON_UNLIKELY(top_ == end_))
            grow();
        tab = nullptr;
    }
    else
    {
        const std::size_t* prime = 
            impl::bucket_sizes();
        while(*prime < n)
            ++prime;
        const std::size_t cap = *prime;
        // allocate up-front so we don't have to
        // deal with memory leaks. if this throws,
        // value_stack will clean up for us.
        tab = ::new(value_sp_->allocate(
            impl::allocation_size(cap))) impl::table{
                std::uint32_t(n), std::uint32_t(cap),
                std::size_t(prime - impl::bucket_sizes())};
        tab->salt = reinterpret_cast<
            std::uintptr_t>(tab);
        std::memset(tab->data() + cap, 0xff, 
            cap * sizeof(access::index_t));
        value* end = top_;
        value* src = top_ -= n * 2;
        key_value_pair* dst = tab->data();
        access::index_t next;
        while(src != end)
        {
            string_view key = access::release_key(src[0]);
            if(auto idx = impl::find_slot(tab, key, next))
            {
                access::construct_key_value_pair(
                    dst, key, pilfer(src[1]));
                access::next(*dst) = *idx;
                *idx = static_cast<std::uint32_t>(
                    dst - tab->data());
                ++dst;
            }
            else
            {
                // handle duplicate
                key_value_pair* dupe = tab->data() + next;
                next = access::next(*dupe);
                dupe->~key_value_pair();
                access::next(access::
                    construct_key_value_pair(dupe, key, 
                        pilfer(src[1]))) = next;
                --tab->size;
            }
            src += 2;
        }
    }
    access::construct_value(
        top_++, tab, value_sp_);
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
    char* ptr = static_cast<char*>(
        value_sp_->allocate(n + 1));
    char* dest = ptr;
    ptr[n] = 0;
    if(BOOST_JSON_UNLIKELY(s.size() != n))
    {
        const std::size_t saved = 
            n - s.size();
        std::memcpy(dest, top_ + 1, saved);
        dest += saved;
    }
    std::memcpy(dest, s.data(), s.size());
    access::construct_value(top_, ptr, n,
        detail::key_tag(), value_sp_);
    ++top_;
}

void
value_stack::
push_string(
    string_view s,
    std::size_t n)
{
    if(BOOST_JSON_UNLIKELY(top_ == end_))
        grow();
    using impl = detail::string_impl;
    if(n > impl::sbo_chars_)
    {
        impl::table* ptr = ::new(value_sp_->allocate(
            sizeof(impl::table) + n + 1)) impl::table{
                std::uint32_t(n), std::uint32_t(n)};
        char* dest = ptr->data();
        dest[n] = 0;
        if(BOOST_JSON_UNLIKELY(s.size() != n))
        {
            const std::size_t saved = 
                n - s.size();
            std::memcpy(dest, top_ + 1, saved);
            dest += saved;
        }
        std::memcpy(dest, s.data(), s.size());
        access::construct_value(top_, ptr, value_sp_);
    }
    else
    {
        value& str = access::construct_value(top_,
            string_kind, value_sp_);
        char* dest = access::get_small_buffer(str);
        dest[impl::sbo_chars_] = 
            impl::sbo_chars_ - n;
        dest[n] = 0;
        if(BOOST_JSON_UNLIKELY(s.size() != n))
        {
            const std::size_t saved = 
                n - s.size();
            std::memcpy(dest, top_ + 1, saved);
            dest += saved;
        }
        std::memcpy(dest, s.data(), s.size());
    }
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
