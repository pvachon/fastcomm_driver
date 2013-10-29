#ifndef __INCLUDED_FASTCOMM_FASTCOMM_IOCTL_H__
#define __INCLUDED_FASTCOMM_FASTCOMM_IOCTL_H__

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define FASTCOMM_IOCTL_BASE         0x120000ul

#define FASTCOMM_IOCTL(x)           (FASTCOMM_IOCTL_BASE | ((x)))

struct fastcomm_ioctl_mcast {
    uint32_t mcast_grp_be;
};

/**
 * ioctl for joining a multicast group
 */
#define FASTCOMM_IOCTL_MCAST_JOIN   FASTCOMM_IOCTL(1)

/**
 * ioctl for leaving a multicast group
 */
#define FASTCOMM_IOCTL_MCAST_LEAVE  FASTCOMM_IOCTL(2)

/**
 * Flag for if_index in `struct fastcomm_ioctl_bind` if ifaddr_be should be used.
 * Otherwise, the contents of if_index is a value from if_nametoindex
 * a
 */
#define FASTCOMM_BIND_IF_INDEX_ANY  0xfffffffful

/**
 * Structure for describing interface to bind to
 */
struct fastcomm_ioctl_bind {
    /**
     * Optional, the interface address to bind to.
     */
    uint32_t ifaddr_be;
    /**
     * The index of the interface. If this value is set to FASTCOMM_BIND_IF_INDEX_ANY,
     * attempts to discover the interface using ifaddr_be. Otherwise, ignores ifaddr_be
     * and uses the if_index.
     * \see FASTCOMM_BIND_IF_INDEX_ANY
     */
    uint32_t if_index;
};

/**
 * ioctl for binding the proxy socket to an interface
 */
#define FASTCOMM_IOCTL_BIND_IF      FASTCOMM_IOCTL(3)

#endif /* __INCLUDED_FASTCOMM_FASTCOMM_IOCTL_H__ */

