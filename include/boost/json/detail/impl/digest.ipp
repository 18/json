//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_DIGEST_IPP
#define BOOST_JSON_DETAIL_IMPL_DIGEST_IPP

namespace boost {
namespace json {
namespace detail {

std::size_t
digest(
    char const* s,
    std::size_t n,
    std::size_t salt) noexcept
{
#if BOOST_JSON_ARCH == 64
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = 104729 ^ (n * m);

    const uint64_t * data = (const uint64_t *)s;
    const uint64_t * end = data + (n/8);

    while(data != end)
    {
        uint64_t k = *data++;

        k *= m; 
        k ^= k >> r; 
        k *= m; 
    
        h ^= k;
        h *= m; 
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch(n & 7)
    {
    case 7: h ^= uint64_t(data2[6]) << 48;
    case 6: h ^= uint64_t(data2[5]) << 40;
    case 5: h ^= uint64_t(data2[4]) << 32;
    case 4: h ^= uint64_t(data2[3]) << 24;
    case 3: h ^= uint64_t(data2[2]) << 16;
    case 2: h ^= uint64_t(data2[1]) << 8;
    case 1: h ^= uint64_t(data2[0]);
    h *= m;
    };
 
    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
#else
    std::uint32_t const prime = 0x01000193UL;
    std::uint32_t hash  = 0x811C9DC5UL;
    hash += salt;
    for(;n--;++s)
        hash = (*s ^ hash) * prime;
    return hash;
#endif
}

} // detail
} // json
} // boost

#endif
