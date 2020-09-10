//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_IMPL_VALUE_HPP
#define BOOST_JSON_IMPL_VALUE_HPP

#include <boost/json/detail/except.hpp>
#include <cstring>
#include <limits>
#include <type_traits>

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

struct value::undo
{
    union
    {
        value saved;
    };
    value* self;

    explicit
    undo(value* self_) noexcept
        : self(self_)
    {
        relocate(&saved, *self_);
    }

    void
    commit() noexcept
    {
        saved.~value();
        self = nullptr;
    }

    ~undo()
    {
        if(self)
            relocate(self, saved);
    }
};

//----------------------------------------------------------

value::
value(
    object_impl::table* tab,
    const storage_ptr& sp) noexcept
    : obj_(tab, sp)
{
}

value::
value(
    array_impl::table* tab,
    const storage_ptr& sp) noexcept
    : arr_(tab, sp)
{
}

value::
value(
    string_view s, 
    detail::string_tag,
    const storage_ptr& sp)
    : str_(s, detail::string_tag(), sp)
{
}

value::
value(
    string_view s, 
    detail::key_tag,
    const storage_ptr& sp)
    : str_(s, detail::key_tag(), sp)
{
}

value::
value(
    string_view s1, 
    string_view s2, 
    detail::string_tag,
    const storage_ptr& sp)
    : str_(s1, s2, detail::string_tag(), sp)
{
}

value::
value(
    string_view s1, 
    string_view s2,
    detail::key_tag,
    const storage_ptr& sp)
    : str_(s1, s2, detail::key_tag(), sp)
{
}

template<class T, class>
value&
value::
operator=(T&& t)
{
    undo u(this);
    ::new(this) value(
        std::forward<T>(t),
        u.saved.storage());
    u.commit();
    return *this;
}

void
value::
relocate(
    value* dest,
    value const& src) noexcept
{
    std::memcpy(
        reinterpret_cast<
            void*>(dest),
        &src,
        sizeof(src));
}

//----------------------------------------------------------

inline
std::uint32_t
key_value_pair::
key_size(std::size_t n)
{
    if(n > string::max_size())
        detail::throw_length_error(
            "key too large",
            BOOST_CURRENT_LOCATION);
    return static_cast<
        std::uint32_t>(n);
}

template<class... Args>
key_value_pair::
key_value_pair(
    string_view key,
    Args&&... args)
    : value_(std::forward<Args>(args)...)
    , key_(
        [&]
        {
            if(key.size() > string::max_size())
                detail::throw_length_error(
                    "key too large",
                    BOOST_CURRENT_LOCATION);
            auto s = reinterpret_cast<
                char*>(value_.storage()->
                    allocate(key.size() + 1));
            std::memcpy(s, key.data(), key.size());
            s[key.size()] = 0;
            return s;
        }())
    , len_(key_size(key.size()))
{
}

//----------------------------------------------------------

BOOST_JSON_NS_END

#endif
