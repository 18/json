//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_ARRAY_IMPL_IPP
#define BOOST_JSON_DETAIL_IMPL_ARRAY_IMPL_IPP

#include <boost/json/detail/array_impl.hpp>
#include <boost/json/detail/except.hpp>

BOOST_JSON_NS_BEGIN
namespace detail {

array_impl::
array_impl(
    std::size_t capacity,
    storage_ptr const& sp)
{
    if(capacity > max_size())
        detail::throw_length_error(
            "capacity > max_size()",
            BOOST_CURRENT_LOCATION);
    if(capacity > 0)
    {
        tab_ = ::new(sp->allocate(
            allocation_size(capacity))) table;
        tab_->capacity = static_cast<
            std::uint32_t>(capacity);
        tab_->size = 0;
    }
}

array_impl::
array_impl(
    unchecked_array&& ua,
    storage_ptr const& sp)
{
    const std::size_t n = ua.size();
    if(BOOST_JSON_LIKELY(n > 0))
    {
        tab_ = ::new(sp->allocate(
            allocation_size(n))) table{
            static_cast<std::uint32_t>(n),
            static_cast<std::uint32_t>(n)};
        ua.relocate(reinterpret_cast<
            value*>(tab_ + 1));
    }
}

array_impl::
array_impl(array_impl&& other) noexcept
    : tab_(detail::exchange(
        other.tab_, nullptr))
{
}

auto
array_impl::
operator=(
    array_impl&& other) noexcept ->
        array_impl&
{
    ::new(this) array_impl(
        std::move(other));
    return *this;
}

void
array_impl::
swap(array_impl& rhs) noexcept
{
    auto tmp = tab_;
    tab_ = rhs.tab_;
    rhs.tab_ = tmp;
}

void
array_impl::
destroy_impl(
    storage_ptr const& sp) noexcept
{
    auto it = data() + tab_->size;
    while(it != data())
        (*--it).~value();
    sp->deallocate(tab_,
        allocation_size(capacity()));
}

void
array_impl::
destroy(
    storage_ptr const& sp) noexcept
{
    if(! tab_ ||
        sp.is_not_counted_and_deallocate_is_trivial())
        return;
    destroy_impl(sp);
}

} // detail
BOOST_JSON_NS_END

#endif
