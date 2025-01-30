// engler, cs140e: your code to find the tty-usb device on your laptop.
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "libunix.h"

#define _SVID_SOURCE
#include <dirent.h>
static const char *ttyusb_prefixes[] = {
    "ttyUSB",       // linux
    "ttyACM",       // linux
    "cu.SLAB_USB",  // mac os
    "cu.usbserial"  // mac os
                    // if your system uses another name, add it.
};

static int filter(const struct dirent *d) {
    // scan through the prefixes, returning 1 when you find a match.
    // 0 if there is no match.
    for (int i = 0; i < sizeof(ttyusb_prefixes) / sizeof(ttyusb_prefixes[0]); i++) {
        const char *prefix = ttyusb_prefixes[i];
        if (strncmp(prefix, d->d_name, strlen(prefix)) == 0) return 1;
    }
    return 0;
}

// find the TTY-usb device (if any) by using <scandir> to search for
// a device with a prefix given by <ttyusb_prefixes> in /dev
// returns:
//  - device name.
// error: panic's if 0 or more than 1 devices.
char *find_ttyusb(void) {
    // use <alphasort> in <scandir>
    // return a malloc'd name so doesn't corrupt.
    struct dirent **namelist;
    int num_entries = scandir("/dev", &namelist, filter, alphasort);
    if (num_entries == -1) return NULL;
    if (num_entries != 1) panic("Found %u matching devices", num_entries);
    char *result = malloc(100);
    strcat(result, "/dev/");
    strcat(result, (*namelist)->d_name);
    return strdupf(result);
}

// return the most recently mounted ttyusb (the one
// mounted last).  use the modification time
// returned by stat().
char *find_ttyusb_last(void) {
    struct dirent **namelist;
    int num_entries = scandir("/dev", &namelist, filter, alphasort);
    if (num_entries == -1) return NULL;
    printf("Num entries: %u\n", num_entries);
    time_t best_time = 0;
    char *best_name = NULL;
    char *dir_abs_path = malloc(100);
    struct stat filedata;
    for (int i = 0; i < num_entries; i++) {
        strcat(dir_abs_path, "/dev/");
        strcat(dir_abs_path, namelist[i]->d_name);
        stat(dir_abs_path, &filedata);

        if (filedata.st_mtimespec.tv_sec > best_time) {
            best_time = filedata.st_mtimespec.tv_sec;
            best_name = strdupf(dir_abs_path);
        }

        dir_abs_path[0] = '\0';
    }

    return best_name;
}

// return the oldest mounted ttyusb (the one mounted
// "first") --- use the modification returned by
// stat()
char *find_ttyusb_first(void) {
    struct dirent **namelist;
    int num_entries = scandir("/dev", &namelist, filter, alphasort);
    if (num_entries == -1) return NULL;
    time_t best_time = __LONG_MAX__;
    char *best_name = NULL;
    char *dir_abs_path = malloc(100);
    struct stat filedata;
    for (int i = 0; i < num_entries; i++) {
        strcat(dir_abs_path, "/dev/");
        strcat(dir_abs_path, namelist[i]->d_name);
        stat(dir_abs_path, &filedata);
        if (filedata.st_mtimespec.tv_sec < best_time) {
            best_time = filedata.st_mtimespec.tv_sec;
            best_name = strdupf(dir_abs_path);
        }

        dir_abs_path[0] = '\0';
    }

    return best_name;
}
