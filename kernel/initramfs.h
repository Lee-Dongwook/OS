#ifndef INITRAMFS_H
#define INITRAMFS_H

typedef struct {
    const char *name;
    const char *contents;
} initramfs_file_t;

unsigned int initramfs_file_count(void);
void initramfs_init(const char *boot_file_data, unsigned long long boot_file_size);
const initramfs_file_t *initramfs_file_at(unsigned int index);
const initramfs_file_t *initramfs_find(const char *name);

#endif
