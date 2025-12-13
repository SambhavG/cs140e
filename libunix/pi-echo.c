#include "libunix.h"
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

int min(int a, int b) { return a < b ? a : b; }

// hack-y state machine to indicate when we've seen the special string
// 'DONE!!!' from the pi telling us to shutdown.
int pi_done(unsigned char *s) {
  static unsigned pos = 0;
  const char exit_string[] = "DONE!!!\n";
  const int n = sizeof exit_string - 1;

  for (; *s; s++) {
    assert(pos < n);
    if (*s != exit_string[pos++]) {
      pos = 0;
      return pi_done(s + 1); // check remainder
    }
    // maybe should check if "DONE!!!" is last thing printed?
    if (pos == sizeof exit_string - 1)
      return 1;
  }
  return 0;
}

// overwrite any unprintable characters with a space.
// otherwise terminals can go haywire/bizarro.
// note, the string can contain 0's, so we send the
// size.
void remove_nonprint(uint8_t *buf, int n) {
  for (int i = 0; i < n; i++) {
    uint8_t *p = &buf[i];
    if (isprint(*p) || (isspace(*p) && *p != '\r'))
      continue;
    *p = ' ';
  }
}

// read and echo the characters from the usbtty until it closes
// (pi rebooted) or we see a string indicating a clean shutdown.
void pi_echo(int unix_fd, int pi_fd, const char *portname) {
  assert(pi_fd);
#if 0
    if(portname)
        output("listening on ttyusb=<%s>\n", portname);
#endif

  // Protocol:
  //  Pi is debugging and gets to a stall state where it needs user input.
  //  1. Pi->host "GET_USER_INPUT" and a package of all debugger state data
  //  2. host prints all the debugger state data, then prints a pidb> prompt
  //  3. user types their input
  //  4. host processes input
  //  if user types b 0x1234, host tells pi to add bp at 0x1234
  //  c: host tells pi to continue without single stepping
  //  s/n: single step one step
  //  q: exit
  //  w 0x1234: watchpoint at 0x1234
  //  r[1-15] = [value]: write value to register
  //  *[addr] = [value]: write value to memory
  //  *addr: read value from memory
  // 5. once host figures out what it wants pi to do, sends corresponding
  // command back to pi which is waiting
  // 6. pi executes command, then runs until it next needs input (while keeping
  // track of debugger state)

  // Every message has following format: byte 1 is command, bytes 2-5 are
  // length, bytes 6-end are message

  // Debugger state has following values:
  // registers 1-16
  // Backtrace?
  // watchpoints
  // breakpoints

  // Additional state that the laptop will have access to:
  // program assembly
  // program code
  // Can use the value of pc (r15) to figure out which function we're in and get
  // that function from program code if it exists Display register values as
  // hex, binary, decimal, and ascii

  while (1) {
    unsigned char buf[4096];

    int n;
    if ((n = read_timeout(unix_fd, buf, sizeof buf, 1000))) {
      buf[n] = 0;
      // output("about to echo <%s> to pi\n", buf);
      // write_exact(pi_fd, buf, n);
      // chunked_write(buf, pi_fd, n, 1000);
      printf("Going to send %d bytes\n", n);
      put_uint8(pi_fd, n);
      for (int i = 0; i < n; i++) {
        put_uint8(pi_fd, buf[i]);
      }
    }

    if (!can_read_timeout(pi_fd, 1000))
      continue;
    n = read(pi_fd, buf, sizeof buf - 1);

    if (!n) {
      // this isn't the program's fault.  so we exit(0).
      if (!portname || tty_gone(portname))
        clean_exit("pi ttyusb connection closed.  cleaning up\n");
      // so we don't keep banginging on the CPU.
      usleep(1000);
    } else if (n < 0) {
      sys_die(read, "pi connection closed.  cleaning up\n");
    } else {
      buf[n] = 0;
      // if you keep getting "" "" "" it's b/c of the GET_CODE message from
      // bootloader
      remove_nonprint(buf, n);
      output("%s", buf);

      if (pi_done(buf)) {
        // output("\nSaw done\n");
        clean_exit("\nbootloader: pi exited.  cleaning up\n");
      }
    }
  }
  notreached();
}
