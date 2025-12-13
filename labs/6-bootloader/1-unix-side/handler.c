#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>

#include "libunix.h"
#include "put-code.h"

int main(int argc, char *argv[]) {
  debug_output("handler started\n");

  // Wait for 100 seconds
  sleep(100);

  return 0;
}
