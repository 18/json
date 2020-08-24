//
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_UNCHECKED_BUILDER_HPP
#define BOOST_JSON_DETAIL_UNCHECKED_BUILDER_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/error.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/value.hpp>
#include <boost/json/detail/raw_stack.hpp>
#include <stddef.h>

namespace boost {
namespace json {
namespace detail {

class unchecked_builder
{
    storage_ptr sp_;
    // stores values and key_value_pairs
    detail::raw_stack value_;
    // stores trivially destructable data
    detail::raw_stack state_;

public:
    inline
    ~unchecked_builder();

    inline
    explicit 
    unchecked_builder(storage_ptr sp = {}) noexcept;

    inline
    void
    reserve(std::size_t n);

    inline
    void
    reset(storage_ptr sp = {}) noexcept;

    inline
    value
    release();

    inline
    void
    clear() noexcept;

    inline
    void
    begin_array();

    inline
    void
    end_array(std::size_t n);

    inline
    void
    begin_object();

    inline
    void
    end_object(std::size_t n);

    inline
    void
    insert_key_part(
        string_view s);

    inline
    void
    insert_key(
        string_view s,
        std::size_t n);

    inline
    void
    insert_string_part(
        string_view s);

    inline
    void
    insert_string(
        string_view s,
        std::size_t n);

    inline
    void
    insert_int64(
        int64_t i);

    inline
    void
    insert_uint64(
        uint64_t u);

    inline
    void
    insert_double(
        double d);

    inline
    void
    insert_bool(
        bool b);

    inline
    void
    insert_null();

private:
    inline
    void
    destroy() noexcept;

    template<class T>
    void
    push(T const& t);

    inline
    void
    push_chars(string_view s);

    template<class... Args>
    void
    emplace(
        Args&&... args);

    template<class T>
    void
    pop_value(T& t);

    template<class T>
    void
    pop_state(T& t);

    inline
    object
    pop_object(std::size_t n);

    inline
    detail::unchecked_array
    pop_array(std::size_t n) noexcept;

    inline
    string_view
    pop_key() noexcept;

    inline
    string_view
    pop_chars(
        std::size_t n) noexcept;
};

unchecked_builder::
~unchecked_builder()
{
    destroy();
}

unchecked_builder::
unchecked_builder(
    storage_ptr sp) noexcept
    : value_(sp)
    , state_(sp)
{
}

void
unchecked_builder::
reserve(std::size_t n)
{
#ifndef BOOST_NO_EXCEPTIONS
    try
    {
#endif
        value_.reserve(n);
        state_.reserve(n);
#ifndef BOOST_NO_EXCEPTIONS
    }
    catch(std::bad_alloc const&)
    {
    }
#endif
}

void
unchecked_builder::
reset(storage_ptr sp) noexcept
{
    clear();
    sp_ = std::move(sp);

    // reset() must be called before
    // building every new top level value.
    // The top level `value` is kept
    // inside a notional 1-element array.
}

value
unchecked_builder::
release()
{
    value_.subtract(sizeof(value));

    union U
    {
        value v;
        U(){}
        ~U(){}
    };
    U u;
    auto top = reinterpret_cast<value*>(
        value_.current());
    std::memcpy(&u, top, sizeof(value));

    // give up the resource in case
    // it uses shared ownership.
    sp_ = {};

    return pilfer(u.v);
}

void
unchecked_builder::
clear() noexcept
{
    destroy();

    // give up the resource in case
    // it uses shared ownership.
    sp_ = {};
}

//----------------------------------------------------------

void
unchecked_builder::
begin_array()
{
    value_.add(sizeof(value));
    ::new(value_.behind(sizeof(value))) value;
}

void
unchecked_builder::
end_array(std::size_t n)
{
    BOOST_ASSERT(! value_.empty());
    
    auto a = pop_array(n);
    value_.subtract(sizeof(value));
    emplace(std::move(a));
}

void
unchecked_builder::
begin_object()
{
    value_.add(sizeof(value));
    ::new(value_.behind(sizeof(value))) value;
}

void
unchecked_builder::
end_object(std::size_t n)
{
    BOOST_ASSERT(! value_.empty());
    auto o = pop_object(n);
    value_.subtract(sizeof(value));
    emplace(std::move(o));
}

void
unchecked_builder::
insert_key_part(
    string_view s)
{
    push_chars(s);
}

void
unchecked_builder::
insert_key(
    string_view s,
    std::size_t n)
{
    push_chars(s);
    push(n);
}

void
unchecked_builder::
insert_string_part(
    string_view s)
{
    push_chars(s);
}

void
unchecked_builder::
insert_string(
    string_view s,
    std::size_t n)
{
    if(n == s.size())
        return emplace(s, sp_);

    string str(sp_);
    auto const sv =
        pop_chars(n - s.size());
    str.reserve(
        sv.size() + s.size());
    std::memcpy(
        str.data(),
        sv.data(), sv.size());
    std::memcpy(
        str.data() + sv.size(),
        s.data(), s.size());
    str.grow(sv.size() + s.size());
    emplace(std::move(str), sp_);
}

void
unchecked_builder::
insert_int64(
    int64_t i)
{
    emplace(i, sp_);
}

void
unchecked_builder::
insert_uint64(
    uint64_t u)
{
    emplace(u, sp_);
}

void
unchecked_builder::
insert_double(
    double d)
{
    emplace(d, sp_);
}

void
unchecked_builder::
insert_bool(
    bool b)
{
    emplace(b, sp_);
}

void
unchecked_builder::
insert_null()
{
    emplace(nullptr, sp_);
}

//----------------------------------------------------------

void
unchecked_builder::
destroy() noexcept
{
    while(! value_.empty())
        reinterpret_cast<value*>(
            value_.pop(sizeof(value)))->~value();
    state_.clear();
}

template<class T>
void
unchecked_builder::
push(T const& t)
{
    std::memcpy(state_.push(
        sizeof(T)), &t, sizeof(T));
}

void
unchecked_builder::
push_chars(string_view s)
{
    std::memcpy(state_.push(s.size()),
        s.data(), s.size());
}

template<class... Args>
void
unchecked_builder::
emplace(Args&&... args)
{
    // prevent splits from exceptions
    value_.prepare(sizeof(value));
    ::new(static_cast<void*>(value_.current())) value(
        std::forward<Args>(args)...);
    value_.add_unchecked(sizeof(value));
}

template<class T>
void
unchecked_builder::
pop_value(T& t)
{
    std::memcpy(&t,
        value_.pop(sizeof(T)),
        sizeof(T));
}

template<class T>
void
unchecked_builder::
pop_state(T& t)
{
    std::memcpy(&t,
        state_.pop(sizeof(T)),
        sizeof(T));
}

object
unchecked_builder::
pop_object(std::size_t n)
{
    BOOST_ASSERT(! value_.empty());
    object obj(object::unchecked_tag{}, sp_);
    if(n == 0)
        return obj;
    auto src = reinterpret_cast<value*>(
        value_.current());
    auto dst = obj.prepare(n) + n;
    for(std::size_t i = n; i--;)
    {
        ::new(--dst) object::value_type(
            pop_key(), std::move(*--src), sp_);
        obj.grow();
    }
    obj.build();
    value_.pop(n * sizeof(value));
    return std::move(obj);
}

detail::unchecked_array
unchecked_builder::
pop_array(std::size_t n) noexcept
{
    BOOST_ASSERT(! value_.empty());
    if(n == 0)
        return { nullptr, 0, sp_ };
    auto const bytes =
        n * sizeof(value);
    return { reinterpret_cast<value*>(
        value_.pop(bytes)), n, sp_ };
}

string_view
unchecked_builder::
pop_key() noexcept
{
    std::size_t n;
    pop_state(n);
    return { state_.pop(n), n };
}

string_view
unchecked_builder::
pop_chars(std::size_t n) noexcept
{
    return { state_.pop(n), n };
}

} // detail
} // json
} // boost

#ifdef BOOST_JSON_HEADER_ONLY
#include <boost/json/impl/unchecked_builder.ipp>
#endif

#endif
