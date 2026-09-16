#include "shell.h"
#include "initramfs.h"
#include "pmm.h"
#include "scheduler.h"
#include "task.h"
#include "mach_ipc.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);
extern void console_clear(void);

#define SHELL_LINE_MAX 64
#define SHELL_COLOR 0x00FFFFFF
#define SUCCESS_COLOR 0x0000FF00
#define ERROR_COLOR 0x00FF0000

static char line[SHELL_LINE_MAX];
static unsigned int line_length;

static int strings_equal(const char *left, const char *right) {
    while (*left && *right) {
        if (*left++ != *right++) {
            return 0;
        }
    }
    return *left == *right;
}

static void print_prompt(void) {
    kputs("os> ", SHELL_COLOR);
}

static void print_memory(unsigned long long bytes) {
    kput_dec(bytes / 1024 / 1024, SHELL_COLOR);
    kputs(" MB", SHELL_COLOR);
}

static void execute_command(void) {
    if (line_length == 0) {
        return;
    }

    if (strings_equal(line, "help")) {
        kputs("HELP CLEAR MEM TASKS PORTS SPAWN LS CAT <NAME>\n", SHELL_COLOR);
    } else if (strings_equal(line, "clear")) {
        console_clear();
    } else if (strings_equal(line, "mem")) {
        kputs("MEMORY TOTAL: ", SHELL_COLOR);
        print_memory(pmm_total_usable_memory());
        kputs(" FREE: ", SHELL_COLOR);
        print_memory(pmm_free_memory());
        kputs("\n", SHELL_COLOR);
    } else if (strings_equal(line, "tasks")) {
        kputs("ACTIVE TASKS: ", SHELL_COLOR);
        kput_dec(task_active_count(), SHELL_COLOR);
        kputs(" KERNEL THREADS: ", SHELL_COLOR);
        kput_dec(scheduler_thread_count(), SHELL_COLOR);
        kputs("\n", SHELL_COLOR);
    } else if (strings_equal(line, "ports")) {
        kputs("ACTIVE PORTS: ", SHELL_COLOR);
        kput_dec(mach_port_active_count(), SHELL_COLOR);
        kputs("\n", SHELL_COLOR);
    } else if (strings_equal(line, "spawn")) {
        task_t *task = task_create();
        unsigned int port = mach_port_allocate();
        if (!task || port == 0 || task_add_port(task, port) != 0) {
            if (port) {
                mach_port_destroy(port);
            }
            if (task) {
                task_destroy(task);
            }
            kputs("TASK CREATION FAILED\n", ERROR_COLOR);
        } else {
            kputs("TASK CREATED WITH PORT: ", SUCCESS_COLOR);
            kput_dec(port, SUCCESS_COLOR);
            kputs("\n", SUCCESS_COLOR);
        }
    } else if (strings_equal(line, "ls")) {
        for (unsigned int i = 0; i < initramfs_file_count(); i++) {
            kputs(initramfs_file_at(i)->name, SUCCESS_COLOR);
            kputs("\n", SHELL_COLOR);
        }
    } else if (line_length > 4 && line[0] == 'c' && line[1] == 'a' &&
               line[2] == 't' && line[3] == ' ') {
        const initramfs_file_t *file = initramfs_find(&line[4]);
        if (file) {
            kputs(file->contents, SHELL_COLOR);
        } else {
            kputs("FILE NOT FOUND\n", ERROR_COLOR);
        }
    } else {
        kputs("UNKNOWN COMMAND: ", ERROR_COLOR);
        kputs(line, ERROR_COLOR);
        kputs("\n", ERROR_COLOR);
    }
}

void shell_init(void) {
    line_length = 0;
    kputs("[SHELL] TYPE HELP FOR COMMANDS\n", SUCCESS_COLOR);
    print_prompt();
}

void shell_handle_char(char c) {
    if (c == '\b') {
        if (line_length > 0) {
            line_length--;
            line[line_length] = '\0';
            kputs("\b \b", SHELL_COLOR);
        }
        return;
    }
    if (c == '\n') {
        kputs("\n", SHELL_COLOR);
        line[line_length] = '\0';
        execute_command();
        line_length = 0;
        print_prompt();
        return;
    }
    if (c >= ' ' && c <= '~' && line_length + 1 < SHELL_LINE_MAX) {
        line[line_length++] = c;
        char output[2] = {c, '\0'};
        kputs(output, SHELL_COLOR);
    }
}
