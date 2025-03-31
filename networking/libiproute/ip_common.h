/* vi: set sw=4 ts=4: */
#ifndef IP_COMMON_H
#define IP_COMMON_H 1

#include "libbb.h"
#ifdef __linux__
# include <asm/types.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#else
/* Stub headers for non-Linux platforms */
# include <sys/types.h>
# include <sys/socket.h>
# include <net/if.h>
/* Define minimal types needed */
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef int32_t __s32;
#endif
#ifdef __linux__
# if !defined IFA_RTA
#  include <linux/if_addr.h>
# endif
# if !defined IFLA_RTA
#  include <linux/if_link.h>
# endif
#endif

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

char FAST_FUNC **ip_parse_common_args(char **argv);
//int FAST_FUNC print_neigh(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
int FAST_FUNC ipaddr_list_or_flush(char **argv, int flush);
//int FAST_FUNC iproute_monitor(char **argv);
//void FAST_FUNC ipneigh_reset_filter(void);

int FAST_FUNC do_ipaddr(char **argv);
int FAST_FUNC do_iproute(char **argv);
int FAST_FUNC do_iprule(char **argv);
int FAST_FUNC do_ipneigh(char **argv);
int FAST_FUNC do_iptunnel(char **argv);
int FAST_FUNC do_iplink(char **argv);
//int FAST_FUNC do_ipmonitor(char **argv);
//int FAST_FUNC do_multiaddr(char **argv);
//int FAST_FUNC do_multiroute(char **argv);

POP_SAVED_FUNCTION_VISIBILITY

#ifndef	INFINITY_LIFE_TIME
#define     INFINITY_LIFE_TIME      0xFFFFFFFFU
#endif

#endif
