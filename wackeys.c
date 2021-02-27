#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <libinput.h>
#include <libudev.h>

#define SOCK_PATH "/tmp/WacKeys.sock"

static int sockfd;
static int updated_pos = 0; // Will hold ring position
static volatile sig_atomic_t stop = 0;
static int exit_status = EXIT_SUCCESS;

void close_sockfd(int sig)
{
    close(sockfd);
}

void signal_handler(int sig)
{
    stop = 1;
    close_sockfd(sig);
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
    char msg[9];
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
    char msg[9];
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
            default: // do nothing
                break;
        }
        libinput_event_destroy(event);
        libinput_dispatch(li);
    }
}

void usage() {
    printf("\
Usage: %s [options]\n\
  -d, --daemon\tRun process as daemon\n\
  -h, --help\tDisplay help and exit\n\
  -s, --seat\tDefine seat name to use\n\
  -v, --version\tPrint version number and exit\n", NAME);
}

int main(int argc, char **argv)
{
    int opt = 0, run_as_daemon = 0;
    char seat[256] = "seat0"; // default seat set to seat0
    const struct option options[] = {
        {"daemon",   no_argument,        &run_as_daemon,    1},
        {"help",     no_argument,        0,               'h'},
        {"seat",     required_argument,  0,               's'},
        {"version",  no_argument,        0,               'v'},
    };

    while((opt = getopt_long(argc, argv, "dhs:v", options, NULL)) != -1) {
        switch (opt) {
            case 0: // do nothing
                break;
            case 'd':
                run_as_daemon = 1;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 's':
                strncpy(seat, optarg, sizeof(seat));
                break;
            case 'v':
                printf("%s v%s\n", NAME, VERSION);
                exit(EXIT_SUCCESS);
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if(run_as_daemon)
        daemon(0, 1);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, close_sockfd);

    int fd = -1;
    socklen_t len = sizeof(struct sockaddr_un);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));

    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Failed connecting socket\n");
        exit(EXIT_FAILURE);
    }

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);
    unlink(SOCK_PATH);
    if(bind(fd, (struct sockaddr *) &addr, len) < 0) {
        fprintf(stderr, "Failed binding socket\n");
        exit(EXIT_FAILURE);
    }

    // Change permissons so user can connect to socket
    chmod(SOCK_PATH, 0666);

    if(listen(fd, 10) < 0) {
        fprintf(stderr, "Failed listening to socket\n");
        exit(EXIT_FAILURE);
    }

    struct udev *udev;
    struct libinput *li;

    const struct libinput_interface interface = {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted
    };

    if(!(udev = udev_new())) {
        fprintf(stderr, "Failed to create udev\n");
        exit_status = EXIT_FAILURE;
        goto EXIT;
    }

    li = libinput_udev_create_context(&interface, NULL, udev);

    if(!li) {
        fprintf(stderr, "Failed to initialize libinput context\n");
        exit_status = EXIT_FAILURE;
        goto EXIT;
    }

    if(libinput_udev_assign_seat(li, seat) < 0) {
        fprintf(stderr, "Failed to assign seat\n");
        exit_status = EXIT_FAILURE;
        goto EXIT;
    }

    libinput_dispatch(li);

    // Poll for other events
    struct pollfd fds[2];
    fds[0].fd = libinput_get_fd(li);
    fds[1].fd = fd;
    fds[0].events = POLLIN;
    fds[1].events = POLLIN;
    fds[0].revents = 0;
    fds[1].revents = 0;

    nfds_t nfds = sizeof(fds)/sizeof(struct pollfd);

    while(!stop && poll(fds, nfds, -1) > -1) {
        if(fds[0].revents & POLLIN) {
            handle_libinput_events(li);
        }
        if(fds[1].revents & POLLIN) {
            if((sockfd = accept(fd, NULL, &len)) < 0)
                fprintf(stderr, "Failed accepting client\n");
        }
    }
EXIT:
    udev_unref(udev);
    libinput_unref(li);

    return exit_status;
}
