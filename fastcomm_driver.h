#ifndef __INCLUDED_FASTCOMM_DRIVER_FASTCOMM_DRIVER_H__
#define __INCLUDED_FASTCOMM_DRIVER_FASTCOMM_DRIVER_H__

#define FASTCOMM_MAJOR          0xbee
#define FASTCOMM_NAME           "fastcomm"

#ifdef _FASTCOMM_DEBUG_
#define DIAGK(x, ...) do { printk(KERN_INFO x " (" __FILE__ ":%d)\n", ##__VA_ARGS__, __LINE__); } while (0)
#else
#define DIAGK(...)
#endif

#endif /* __INCLUDED_FASTCOMM_DRIVER_FASTCOMM_DRIVER_H__ */

