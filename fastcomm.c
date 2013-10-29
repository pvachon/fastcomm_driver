/*
 * FASTCOMM driver layer.
 *
 * Copyright (C)2013 Phil Vachon <phil@cowpig.ca>
 *
 * Licensed under the GNU General Public License v2. See LICENSE in the source
 * distribution directory for more information.
 */
#include <fastcomm_driver.h>
#include <fastcomm_ioctl.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/user_namespace.h>
#include <net/inet_sock.h>

struct fastcomm_igmp_group {
    atomic_t refcnt;
    uint32_t group_addr_be;
    struct rcu_head rcu;
    struct list_head gnode;
};

struct fastcomm_state {
    /* The netdev interface index */
    int if_idx;

    /* The actual device we're referencing */
    struct in_device *in_dev;

    /* List of groups this instance has joined */
    struct list_head groups;

};

static inline
void fastcomm_igmp_group_destroy_rcu(struct rcu_head *head)
{
    struct fastcomm_igmp_group *grp = container_of(head, struct fastcomm_igmp_group, rcu);

    kfree(grp);
}

static inline
struct fastcomm_igmp_group *find_igmp_group_rcu(struct fastcomm_state *state, uint32_t grp_be)
{
    /* TODO: RCU-ify this */
    struct fastcomm_igmp_group *grp;

    list_for_each_entry_rcu(grp, &state->groups, gnode) {
        if (grp->group_addr_be == grp_be) {
            return grp;
        }
    }

    return NULL;
}

static
long fastcomm_drv_ioctl_bind_if(struct fastcomm_state *state,
                                struct fastcomm_ioctl_bind *bind)
{
    long ret = 0;

    int ifidx = -1;
    struct net_device *dev = NULL;

    /* Find the device we want to bind to */
    if (bind->if_index == FASTCOMM_BIND_IF_INDEX_ANY) {
        /* Attempt to find the device using the specified address */
        dev = ip_dev_find(current->nsproxy->net_ns, bind->ifaddr_be);
        ifidx = dev->ifindex;
    } else {
        dev = dev_get_by_index(current->nsproxy->net_ns, bind->if_index);
        ifidx = bind->if_index;
    }

    if (NULL == dev) {
        ret = -EADDRNOTAVAIL;
        DIAGK("Failed to find a device that corresponds to the given interface index: %u\n", (unsigned)bind->if_index);
        goto done;
    }

    state->in_dev = in_dev_get(dev);
    dev_put(dev);

    state->if_idx = ifidx;

done:
    return ret;
}

static
long fastcomm_drv_change_mcast_membership(struct fastcomm_state *state,
                                          struct fastcomm_ioctl_mcast *mcast,
                                          int leave)
{
    long ret = 0;
    struct fastcomm_igmp_group *group = NULL;

    if (state->if_idx < 0 || NULL == state->in_dev) {
        DIAGK("IGMP stack management has not been bound to an interface yet");
        ret = -ENODEV;
        goto done;
    }

    group = find_igmp_group_rcu(state, mcast->mcast_grp_be);

    if (leave) {
        if (NULL != group) {
            atomic_dec(&group->refcnt);

            if (atomic_read(&group->refcnt) == 0) {
                rtnl_lock();
                ip_mc_dec_group(state->in_dev, group->group_addr_be);
                rtnl_unlock();
                list_del_rcu(&group->gnode);
                call_rcu(&group->rcu, fastcomm_igmp_group_destroy_rcu);
            }
        } else {
            DIAGK("Group not found, skipping");
            ret = -EINVAL;
            goto done;
        }
    } else {
        if (NULL == group) {
            DIAGK("New IGMP group subscription");
            group = kzalloc(sizeof(*group), GFP_KERNEL);
            if (NULL == group) {
                ret = -ENOMEM;
                goto done;
            }

            INIT_LIST_HEAD(&group->gnode);
            group->group_addr_be = mcast->mcast_grp_be;
            list_add_rcu(&state->groups, &group->gnode);
            rtnl_lock();
            ip_mc_inc_group(state->in_dev, group->group_addr_be);
            rtnl_unlock();
        }

        atomic_inc(&group->refcnt);
    }

    if (ret < 0) {
        DIAGK("Failed in ip_mc_join_group - %ld", ret);
    }

done:
    return ret;

}

static
long fastcomm_drv_ioctl_join_mcast_grp(struct fastcomm_state *state,
                                       struct fastcomm_ioctl_mcast *mcast)
{
    return fastcomm_drv_change_mcast_membership(state, mcast, 0);
}

static
long fastcomm_drv_ioctl_leave_mcast_grp(struct fastcomm_state *state,
                                        struct fastcomm_ioctl_mcast *mcast)
{
    return fastcomm_drv_change_mcast_membership(state, mcast, 1);
}

static
long fastcomm_drv_ioctl(struct file *fp, unsigned int id, unsigned long data)
{
    long ret = 0;
    struct fastcomm_state *state = (struct fastcomm_state *)fp->private_data;

    DIAGK("Driver device node ioctl 0x%08x requested.", id);

    switch (id) {
    case FASTCOMM_IOCTL_MCAST_JOIN: {
            struct fastcomm_ioctl_mcast kmcast;
            DIAGK("Got FASTCOMM_IOCTL_MCAST_JOIN");

            if (copy_from_user(&kmcast, (void *)data, sizeof(kmcast)) > 0) {
                printk(KERN_ALERT "Couldn't copy fastcomm_ioctl_mcast data from userspace ptr.\n");
            }

            ret = fastcomm_drv_ioctl_join_mcast_grp(state, &kmcast);
        }
        break;

    case FASTCOMM_IOCTL_MCAST_LEAVE: {
            struct fastcomm_ioctl_mcast kmcast;
            DIAGK("Got FASTCOMM_IOCTL_MCAST_LEAVE");

            if (copy_from_user(&kmcast, (void *)data, sizeof(kmcast)) > 0) {
                printk(KERN_ALERT "Couldn't copy fastcomm_ioctl_mcast data from userspace ptr.\n");
            }

            ret = fastcomm_drv_ioctl_leave_mcast_grp(state, &kmcast);
        }
        break;

    case FASTCOMM_IOCTL_BIND_IF: {
            struct fastcomm_ioctl_bind kbind;
            DIAGK("Got FASTCOMM_IOCTL_BIND_IF");

            if (copy_from_user(&kbind, (void *)data, sizeof(kbind)) > 0) {
                printk(KERN_ALERT "Couldn't copy fastcomm_ioctl_bind data from userspace ptr.\n");
            }

            ret = fastcomm_drv_ioctl_bind_if(state, &kbind);
        }
        break;
    default:
        ret = -EINVAL;
        goto done;
    }

done:
    return ret;
}

static
int fastcomm_drv_release(struct inode *in, struct file *fp)
{
    int ret = 0;

    struct fastcomm_state *fc_state = (struct fastcomm_state *)fp->private_data;
    struct fastcomm_igmp_group *group = NULL, *next = NULL;

    DIAGK("Driver device node was closed");

    /* Leave the IGMP groups that we joined for this app */
    list_for_each_entry_safe(group, next, &fc_state->groups, gnode) {
        DIAGK("Subscription at shutdown: 0x%08x", group->group_addr_be);
        rtnl_lock();
        ip_mc_dec_group(fc_state->in_dev, group->group_addr_be);
        rtnl_unlock();
        list_del_rcu(&group->gnode);
        call_rcu(&group->rcu, fastcomm_igmp_group_destroy_rcu);
    }

    if (NULL != fc_state->in_dev) {
        in_dev_put(fc_state->in_dev);
        fc_state->in_dev = NULL;
    }

    kfree(fc_state);

    return ret;
}

static
int fastcomm_drv_open(struct inode *in, struct file *fp)
{
    int ret = 0;

    struct fastcomm_state *fc_state = NULL;

    DIAGK("Driver device node was opened");

    fc_state = kzalloc(sizeof(*fc_state), GFP_KERNEL);

    if (NULL == fc_state) {
        DIAGK("Out of memory, unable to allocate struct fastcomm_state!");
        ret = -ENOMEM;
        goto done;
    }

    INIT_LIST_HEAD(&fc_state->groups);

    fp->private_data = (void *)fc_state;

done:
    if (ret < 0) {
        if (NULL != fc_state) {
            kfree(fc_state);
            fc_state = NULL;
        }
    }
    return ret;
}

static
struct file_operations fastcomm_fops = {
    .read = NULL,
    .write = NULL,
    .open = fastcomm_drv_open,
    .release = fastcomm_drv_release,
    .unlocked_ioctl = fastcomm_drv_ioctl,
};

static __init
int init_fastcomm(void)
{
    int ret = 0;
    DIAGK("Initializing the FASTCOMM Driver Interface");

    DIAGK("Author: Phil Vachon <philippe@cowpig.ca>");
    DIAGK("Built on " __DATE__);

    if (register_chrdev(FASTCOMM_MAJOR, FASTCOMM_NAME, &fastcomm_fops) < 0) {
        printk(KERN_ALERT "Failed to initialize character device for fastcomm.\n");
        ret = -1;
        goto done;
    }

done:
    return ret;
}

static __exit
void cleanup_fastcomm(void)
{
    unregister_chrdev(FASTCOMM_MAJOR, FASTCOMM_NAME);
}

module_init(init_fastcomm);
module_exit(cleanup_fastcomm);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Vachon <philippe@cowpig.ca>");
MODULE_DESCRIPTION("Driver providing access to kernel IP stack for FASTCOMM");


