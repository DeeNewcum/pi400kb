#include "pi400.h"

#include "gadget-hid.h"

#include <sys/ioctl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EVIOC_GRAB 1
#define EVIOC_UNGRAB 0

#define MAX_MACRO_SIZE 1024         // the maximum number of keyboard reports that can be in a macro
struct recorded_macro {
    struct keyboard_hid_buf kbd_report[MAX_MACRO_SIZE];
    int num_reports;
};
#define MACRO_PLAYBACK_DELAY 50000     // microseconds between each keyboard report

int hid_output;
volatile int running = 0;
volatile int grabbed = 0;

int ret;
int keyboard_fd;
int mouse_fd;
int uinput_keyboard_fd;
int uinput_mouse_fd;
struct keyboard_hid_buf keyboard_buf;
struct hid_buf          mouse_buf;
bool is_recording = false;
struct recorded_macro macro;

void signal_handler(int dummy) {
    running = 0;
}

void prechecks(char *argv0) {
    DIR *dir;

    if (geteuid() != 0) {
        printf("Error: %s must be run as root.\n", argv0);
        exit(1);
    }

    // is the 'dwc2' module properly loaded?
    dir = opendir("/sys/module/dwc2");
    if (!dir || errno == ENOENT) {
        printf("Error: This must be added to /boot/config.txt and the system rebooted:\n");
        printf("    dtoverlay=dwc2\n");
        exit(1);
    }
    closedir(dir);
}

bool modprobe_libcomposite() {
    pid_t pid;

    pid = fork();

    if (pid < 0) return false;
    if (pid == 0) {
        char* const argv[] = {"modprobe", "libcomposite"};
        execv("/usr/sbin/modprobe", argv);
        exit(0);
    }
    waitpid(pid, NULL, 0);
}

bool trigger_hook() {
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s %u", HOOK_PATH, grabbed ? 1u : 0u);
    system(buf);
}

int find_hidraw_device(char *device_type, int16_t vid, int16_t pid) {
    int fd;
    int ret;
    struct hidraw_devinfo hidinfo;
    char path[20];

    for(int x = 0; x < 16; x++){
        sprintf(path, "/dev/hidraw%d", x);

        if ((fd = open(path, O_RDWR | O_NONBLOCK)) == -1) {
            continue;
        }

        ret = ioctl(fd, HIDIOCGRAWINFO, &hidinfo);

        if(hidinfo.vendor == vid && hidinfo.product == pid) {
            printf("Found %s at: %s\n", device_type, path);
            return fd;
        }

        close(fd);
    }

    return -1;
}

int grab(char *dev) {
    printf("Grabbing: %s\n", dev);
    int fd = open(dev, O_RDONLY);
    ioctl(fd, EVIOCGRAB, EVIOC_UNGRAB);
    usleep(500000);
    ioctl(fd, EVIOCGRAB, EVIOC_GRAB);
    return fd;
}

void ungrab(int fd) {
    ioctl(fd, EVIOCGRAB, EVIOC_UNGRAB);
    close(fd);
}

void printhex(unsigned char *buf, size_t len) {
    for(int x = 0; x < len; x++)
    {
        printf("%x ", buf[x]);
    }
    printf("\n");
}

void ungrab_both() {
    printf("Releasing Keyboard and/or Mouse\n");

    if(uinput_keyboard_fd > -1) {
        ungrab(uinput_keyboard_fd);
    }

    if(uinput_mouse_fd > -1) {
        ungrab(uinput_mouse_fd);
    }

    grabbed = 0;

    trigger_hook();
}

void grab_both() {
    printf("Grabbing Keyboard and/or Mouse\n");

    if(keyboard_fd > -1) {
        uinput_keyboard_fd = grab(KEYBOARD_DEV);
    }

    if(mouse_fd > -1) {
        uinput_mouse_fd = grab(MOUSE_DEV);
    }

    if (uinput_keyboard_fd > -1 || uinput_mouse_fd > -1) {
        grabbed = 1;
    }

    trigger_hook();
}

void send_empty_hid_reports_both() {
    if(keyboard_fd > -1) {
#ifndef NO_OUTPUT
        memset((char*)&keyboard_buf.modifier, 0, KEYBOARD_HID_REPORT_SIZE);
        write(hid_output, (unsigned char *)&keyboard_buf, KEYBOARD_HID_REPORT_SIZE + 1);
#endif
    }

    if(mouse_fd > -1) {
#ifndef NO_OUTPUT
        memset(mouse_buf.data, 0, MOUSE_HID_REPORT_SIZE);
        write(hid_output, (unsigned char *)&mouse_buf, MOUSE_HID_REPORT_SIZE + 1);
#endif
    }
}

// a key event from the USB HID keyboard
void handle_keyboard_report() {
    printf("K:");
    printhex((unsigned char*)&(keyboard_buf.modifier), KEYBOARD_HID_REPORT_SIZE);

    int num_keycodes = strnlen((char*)keyboard_buf.keycode, KEYBOARD_HID_REPORT_SIZE - 2);
    if (keyboard_buf.keycode[0] == KEYCODE_ERR_OVF)
        num_keycodes = -1;

    // Ctrl + Raspberry -- toggle capture on/off
    if (keyboard_buf.modifier == (KEY_MOD_LCTRL | KEY_MOD_LMETA) && num_keycodes == 0) {
        if(grabbed) {
            ungrab_both();
            send_empty_hid_reports_both();
        } else {
            grab_both();
        }

    // Ctrl + Shift + Raspberry -- exit
    } else if (keyboard_buf.modifier == (KEY_MOD_LCTRL | KEY_MOD_LMETA | KEY_MOD_LSHIFT) && num_keycodes == 0) {
        running = 0;
        return;

    // Shift + Raspberry + F1 -- record macro
    } else if (keyboard_buf.modifier == (KEY_MOD_LMETA | KEY_MOD_LSHIFT) && num_keycodes == 1 && keyboard_buf.keycode[0] == KEYCODE_F1) {
        if (grabbed) {
            if (is_recording) {
                is_recording = false;
                printf("Finished recording the macro.\n");
            } else {
                is_recording = true;
                memset(&macro, 0, sizeof(macro));
                printf("Started recording the macro.\n");
            }
        }

    // Raspberry + F1 -- playback macro
    } else if (keyboard_buf.modifier == KEY_MOD_LMETA && num_keycodes == 1 && keyboard_buf.keycode[0] == KEYCODE_F1) {
        if (grabbed) {
            printf("Playing back the macro\n");
#ifndef NO_OUTPUT
            for (int ctr=0; ctr<macro.num_reports; ctr++) {
                write(hid_output, (unsigned char*)&macro.kbd_report[ctr], KEYBOARD_HID_REPORT_SIZE + 1);
                usleep(MACRO_PLAYBACK_DELAY);
            }
#endif
        }

    // all other keys
    } else {
        if (grabbed && is_recording) {
            if (macro.num_reports >= MAX_MACRO_SIZE) {
                // automatically stop recording the macro when we hit the limit
                is_recording = false;
                printf("Finished recording the macro.\n");
            } else {
                memcpy(&macro.kbd_report[macro.num_reports], &keyboard_buf, KEYBOARD_HID_REPORT_SIZE + 1);
                macro.num_reports++;
            }
        }

#ifndef NO_OUTPUT
        if(grabbed) {
            write(hid_output, (unsigned char*)&keyboard_buf, KEYBOARD_HID_REPORT_SIZE + 1);
            usleep(1000);
        }
#endif
    }
}

int main(int argc, char *argv[]) {
#ifndef NO_OUTPUT
    prechecks(argv[0]);
#endif

    modprobe_libcomposite();

    keyboard_buf.report_id = 1;
    mouse_buf.report_id = 2;

    keyboard_fd = find_hidraw_device("keyboard", KEYBOARD_VID, KEYBOARD_PID);
    if(keyboard_fd == -1) {
        printf("Failed to open keyboard device\n");
    }
    
    mouse_fd = find_hidraw_device("mouse", MOUSE_VID, MOUSE_PID);
    if(mouse_fd == -1) {
        printf("Failed to open mouse device\n");
    }

    if(mouse_fd == -1 && keyboard_fd == -1) {
        printf("No devices to forward, bailing out!\n");
        return 1;
    }

#ifndef NO_OUTPUT
    ret = initUSB();
    if(ret != USBG_SUCCESS && ret != USBG_ERROR_EXIST) {
        return 1;
    }
#endif

    grab_both();


#ifndef NO_OUTPUT
    do {
        hid_output = open("/dev/hidg0", O_WRONLY | O_NDELAY);
    } while (hid_output == -1 && errno == EINTR);

    if (hid_output == -1){
        printf("Error opening /dev/hidg0 for writing.\n");
        return 1;
    }
#endif

    printf("Running...\n");
    running = 1;
    signal(SIGINT, signal_handler);

    struct pollfd pollFd[2];
    pollFd[0].fd = keyboard_fd;
    pollFd[0].events = POLLIN;
    pollFd[1].fd = mouse_fd;
    pollFd[1].events = POLLIN;

    while (running){
        poll(pollFd, 2, -1);
        if(keyboard_fd > -1) {
            int c = read(keyboard_fd, (char*)&keyboard_buf.modifier, KEYBOARD_HID_REPORT_SIZE);

            if(c == KEYBOARD_HID_REPORT_SIZE)
                handle_keyboard_report();
        }
        if(mouse_fd > -1) {
            int c = read(mouse_fd, mouse_buf.data, MOUSE_HID_REPORT_SIZE);

            if(c == MOUSE_HID_REPORT_SIZE){
                printf("M:");
                printhex(mouse_buf.data, MOUSE_HID_REPORT_SIZE);

#ifndef NO_OUTPUT
                if(grabbed) {
                    write(hid_output, (unsigned char *)&mouse_buf, MOUSE_HID_REPORT_SIZE + 1);
                    usleep(1000);
                }
#endif
            }
        }
    }

    ungrab_both();
    send_empty_hid_reports_both();

#ifndef NO_OUTPUT
    printf("Cleanup USB\n");
    cleanupUSB();
#endif

    return 0;
}
