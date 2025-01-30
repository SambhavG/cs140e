#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libunix.h"

// allocate buffer, read entire file into it, return it.
// buffer is zero padded to a multiple of 4.
//
//  - <size> = exact nbytes of file.
//  - for allocation: round up allocated size to 4-byte multiple, pad
//    buffer with 0s.
//
// fatal error: open/read of <name> fails.
//   - make sure to check all system calls for errors.
//   - make sure to close the file descriptor (this will
//     matter for later labs).
//
void *read_file(unsigned *size, const char *name) {
    // How:
    //    - use stat() to get the size of the file.
    //    - round up to a multiple of 4.
    //    - allocate a buffer
    //    - zero pads to a multiple of 4.
    //    - read entire file into buffer (read_exact())
    //    - fclose() the file descriptor
    //    - make sure any padding bytes have zeros.
    //    - return it.
    struct stat file_data;
    int err = stat(name, &file_data);
    if (err != 0) return NULL;

    unsigned file_size = (unsigned)file_data.st_size;
    *size = file_size;

    void *data = calloc(1, file_size + 4);
    FILE *fd = fopen(name, "r");
    if (fd == NULL) return NULL;
    if (file_size > 0) {
        read_exact(fileno(fd), data, file_size);
    }
    fclose(fd);

    return data;
}
