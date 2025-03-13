#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "libunix.h"

static const char *progname;
static int num_lines_last_printed = 0;
typedef struct debugger_state {
    int regs[17];
    int changed_regs[17];
    int breakpoints[5];
    int watchpoints[2];
} debugger_state_t;

enum debugger_action{
    ADD_BP,
    REMOVE_BP,
    STEP,
    CONTINUE,
    EXIT,
    ADD_WP,
    REMOVE_WP,
    WRITE_REG,
    READ_ADDR,
    WRITE_ADDR
};

unsigned parse_input(char* input) {
    //Assuming input is some big buffer we can modify in place
    char workspace[100];
    if (input[0] == '\n' || strncmp(input, "s\n", 2) == 0) {
        input[0] = STEP;
        input[1] = 0;
        return 2;
    }

    if (strncmp(input, "b ", 2) == 0) {
        input[0] = ADD_BP;
        //write 1,2,3,4 with the int
        unsigned *addr = (unsigned *) (input+1);
        *addr = (unsigned) strtol(input+2, NULL, 0);
        unsigned addr_sending = *(unsigned *)(input+1);
        input[5] = 0;
        return 6;
    }
    //bc - breakpoint clear
    if (strncmp(input, "bc ", 3) == 0) {
        input[0] = REMOVE_BP;
        unsigned *addr = (unsigned *) (input+1);
        *addr = (unsigned) strtol(input+3, NULL, 0);
        input[5] = 0;
        return 6;
    }
    if (strncmp(input, "w ", 2) == 0) {
        input[0] = ADD_WP;
        unsigned *addr = (unsigned *) (input+1);
        *addr = (unsigned) strtol(input+2, NULL, 0);
        input[5] = 0;
        return 6;
    }
    // wc - watchpoint clear
    if (strncmp(input, "wc ", 3) == 0) {
        input[0] = REMOVE_WP;
        unsigned *addr = (unsigned *) (input+1);
        *addr = (unsigned) strtol(input+3, NULL, 0);
        input[5] = 0;
        return 6;
    }

    if (strncmp(input, "c", 1) == 0) {
        input[0] = CONTINUE;
        input[1] = 0;
        return 2;
    }

    //*(addr) = (value)
    //If first character is * and contains =, then it's a write to memory
    if (strncmp(input, "*", 1) == 0 && strchr(input, '=') != NULL) {
        input[0] = WRITE_ADDR;
        unsigned addr = strtol(input+1, NULL, 0);
        char *equals_char = strchr(input, '=');
        unsigned value = strtol(equals_char+1, NULL, 0);
        //bytes 1-4 are addr, bytes 5-8 are value
        unsigned *addr_ptr = (unsigned *) (input+1);
        *addr_ptr = addr;
        unsigned *value_ptr = (unsigned *) (input+5);
        *value_ptr = value;
        input[9] = 0;
        return 10;
    }
    //*(addr)
    //If first character is * and didn't hit other case, then it's a read from memory
    if (strncmp(input, "*", 1) == 0) {
        input[0] = READ_ADDR;
        unsigned addr = strtol(input+1, NULL, 0);
        unsigned *addr_ptr = (unsigned *) (input+1);
        *addr_ptr = addr;
        input[5] = 0;
        return 10;
    }
    //r[1-15] = (value)
    //If first character is r and contains =, then it's a write to register
    if (strncmp(input, "r", 1) == 0 && strchr(input, '=') != NULL) {
        input[0] = WRITE_REG;
        unsigned reg = strtol(input+1, NULL, 0);
        unsigned value = strtol(strchr(input, '=')+1, NULL, 0);
        unsigned *reg_ptr = (unsigned *) (input+1);
        *reg_ptr = reg;
        unsigned *value_ptr = (unsigned *) (input+5);
        *value_ptr = value;
        input[9] = 0;
        return 10;
    }
    printf("UNIX: when parsing input \"%s\", couldn't find command\n", input);
    return 0;
}

void wait_and_send_input(int unix_fd, int pi_fd) {
    //Wait until the user sends input
    //If no input after 1000 usec, returns
    char buf[4096];
    int n;
    if((n=read_timeout(unix_fd, buf, sizeof buf, 100))) {
        buf[n] = 0;
        int parsed_len = parse_input((char *) buf);
        if (parsed_len == 0) {
            printf("UNIX: Unrecognized input!\n");
            return;
        }
        // printf("UNIX: user provided input that was parsed to be %u bytes long\n", parsed_len);

        put_uint8(pi_fd, parsed_len);
        for (int i = 0; i < parsed_len; i++) {
            put_uint8(pi_fd, buf[i]);
        }
    }
}

void print_debugger_state(debugger_state_t* db) {
    //clear terminal
    // printf("\033c");
    //Process program
    //Replace the final .bin in the path with .list
    unsigned pc = db->regs[15];
    unsigned len = strlen(progname);
    char new_progname[300];
    strcpy(new_progname, progname);
    char new_end[4] = "list";
    for (int i = len; i > len-4; i--) {
        new_progname[i] = new_end[i-len+3];
    }

    //Read the list file
    FILE *list_file = fopen(new_progname, "r");
    if (!list_file) {
        printf("UNIX: Error opening list file\n");
        return;
    }

    // Buffer to store each line from the list file
    char line[256];
    
    // Arrays to store function lines
    char function_lines[200][256];
    int line_count = 0;
    int pc_line_index = -1;
    
    // Store the function name
    char function_name[64] = "unknown";
    int in_target_function = 0;
    unsigned int function_start_addr = 0;
    unsigned int function_end_addr = 0xFFFFFFFF;
    
    // First pass: find the function containing PC
    rewind(list_file);
    while (fgets(line, sizeof(line), list_file)) {
        // Check if this is a function header line (format like "00008574 <fib>:")
        if (strlen(line) > 12 && line[8] == ' ' && line[9] == '<' && strchr(line, '>') != NULL) {
            // Extract function address
            char addr_str[9] = {0};
            strncpy(addr_str, line, 8);
            unsigned int func_addr = 0;
            sscanf(addr_str, "%x", &func_addr);
            
            // If PC is greater than or equal to this function's address
            // and less than the next function's address (which we don't know yet),
            // this might be our function
            if (pc >= func_addr && func_addr > function_start_addr) {
                function_start_addr = func_addr;
                
                // Extract function name between < and >
                char *start = strchr(line, '<') + 1;
                char *end = strchr(line, '>');
                if (start && end && end > start) {
                    int name_len = end - start;
                    if (name_len < sizeof(function_name)) {
                        strncpy(function_name, start, name_len);
                        function_name[name_len] = '\0';
                    }
                }
            } else if (func_addr > function_start_addr && func_addr < function_end_addr) {
                // This is the next function after our target function
                function_end_addr = func_addr;
            }
        }
    }
    
    // Second pass: collect all lines of the target function
    rewind(list_file);
    in_target_function = 0;
    line_count = 0;
    
    while (fgets(line, sizeof(line), list_file) && line_count < 200) {
        // Check if this is a function header line
        if (strlen(line) > 12 && line[8] == ' ' && line[9] == '<' && strchr(line, '>') != NULL) {
            char addr_str[9] = {0};
            strncpy(addr_str, line, 8);
            unsigned int func_addr = 0;
            sscanf(addr_str, "%x", &func_addr);
            
            if (func_addr == function_start_addr) {
                // We've found the start of our target function
                in_target_function = 1;
                // Store the function header
                strcpy(function_lines[line_count++], line);
            } else if (func_addr == function_end_addr) {
                // We've reached the next function, stop collecting
                break;
            }
        } else if (in_target_function) {
            // This is a line in our target function
            
            // Check if this is an instruction line
            if (strlen(line) > 9 && line[8] == ':' && 
                isspace(line[0]) && isspace(line[1]) && isspace(line[2]) && isspace(line[3]) &&
                isxdigit(line[4]) && isxdigit(line[5]) && isxdigit(line[6]) && isxdigit(line[7])) {
                
                // Extract the address part
                char addr_str[5] = {0};
                strncpy(addr_str, line + 4, 4);
                
                // Convert to integer for proper comparison
                unsigned int line_addr = 0;
                sscanf(addr_str, "%x", &line_addr);
                
                // Get the lower 16 bits of PC for comparison
                unsigned int pc_lower = pc & 0xFFFF;
                
                // Check if we found the PC
                if (line_addr == pc_lower) {
                    pc_line_index = line_count;
                }
            }
            
            // Store the line
            strcpy(function_lines[line_count++], line);
        }
    }
    
    fclose(list_file);
    
    // Print the function context
    printf("████████████████████████████████████████████████████████\n");
    
    if (pc_line_index >= 0) {
        // Print the entire function
        for (int i = 0; i < line_count; i++) {
            char *line_ptr = function_lines[i];
            
            // Check if this is an instruction line
            if (strlen(line_ptr) > 9 && line_ptr[8] == ':' && 
                isspace(line_ptr[0]) && isspace(line_ptr[1]) && isspace(line_ptr[2]) && isspace(line_ptr[3]) &&
                isxdigit(line_ptr[4]) && isxdigit(line_ptr[5]) && isxdigit(line_ptr[6]) && isxdigit(line_ptr[7])) {
                
                // Format: extract address and instruction text, skip raw bytes
                char addr[9];
                strncpy(addr, line_ptr + 4, 5);  // Copy "XXXX:"
                addr[5] = '\0';
                
                // Find the start of the instruction text (after the raw bytes)
                char *instr_text = line_ptr + 9;  // Start after the colon
                while (*instr_text && isspace(*instr_text)) instr_text++;  // Skip spaces
                while (*instr_text && !isspace(*instr_text)) instr_text++;  // Skip raw bytes
                while (*instr_text && isspace(*instr_text)) instr_text++;  // Skip spaces
                
                // Print formatted line
                if (i == pc_line_index) {
                    printf("██=>%s %s", addr, instr_text);  // Mark the PC line
                } else {
                    printf("██\033[47m\033[0;30m  %s %s", addr, instr_text);
                    printf("\033[0m");
                }
            } else {
                // For non-instruction lines (like function headers), print as is
                if (strlen(line_ptr) > 0) {
                    printf("██%s", line_ptr);
                }
            }
        }
    } else {
        printf("PC 0x%08x not found in function '%s'\n", pc, function_name);
    }
    
    // printf("----------------------------------------\n\n");

    char buf[1024];
    int offset = 0;
    offset += sprintf(buf + offset, "████████████████████████████████████████████████████████\n");
    for (int i = 0; i < 17; i++) {
        if (i <= 12) {
            if (!db->changed_regs[i]) {
                offset += sprintf(buf + offset, "██ r%-2d : \033[47m\033[0;30m%11d  0x%08x\033[0m\n", i, db->regs[i], db->regs[i]);
            } else {
                offset += sprintf(buf + offset, "██ r%-2d : %11d  0x%08x\n", i, db->regs[i], db->regs[i]);
            }
        } else if (i == 13) {
            if (!db->changed_regs[i]) {
                offset += sprintf(buf + offset, "██ sp  : \033[47m\033[0;30m%11d  0x%08x\033[0m\n", db->regs[i], db->regs[i]);
            } else {
                offset += sprintf(buf + offset, "██ sp  : %11d  0x%08x\n", db->regs[i], db->regs[i]);
            }
        } else if (i == 14) {
            if (!db->changed_regs[i]) {
                offset += sprintf(buf + offset, "██ lr  : \033[47m\033[0;30m%11d  0x%08x\033[0m\n", db->regs[i], db->regs[i]);
            } else {
                offset += sprintf(buf + offset, "██ lr  : %11d  0x%08x\n", db->regs[i], db->regs[i]);
            }
        } else if (i == 15) {
            if (!db->changed_regs[i]) {
                offset += sprintf(buf + offset, "██ pc  : \033[47m\033[0;30m%11d  0x%08x\033[0m\n", db->regs[i], db->regs[i]);
            } else {
                offset += sprintf(buf + offset, "██ pc  : %11d  0x%08x\n", db->regs[i], db->regs[i]);
            }
        } else if (i == 16) {
            if (!db->changed_regs[i]) {
                offset += sprintf(buf + offset, "██ cpsr: \033[47m\033[0;30m%11d  0x%08x\033[0m\n", db->regs[i], db->regs[i]);
            } else {
                offset += sprintf(buf + offset, "██ cpsr: %11d  0x%08x\n", db->regs[i], db->regs[i]);
            }
        }
    }
    offset += sprintf(buf + offset, "████████████████████████████████████████████████████████\n");

    //Print breakpoints, any that isn't 0x0
    int has_bps = 0;
    for (int i = 0; i < 5; i++) {
        if (db->breakpoints[i] != 0x0) {
            offset += sprintf(buf + offset, "██ bp%-2d: 0x%08x\n", i+1, db->breakpoints[i]);
            has_bps = 1;
        }
    }
    if (has_bps) {
        offset += sprintf(buf + offset, "████████████████████████████████████████████████████████\n");
    }

    //Print watchpoints, any that isn't 0xffffffff
    int has_wps = 0;
    for (int i = 0; i < 2; i++) {
        if (db->watchpoints[i] != 0xffffffff) {
            offset += sprintf(buf + offset, "██ wp%-2d: 0x%08x\n", i+1, db->watchpoints[i]);
            has_wps = 1;
        }
    }
    if (has_wps) {
        offset += sprintf(buf + offset, "████████████████████████████████████████████████████████\n");
    }


    printf("%s", buf);
    // printf("\033[10A");
    // printf("\033[K");
    // printf("asdf\n");
}


unsigned parse_debugger_state(debugger_state_t* db, char* buf) {
    //If string starts with "DS"
    char buf2[5] = "_D_S_";
    if (strncmp(buf, buf2, 5) != 0) {
        return 0;
    }
    // printf("UNIX: RECEIVED DEBUGGER STATE PACKET %u\n", buf[5]);
    if (buf[5] == 0) {
        buf += 8;
        //Unserialize
        for (int i = 0; i < 14; i++) {
            unsigned *reg_ptr = (unsigned*)buf;
            db->changed_regs[i] = (*reg_ptr != db->regs[i]);
            db->regs[i] = *reg_ptr;
            buf += 4;
        }
        return 1;
    }

    if (buf[5] == 1) {
        buf += 8;
        //Unserialize
        for (int i = 14; i < 17; i++) {
            unsigned *reg_ptr = (unsigned*)buf;
            db->changed_regs[i] = (*reg_ptr != db->regs[i]);
            db->regs[i] = *reg_ptr;
            buf += 4;
        }
        return 1;
    }
    if (buf[5] == 2) {
        buf += 8;
        //Unserialize
        for (int i = 0; i < 5; i++) {
            db->breakpoints[i] = *(int*)buf;
            buf += 4;
        }
        return 1;
    }
    if (buf[5] == 3) {
        buf += 8;
        //Unserialize
        for (int i = 0; i < 2; i++) {
            db->watchpoints[i] = *(int*)buf;
            buf += 4;
        }
        return 1;
    }
    if (buf[5] == 9) {
        //done
        print_debugger_state(db);
        return 1;
    }
    return 0;
}


int min(int a, int b) {
    return a < b ? a : b;
}

// hack-y state machine to indicate when we've seen the special string
// 'DONE!!!' from the pi telling us to shutdown.
int pi_done(unsigned char *s) {
    static unsigned pos = 0;
    const char exit_string[] = "DONE!!!\n";
    const int n = sizeof exit_string - 1;

    for(; *s; s++) {
        assert(pos < n);
        if(*s != exit_string[pos++]) {
            pos = 0;
            return pi_done(s+1); // check remainder
        }
        // maybe should check if "DONE!!!" is last thing printed?
        if(pos == sizeof exit_string - 1)
            return 1;
    }
    return 0;
}

// overwrite any unprintable characters with a space.
// otherwise terminals can go haywire/bizarro.
// note, the string can contain 0's, so we send the
// size.
void remove_nonprint(uint8_t *buf, int n) {
    for(int i = 0; i < n; i++) {
        uint8_t *p = &buf[i];
        if(isprint(*p) || (isspace(*p) && *p != '\r'))
            continue;
        *p = ' ';
    }
}

// read and echo the characters from the usbtty until it closes 
// (pi rebooted) or we see a string indicating a clean shutdown.
void pi_echo_debug(int unix_fd, int pi_fd, const char *portname, const char *_progname) {
    assert(pi_fd);
    progname = _progname;

    //Protocol:
    // Pi is debugging and gets to a stall state where it needs user input.
    // 1. Pi->host "GET_USER_INPUT" and a package of all debugger state data
    // 2. host prints all the debugger state data, then prints a pidb> prompt
    // 3. user types their input
    // 4. host processes input
        //* if user types b 0x1234, host tells pi to add bp at 0x1234
        // c: host tells pi to continue without single stepping
        // s/n: single step one step
        //* q: exit
        //* w 0x1234: watchpoint at 0x1234
        //* r[1-15] = [value]: write value to register
        // *[addr] = [value]: write value to memory
        // *addr: read value from memory
    // 5. once host figures out what it wants pi to do, sends corresponding command back to pi which is waiting
    // 6. pi executes command, then runs until it next needs input (while keeping track of debugger state)

    // Every message has following format: byte 1 is command, bytes 2-5 are length, bytes 6-end are message

    // Debugger state has following values:
    // registers 1-16
    // Backtrace?
    // watchpoints
    // breakpoints

    // Additional state that the laptop will have access to:
    // program assembly
    // program code
    // Can use the value of pc (r15) to figure out which function we're in and get that function from program code if it exists
    // Display register values as hex, binary, decimal, and ascii
    debugger_state_t db;
    while(1) {
        char buf[4096];

        int n;

        wait_and_send_input(unix_fd, pi_fd);

        if(!can_read_timeout(pi_fd, 100))
            continue;
        n = read(pi_fd, buf, sizeof buf - 1);
        if(!n) {
            // this isn't the program's fault.  so we exit(0).
            if(!portname || tty_gone(portname))
                clean_exit("pi ttyusb connection closed.  cleaning up\n");
            // so we don't keep banging on the CPU.
            usleep(1000);
        // } else if(n < 0) {
        //     sys_die(read, "pi connection closed.  cleaning up\n");
        } else if (parse_debugger_state(&db, buf)) {
            // do nothing, parse takes care of printing
        } else {
            buf[n] = 0;
            // if you keep getting "" "" "" it's b/c of the GET_CODE message from bootloader
            remove_nonprint((unsigned char *) buf, n);
            output("%s ", buf);

            if(pi_done((unsigned char *) buf)) {
                // output("\nSaw done\n");
                clean_exit("\nbootloader: pi exited.  cleaning up\n");
            }
        }
    }
    notreached();
}
