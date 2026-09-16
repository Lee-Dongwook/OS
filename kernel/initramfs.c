#include "initramfs.h"

static const initramfs_file_t files[] = {
    {"README.TXT", "OS INITRAMFS\nUSE HELP FOR AVAILABLE COMMANDS.\n"},
    {"HELLO.TXT", "HELLO FROM THE KERNEL FILE INTERFACE.\n"},
    {"VERSION.TXT", "OS EDUCATIONAL KERNEL 0.2\n"},
};

static initramfs_file_t boot_file = {"BOOT.TXT", 0};

extern void kputs(const char *str, unsigned int color);

static char uppercase(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

static int strings_equal(const char *left, const char *right) {
    while (*left && *right) {
        if (uppercase(*left++) != uppercase(*right++)) {
            return 0;
        }
    }
    return *left == *right;
}

unsigned int initramfs_file_count(void) { return sizeof(files) / sizeof(files[0]) + (boot_file.contents ? 1 : 0); }

void initramfs_init(const char *boot_file_data, unsigned long long boot_file_size) {
    boot_file.contents = boot_file_data && *boot_file_data ? boot_file_data : 0;
    if (boot_file.contents) {
        kputs("[INITRAMFS] FAT BOOT FILE LOADED: ", 0x00FFFF00);
        // 파일 크기는 UEFI가 반환한 실제 읽기 크기이며, 빈 파일도 구분할 수 있다.
        extern void kput_dec(unsigned long long val, unsigned int color);
        kput_dec(boot_file_size, 0x00FFFF00);
        kputs(" BYTES\n", 0x00FFFF00);
    } else {
        kputs("[INITRAMFS] NO FAT BOOT FILE FOUND\n", 0x00FF0000);
    }
}

const initramfs_file_t *initramfs_file_at(unsigned int index) {
    unsigned int built_in_count = sizeof(files) / sizeof(files[0]);
    if (index < built_in_count) {
        return &files[index];
    }
    if (index == built_in_count && boot_file.contents) {
        return &boot_file;
    }
    if (index >= initramfs_file_count()) {
        return 0;
    }
    return 0;
}

const initramfs_file_t *initramfs_find(const char *name) {
    for (unsigned int i = 0; i < initramfs_file_count(); i++) {
        const initramfs_file_t *file = initramfs_file_at(i);
        if (strings_equal(file->name, name)) {
            return file;
        }
    }
    return 0;
}
