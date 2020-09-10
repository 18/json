//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_OBJECT_IMPL_IPP
#define BOOST_JSON_DETAIL_IMPL_OBJECT_IMPL_IPP

#include <boost/json/detail/except.hpp>

BOOST_JSON_NS_BEGIN
namespace detail {

void
object_impl::
do_destroy(storage_ptr const& sp) noexcept
{
    if(tab_)
    {
        detail::destroy(begin(), size());
        sp->deallocate(tab_,
            sizeof(table) +
            capacity() * sizeof(value_type) +
            buckets() * sizeof(index_t));
    }
}

inline
object_impl::
object_impl(
    std::size_t capacity,
    std::size_t prime_index,
    std::uintptr_t salt,
    storage_ptr const& sp)
{
    if(capacity > max_size())
        throw_length_error(
            "capacity > max_size()",
            BOOST_CURRENT_LOCATION);
    tab_ = ::new(sp->allocate(
        allocation_size(capacity))) 
        table{0, capacity, prime_index, salt};
    // capacity == buckets()
    std::memset(bucket_begin(), 0xff, // null_index
        capacity * sizeof(index_t));
}

object_impl::
object_impl(table* tab) noexcept
    : tab_(tab)
{
}

auto
object_impl::
find_slot(
    table* tab,
    string_view key,
    index_t& dupe) noexcept ->
        index_t*
{
    const auto hash = detail::digest(
        key.data(), key.size(), 
            reinterpret_cast<std::uintptr_t>(tab));
    index_t* head = reinterpret_cast<index_t*>(
        tab->data() + tab->capacity) + 
            bucket_index(hash, tab->prime_index);
    index_t i = *head;
    while(i != null_index &&
        tab->data()[i].key() != key)
        i = next(tab->data()[i]);
    if(BOOST_JSON_LIKELY(i == null_index))
        return head;
    // duplicate
    dupe = i;
    return nullptr;
}

object_impl::
object_impl(object_impl&& other) noexcept
    : tab_(detail::exchange(
        other.tab_, nullptr))
{
}

void
object_impl::
clear() noexcept
{
    if(! tab_)
        return;
    detail::destroy(begin(), size());
    std::memset(bucket_begin(), 0xff, // null_index
        buckets() * sizeof(index_t));
    tab_->size = 0;
}

// does not check for dupes
void
object_impl::
rebuild() noexcept
{
    auto const end = this->end();
    for(auto p = begin();
        p != end; ++p)
    {
        auto& head = bucket(p->key());
        next(*p) = head;
        head = index_of(*p);
    }
}

void
object_impl::
swap(object_impl& rhs) noexcept
{
    auto tmp = tab_;
    tab_ = rhs.tab_;
    rhs.tab_ = tmp;
}

//----------------------------------------------------------

void
destroy(
    key_value_pair* p,
    std::size_t n) noexcept
{
    // VFALCO We check again here even
    // though some callers already check it.
    if(n == 0 || ! p)
        return;
    auto const& sp = p->value().storage();
    if(sp.is_not_counted_and_deallocate_is_trivial())
        return;
    p += n;
    while(n--)
        (*--p).~key_value_pair();
}

} // detail
BOOST_JSON_NS_END

#endif
