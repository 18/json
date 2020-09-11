//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_STRING_IMPL_HPP
#define BOOST_JSON_DETAIL_STRING_IMPL_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/detail/value.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/storage_ptr.hpp>
#include <algorithm>
#include <iterator>

BOOST_JSON_NS_BEGIN

class value;

namespace detail {

class string_impl
{
    friend struct value_access;
public:
    struct table
    {
        std::uint32_t size;
        std::uint32_t capacity;

        char*
        data() noexcept
        {
            return reinterpret_cast<
                char*>(this + 1);
        }
    };

    static
    constexpr
    std::size_t 
    sbo_chars_ =
        sizeof(table*) * 2 -
        sizeof(kind) - 1;

#if BOOST_JSON_ARCH == 64
    BOOST_STATIC_ASSERT(sbo_chars_ == 14);
#elif BOOST_JSON_ARCH == 32
    BOOST_STATIC_ASSERT(sbo_chars_ == 6);
#else
# error Unknown architecture
#endif

    static
    constexpr
    kind
    short_string_ =
        static_cast<kind>(
            ((unsigned char)
            kind::string) | 0x80);

    static
    constexpr
    kind
    key_string_ =
        static_cast<kind>(
            ((unsigned char)
            kind::string) | 0x40);

    struct sbo
    {
        kind k; // must come first
        char buf[sbo_chars_ + 1];
    };

    struct pointer
    {
        kind k;
        char pad[
            sizeof(table*) -
            sizeof(kind)];
        table* t;
    };

    struct key
    {
        kind k;
        std::uint32_t n;
        char* s;
    };

private:
    union
    {
        sbo s_;
        pointer p_;
        key k_;
    };

public:
    static
    constexpr
    std::size_t
    max_size() noexcept
    {
        // max_size depends on the address model
        using min = std::integral_constant<std::size_t,
            std::size_t(-1) - sizeof(table)>;
        return min::value < BOOST_JSON_MAX_STRING_SIZE ?
            min::value : BOOST_JSON_MAX_STRING_SIZE;
    }

    BOOST_JSON_DECL
    string_impl() noexcept;

    BOOST_JSON_DECL
    string_impl(table* tab) noexcept;

    BOOST_JSON_DECL
    string_impl(
        char* p,
        std::size_t n,
        key_tag) noexcept;

    BOOST_JSON_DECL
    string_impl(
        std::size_t new_size,
        storage_ptr const& sp);

    template<class InputIt>
    string_impl(
        InputIt first,
        InputIt last,
        storage_ptr const& sp,
        std::random_access_iterator_tag)
        : string_impl(last - first, sp)
    {
        std::copy(first, last, data());
    }

    template<class InputIt>
    string_impl(
        InputIt first,
        InputIt last,
        storage_ptr const& sp,
        std::input_iterator_tag)
        : string_impl(0, sp)
    {
        struct undo
        {
            string_impl* s;
            storage_ptr const& sp;

            ~undo()
            {
                if(s)
                    s->destroy(sp);
            }
        };

        undo u{this, sp};
        auto dest = data();
        while(first != last)
        {
            if(size() < capacity())
                size(size() + 1);
            else
                dest = append(1, sp);
            *dest++ = *first++;
        }
        term(size());
        u.s = nullptr;
    }

    std::size_t
    size() const noexcept
    {
        return s_.k == kind::string ?
            p_.t->size :
            sbo_chars_ -
                s_.buf[sbo_chars_];
    }

    std::size_t
    capacity() const noexcept
    {
        return s_.k == kind::string ?
            p_.t->capacity :
            sbo_chars_;
    }

    void
    size(std::size_t n)
    {
        if(s_.k == kind::string)
            p_.t->size = static_cast<
                std::uint32_t>(n);
        else
            s_.buf[sbo_chars_] =
                static_cast<char>(
                    sbo_chars_ - n);
    }

    BOOST_JSON_DECL
    static
    std::uint32_t
    growth(
        std::size_t new_size,
        std::size_t capacity);

    string_view
    release_key() noexcept
    {
        BOOST_ASSERT(
            k_.k == key_string_);
        // prevent deallocate
        k_.k = short_string_;
        return {k_.s, k_.n};
    }

    void
    destroy(
        storage_ptr const& sp) noexcept
    {
        if(s_.k == kind::string)
        {
            sp->deallocate(p_.t,
                sizeof(table) +
                    p_.t->capacity + 1,
                alignof(table));
        }
        else if(s_.k != key_string_)
        {
            // do nothing
        }
        else
        {
            BOOST_ASSERT(
                s_.k == key_string_);
            // VFALCO unfortunately the key string
            // kind increases the cost of the destructor.
            // This function should be skipped when using
            // monotonic_resource.
            sp->deallocate(k_.s, k_.n + 1);
        }
    }

    BOOST_JSON_DECL
    char*
    assign(
        std::size_t new_size,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    char*
    append(
        std::size_t n,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    void
    insert(
        std::size_t pos,
        const char* s,
        std::size_t n,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    char*
    insert_unchecked(
        std::size_t pos,
        std::size_t n,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    void
    replace(
        std::size_t pos,
        std::size_t n1,
        const char* s,
        std::size_t n2,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    char*
    replace_unchecked(
        std::size_t pos,
        std::size_t n1,
        std::size_t n2,
        storage_ptr const& sp);

    BOOST_JSON_DECL
    void
    shrink_to_fit(
        storage_ptr const& sp) noexcept;

    void
    term(std::size_t n) noexcept
    {
        if(s_.k == short_string_)
        {
            s_.buf[sbo_chars_] =
                static_cast<char>(
                    sbo_chars_ - n);
            s_.buf[n] = 0;
        }
        else
        {
            p_.t->size = static_cast<
                std::uint32_t>(n);
            data()[n] = 0;
        }
    }

    char*
    data() noexcept
    {
        if(s_.k == short_string_)
            return s_.buf;
        return reinterpret_cast<
            char*>(p_.t + 1);
    }

    char const*
    data() const noexcept
    {
        if(s_.k == short_string_)
            return s_.buf;
        return reinterpret_cast<
            char const*>(p_.t + 1);
    }

    char*
    end() noexcept
    {
        return data() + size();
    }

    char const*
    end() const noexcept
    {
        return data() + size();
    }
};

} // detail
BOOST_JSON_NS_END

#endif
