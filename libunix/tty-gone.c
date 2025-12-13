#include "libunix.h"
#include <sys/stat.h>

int exists(const char *name) {
  struct stat s;
  return stat(name, &s) >= 0;
}

int tty_gone(const char *ttyname) {
  struct stat s;
  return stat(ttyname, &s) < 0;
}
