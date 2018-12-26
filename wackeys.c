#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <libinput.h>
#include <libudev.h>

#define VENDOR_ID "056a"
#define SOCK_PATH "/tmp/WacKeys.sock"

static int sockfd;
static int updated_pos = 0; // Will hold ring position
static volatile sig_atomic_t stop = 0;

void signal_handler(int sig)
{
    stop = 1;
    close_sockfd(sig);
}

void close_sockfd(int sig)
{
    close(sockfd);
}

int open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

void close_restricted(int fd, void *user_data)
{
    close(fd);
}

void handle_buttons(struct libinput_event *ev)
{
    char msg[8];
    struct libinput_event_tablet_pad *pad = libinput_event_get_tablet_pad_event(ev);
    enum libinput_button_state state;
    unsigned int button, mode;

    button = libinput_event_tablet_pad_get_button_number(pad);
    state = libinput_event_tablet_pad_get_button_state(pad);
    mode = libinput_event_tablet_pad_get_mode(pad);

    sprintf(msg, "B%c%03dM%02d",
            state == LIBINPUT_BUTTON_STATE_PRESSED ? 'P' : 'R',
            button,
            mode);
    write(sockfd, msg, strlen(msg));
}

void handle_ring(struct libinput_event *ev)
{
    char msg[8];
    struct libinput_event_tablet_pad *pad = libinput_event_get_tablet_pad_event(ev);
    unsigned int mode;
    int pos = 0;

    pos = (int)libinput_event_tablet_pad_get_ring_position(pad);
    mode = libinput_event_tablet_pad_get_mode(pad);

    if(pos != -1) {
        updated_pos = pos;
        sprintf(msg, "RR%03dM%02d",
                pos,
                mode);
        write(sockfd, msg, strlen(msg));
    } else {
        // When rotation is done, send last position instead of -1
        sprintf(msg, "RD%03dM%02d",
                updated_pos,
                mode);
        write(sockfd, msg, strlen(msg));
    }
}

void handle_libinput_events(struct libinput *li)
{
    struct libinput_event *event;
    libinput_dispatch(li);
    while((event = libinput_get_event(li)) != NULL) {
        switch(libinput_event_get_type(event)) {
            case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
                handle_buttons(event);
                break;
            case LIBINPUT_EVENT_TABLET_PAD_RING:
                handle_ring(event);
                break;
        }
        libinput_event_destroy(event);
        libinput_dispatch(li);
    }
}

void libudev_add_wacom(struct udev *udev, struct libinput *li)
{
    // Wait until event nodes are created
    usleep(500000);

    struct udev_device *dev;
    struct udev_list_entry *devices, *list;
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);

    udev_enumerate_add_match_property(enumerate, "ID_VENDOR_ID", VENDOR_ID);
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(list, devices) {
        dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(list));
        const char *path = udev_device_get_devnode(dev);
        if(path) {
            if(strstr(path, "event") != NULL)
                if(!libinput_path_add_device(li, path)) {
                    fprintf(stderr, "Failed to add device: %s\n", path);
                    libinput_unref(li);
                    udev_device_unref(dev);
                    udev_enumerate_unref(enumerate);
                    udev_unref(udev);
                    exit(EXIT_FAILURE);
                }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);
}

void handle_libudev_events(struct udev_monitor *mon, struct libinput *li)
{
    struct udev_device *dev = udev_monitor_receive_device(mon);
    if(dev)
        if(strcmp(udev_device_get_action(dev), "add") == 0)
            libudev_add_wacom(udev_monitor_get_udev(mon), li);
    udev_device_unref(dev);
}

int main(int argc, char **argv)
{
    int opt = 0, run_as_daemon = 0;

    while((opt = getopt(argc, argv, "hdv")) != -1) {
        switch(opt) {
            case 'd':
                run_as_daemon = 1;
                break;
            case 'h':
                printf("Usage: %s [options]\n  -d\tRun process as daemon\n  -v\tPrint version number and exit\n", NAME);
                exit(EXIT_SUCCESS);
            case 'v':
                printf("%s v%s\n", NAME, VERSION);
                exit(EXIT_SUCCESS);
        }
    }

    if(run_as_daemon)
        daemon(0, 1);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, close_sockfd);

    int fd = -1, len = sizeof(struct sockaddr_un);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));

    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        fprintf(stderr, "Failed connecting socket\n");

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);
    unlink(SOCK_PATH);
    if(bind(fd, (struct sockaddr *) &addr, len) < 0)
        fprintf(stderr, "Failed binding socket\n");

    // Change permissons so user can connect to socket
    chmod(SOCK_PATH, 0666);

    if(listen(fd, 10) < 0)
        fprintf(stderr, "Failed listening to socket\n");

    struct udev *udev;
    struct udev_monitor *mon;
    struct libinput *li;

    const struct libinput_interface interface = {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted
    };

    if(!(udev = udev_new())) {
        fprintf(stderr, "Failed to create udev\n");
        exit(EXIT_FAILURE);
    }

    if(!(mon = udev_monitor_new_from_netlink(udev, "udev"))) {
        fprintf(stderr, "Failed to create udev monitor\n");
        exit(EXIT_FAILURE);
    }

    li = libinput_path_create_context(&interface, NULL);
    if(!li) {
        fprintf(stderr, "Failed to initialize libinput context\n");
        exit(EXIT_FAILURE);
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
    int mon_fd = udev_monitor_get_fd(mon);
    udev_monitor_enable_receiving(mon);

    libudev_add_wacom(udev, li);

    // Handle first "LIBINPUT_EVENT_DEVICE_ADDED" without logging
    handle_libinput_events(li);

    // Poll for other events
    struct pollfd fds[3];
    fds[0].fd = libinput_get_fd(li);
    fds[1].fd = mon_fd;
    fds[2].fd = fd;
    fds[0].events = POLLIN;
    fds[1].events = POLLIN;
    fds[2].events = POLLIN;
    fds[0].revents = 0;
    fds[1].revents = 0;
    fds[2].revents = 0;

    while(!stop && poll(fds, 3, -1) > -1) {
        if(fds[0].revents & POLLIN) {
            handle_libinput_events(li);
        }
        if(fds[1].revents & POLLIN) {
            handle_libudev_events(mon, li);
        }
        if(fds[2].revents & POLLIN) {
            if((sockfd = accept(fd, NULL, &len)) < 0)
                fprintf(stderr, "Failed accepting client\n");
        }
    }
    udev_unref(udev);
    libinput_unref(li);

    return EXIT_SUCCESS;
}
