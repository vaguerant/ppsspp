#ifndef	_NETINET_IN_H
#define	_NETINET_IN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct in_addr
{
   unsigned int s_addr;
};

#define INADDR_ANY   0
#define INADDR_NONE -1

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

struct sockaddr_in
{
   short sin_family;
   unsigned short sin_port;
   struct in_addr sin_addr;
   char sin_zero[8];
};

/* no ipv6 support, those are only provided to prevent compilation errors. */
#define IPPROTO_IPV6    0
#define IPV6_V6ONLY		0
struct in6_addr {
   unsigned char s6_addr[16];
};
struct sockaddr_in6 {
   short sin6_family;
   unsigned short sin6_port;
   uint32_t sin6_flowinfo;
   struct in6_addr sin6_addr;
   uint32_t sin6_scope_id;
};
static const struct in6_addr in6addr_any = {0};

uint32_t ntohl (uint32_t netlong);
uint16_t ntohs (uint16_t netshort);
uint32_t htonl (uint32_t hostlong);
uint16_t htons (uint16_t hostshort);

#ifdef __cplusplus
}
#endif

#endif	/* _NETINET_IN_H */
