//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_ARRAY_IMPL_HPP
#define BOOST_JSON_DETAIL_IMPL_ARRAY_IMPL_HPP

BOOST_JSON_NS_BEGIN
namespace detail {

constexpr
std::size_t
array_impl::
max_size() noexcept
{
    // max_size depends on the address model
    using min = std::integral_constant<std::size_t,
        (std::size_t(-1) - sizeof(table)) / sizeof(value)>;
    return min::value < BOOST_JSON_MAX_STRUCTURED_SIZE ?
        min::value : BOOST_JSON_MAX_STRUCTURED_SIZE;
}

constexpr
std::size_t
array_impl::
allocation_size(
    std::size_t capacity) noexcept
{
    // make sure to update max_size
    // if this is changed
    return (sizeof(table) + capacity * sizeof(value) +
        sizeof(table) + 1) / sizeof(table) * sizeof(table);
}

auto
array_impl::
index_of(value const* pos) const noexcept ->
    std::size_t
{
    return static_cast<
        std::size_t>(pos - data());
}

} // detail
BOOST_JSON_NS_END

#endif
