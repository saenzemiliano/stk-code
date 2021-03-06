//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2019 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include <string.h>
#ifdef WIN32
#ifdef __GNUC__
#    include <ws2tcpip.h>    // Mingw / gcc on windows
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x501
#    include <winsock2.h>
#    include <ws2tcpip.h>

extern "C"
{
#ifndef InetPtopN
   WINSOCK_API_LINKAGE  INT WSAAPI inet_pton(INT Family, PCSTR pszAddrString, PVOID pAddrBuf);
#endif
}

#else
#    include <winsock2.h>
#    include <in6addr.h>
#    include <ws2tcpip.h>
#endif

#else

#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#endif

#include "network/network_config.hpp"
#include <array>
#include <string>

// ============================================================================
// Android STK seems to crash when using inet_ntop so we copy it from linux
static const char *
stk_inet_ntop4(const u_char *src, char *dst, socklen_t size)
{
    static const char fmt[] = "%u.%u.%u.%u";
    char tmp[sizeof "255.255.255.255"];

    if (sprintf(tmp, fmt, src[0], src[1], src[2], src[3]) >= (int)size)
    {
        return NULL;
    }
    return strcpy(dst, tmp);
}

static const char *
stk_inet_ntop6(const uint8_t *src, char *dst, socklen_t size)
{
    /*
    * Note that int32_t and int16_t need only be "at least" large enough
    * to contain a value of the specified size.  On some systems, like
    * Crays, there is no such thing as an integer variable with 16 bits.
    * Keep this in mind if you think this function should have been coded
    * to use pointer overlays.  All the world's not a VAX.
    */
    char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
    struct { int base, len; } best, cur;
    std::array<uint32_t, 8> words;
    int i;
    /*
    * Preprocess:
    *        Copy the input (bytewise) array into a wordwise array.
    *        Find the longest run of 0x00's in src[] for :: shorthanding.
    */
    words.fill(0);
    for (i = 0; i < 16; i += 2)
        words[i / 2] = ((uint32_t)src[i] << 8) | src[i + 1];
    // Test for nat64 prefix (remove the possible IPv4 in the last 32bit)
    std::array<uint32_t, 8> test_nat64 = words;
    test_nat64[6] = 0;
    test_nat64[7] = 0;

    best.base = -1;
    cur.base = -1;
    best.len = 0;
    cur.len = 0;
    for (i = 0; i < 8; i++)
    {
        if (words[i] == 0)
        {
            if (cur.base == -1)
                cur.base = i, cur.len = 1;
            else
                cur.len++;
        }
        else
        {
            if (cur.base != -1)
            {
                if (best.base == -1 || cur.len > best.len)
                    best = cur;
                cur.base = -1;
            }
        }
    }
    if (cur.base != -1)
    {
        if (best.base == -1 || cur.len > best.len)
            best = cur;
    }
    if (best.base != -1 && best.len < 2)
            best.base = -1;
    /*
    * Format the result.
    */
    tp = tmp;
    for (i = 0; i < 8; i++)
    {
        /* Are we inside the best run of 0x00's? */
        if (best.base != -1 && i >= best.base && i < (best.base + best.len))
        {
            if (i == best.base)
                *tp++ = ':';
            continue;
        }
        /* Are we following an initial run of 0x00s or any real hex? */
        if (i != 0)
            *tp++ = ':';
        /* Is this address an encapsulated IPv4? */
        if (i == 6 &&
            ((best.base == 0 &&
            (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) ||
            test_nat64 == NetworkConfig::get()->getNAT64PrefixData()))
        {
            if (!stk_inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
                return (NULL);
            tp += strlen(tp);
            break;
        }
        tp += sprintf(tp, "%x", words[i]);
    }
    /* Was it a trailing run of 0x00's? */
    if (best.base != -1 && (best.base + best.len) == 8)
        *tp++ = ':';
    *tp++ = '\0';
    /*
     * Check for overflow, copy, and we're done.
     */
    if ((socklen_t)(tp - tmp) > size)
    {
        return NULL;
    }
    return strcpy(dst, tmp);
}

// ----------------------------------------------------------------------------
bool isIPv4MappedAddress(const struct sockaddr_in6* in6)
{
    uint8_t w0 = in6->sin6_addr.s6_addr[0];
    uint8_t w1 = in6->sin6_addr.s6_addr[1];
    uint8_t w2 = in6->sin6_addr.s6_addr[2];
    uint8_t w3 = in6->sin6_addr.s6_addr[3];
    uint8_t w4 = in6->sin6_addr.s6_addr[4];
    uint8_t w5 = in6->sin6_addr.s6_addr[5];
    uint8_t w6 = in6->sin6_addr.s6_addr[6];
    uint8_t w7 = in6->sin6_addr.s6_addr[7];
    uint8_t w8 = in6->sin6_addr.s6_addr[8];
    uint8_t w9 = in6->sin6_addr.s6_addr[9];
    uint8_t w10 = in6->sin6_addr.s6_addr[10];
    uint8_t w11 = in6->sin6_addr.s6_addr[11];
    if (w0 == 0 && w1 == 0 && w2 == 0 && w3 == 0 && w4 == 0 &&
        w5 == 0 && w6 == 0 && w7 == 0 && w8 == 0 && w9 == 0 &&
        w10 == 0xff && w11 == 0xff)
        return true;
    return false;
}   // isIPv4MappedAddress

// ----------------------------------------------------------------------------
std::string getIPV6ReadableFromIn6(const struct sockaddr_in6* in)
{
    std::string ipv6;
    ipv6.resize(INET6_ADDRSTRLEN, 0);
    stk_inet_ntop6(in->sin6_addr.s6_addr, &ipv6[0], INET6_ADDRSTRLEN);
    size_t len = strlen(ipv6.c_str());
    ipv6.resize(len);
    return ipv6;
}   // getIPV6ReadableFromIn6

// ----------------------------------------------------------------------------
bool sameIPV6(const struct sockaddr_in6* in_1, const struct sockaddr_in6* in_2)
{
    // Check port first, then address
    if (in_1->sin6_port != in_2->sin6_port)
        return false;

    const struct in6_addr* a = &(in_1->sin6_addr);
    const struct in6_addr* b = &(in_2->sin6_addr);
    for (unsigned i = 0; i < sizeof(struct in6_addr); i++)
    {
        if (a->s6_addr[i] != b->s6_addr[i])
            return false;
    }
    return true;
}   // sameIPV6

// ----------------------------------------------------------------------------
/** Workaround of a bug in iOS 9 where port number is not written. */
extern "C" int getaddrinfo_compat(const char* hostname,
                                  const char* servname,
                                  const struct addrinfo* hints,
                                  struct addrinfo** res)
{
#ifdef IOS_STK
    int err;
    int numericPort;

    // If we're given a service name and it's a numeric string,
    // set `numericPort` to that, otherwise it ends up as 0.
    numericPort = servname != NULL ? atoi(servname) : 0;

    // Call `getaddrinfo` with our input parameters.
    err = getaddrinfo(hostname, servname, hints, res);

    // Post-process the results of `getaddrinfo` to work around
    if ((err == 0) && (numericPort != 0))
    {
        for (const struct addrinfo* addr = *res; addr != NULL;
             addr = addr->ai_next)
        {
            in_port_t* portPtr;
            switch (addr->ai_family)
            {
                case AF_INET:
                {
                    portPtr = &((struct sockaddr_in*)addr->ai_addr)->sin_port;
                }
                break;
                case AF_INET6:
                {
                    portPtr = &((struct sockaddr_in6*)addr->ai_addr)->sin6_port;
                }
                break;
                default:
                {
                    portPtr = NULL;
                }
                break;
            }
            if ((portPtr != NULL) && (*portPtr == 0))
            {
                *portPtr = htons(numericPort);
            }
        }
    }
    return err;
#else
    return getaddrinfo(hostname, servname, hints, res);
#endif
}   // getaddrinfo_compat

// ----------------------------------------------------------------------------
void andIPv6(struct in6_addr* ipv6, const struct in6_addr* mask)
{
    for (unsigned i = 0; i < sizeof(struct in6_addr); i++)
        ipv6->s6_addr[i] &= mask->s6_addr[i];
}   // andIPv6

// ----------------------------------------------------------------------------
extern "C" int64_t upperIPv6(const char* ipv6)
{
    struct in6_addr v6_in;
    if (inet_pton(AF_INET6, ipv6, &v6_in) != 1)
        return 0;
    uint64_t result = 0;
    unsigned shift = 56;
    for (unsigned i = 0; i < 8; i++)
    {
        uint64_t val = v6_in.s6_addr[i];
        result += val << shift;
        shift -= 8;
    }
    return result;
}

// ----------------------------------------------------------------------------
extern "C" int insideIPv6CIDR(const char* ipv6_cidr, const char* ipv6_in)
{
    const char* mask_location = strchr(ipv6_cidr, '/');
    if (mask_location == NULL)
        return 0;
    struct in6_addr v6_in;
    if (inet_pton(AF_INET6, ipv6_in, &v6_in) != 1)
        return 0;

    char ipv6[INET6_ADDRSTRLEN] = {};
    memcpy(ipv6, ipv6_cidr, mask_location - ipv6_cidr);
    struct in6_addr cidr;
    if (inet_pton(AF_INET6, ipv6, &cidr) != 1)
        return 0;

    int mask_length = atoi(mask_location + 1);
    if (mask_length > 128 || mask_length <= 0)
        return 0;

    struct in6_addr mask = {};
    for (int i = mask_length, j = 0; i > 0; i -= 8, j++)
    {
        if (i >= 8)
            mask.s6_addr[j] = 0xff;
        else
            mask.s6_addr[j] = (unsigned long)(0xffU << (8 - i));
    }

    andIPv6(&cidr, &mask);
    andIPv6(&v6_in, &mask);
    for (unsigned i = 0; i < sizeof(struct in6_addr); i++)
    {
        if (cidr.s6_addr[i] != v6_in.s6_addr[i])
            return 0;
    }
    return 1;
}   // andIPv6

#ifndef ENABLE_IPV6
// ----------------------------------------------------------------------------
extern "C" int isIPv6Socket()
{
    return 0;
}   // isIPV6

// ----------------------------------------------------------------------------
extern "C" void setIPv6Socket(int val)
{
}   // setIPV6

#else
#include <atomic>
// ============================================================================
// For client and server in same process using different thread
std::atomic<int> g_ipv6(0);
// ============================================================================
extern "C" int isIPv6Socket()
{
    return g_ipv6.load();
}   // isIPV6

// ----------------------------------------------------------------------------
extern "C" void setIPv6Socket(int val)
{
    g_ipv6.store(val);
}   // setIPV6

#endif
