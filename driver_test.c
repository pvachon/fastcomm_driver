#include <fastcomm_ioctl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int ret = EXIT_FAILURE;

    if (argc < 2) {
        printf("Insufficient arguments. Usage: %s [ifname]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open("/dev/fastcomm", O_RDWR);

    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    int ifindex = if_nametoindex(argv[1]);

    if (ifindex <= 0) {
        printf("Invalid interface name: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    printf("Using interface %s (%d)\n", argv[1], ifindex);

    struct fastcomm_ioctl_bind bind = { .ifaddr_be = 0x0, .if_index = ifindex };

    if (ioctl(fd, FASTCOMM_IOCTL_BIND_IF, &bind) < 0) {
        perror("ioctl");
        goto done;
    }

    struct fastcomm_ioctl_mcast mc;
    mc.mcast_grp_be = htonl(0xe0c0abcd);

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_JOIN, &mc) < 0) {
        perror("ioctl(FASTCOMM_IOCTL_MCAST_JOIN)");
        goto done;
    }

    printf("Sleeping... press any key when done.\n");
    getchar();

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_LEAVE, &mc) < 0) {
        perror("ioctl(FASTCOMM_IOCTL_MCAST_LEAVE)");
        goto done;
    }

    printf("Removed group subscription\n");
    getchar();

    mc.mcast_grp_be = htonl(0xe010e010ul);

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_JOIN, &mc) < 0) {
        perror("2) double ioctl(FASTCOMM_IOCTL_MCAST_JOIN)");
        goto done;
    }

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_JOIN, &mc) < 0) {
        perror("2) second ioctl(FASTCOMM_IOCTL_MCAST_JOIN)");
        goto done;
    }

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_LEAVE, &mc) < 0) {
        perror("2) first ioctl(FASTCOMM_IOCTL_MCAST_LEAVE)");
        goto done;
    }

    printf("Double add, single remove finished.\n");
    getchar();

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_LEAVE, &mc) < 0) {
        perror("2) second ioctl(FASTCOMM_IOCTL_MCAST_LEAVE)");
        goto done;
    }

    printf("Decrement refcount again.\n");
    getchar();

    if (ioctl(fd, FASTCOMM_IOCTL_MCAST_JOIN, &mc) < 0) {
        perror("2) second ioctl(FASTCOMM_IOCTL_MCAST_JOIN)");
        goto done;
    }

    printf("Joined again...\n");
    getchar();

    close(fd);

    ret = EXIT_SUCCESS;
done:
    return ret;
}
