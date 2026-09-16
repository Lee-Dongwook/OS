#include "vmm.h"
#include "virtio_blk.h"
#include "fat32.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);

void kernel_main(BootInfo *boot_info) {
    console_init(boot_info);
    kputs("XNU OS KERNEL INIT...\n\n", 0x0000FF00);

    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);
    vmm_init(boot_info);
    gdt_init();
    idt_init();

    virtio_blk_init();

    unsigned char sector_buf[512];
    if (virtio_blk_read(0, sector_buf) == 0) {
        kputs("[TEST] READ SECTOR 0 SUCCESS! FIRST 2 BYTES: ", 0x0000FF00);
        kput_hex(sector_buf[0], 0x0000FF00);
        kputs(" ", 0x0000FF00);
        kput_hex(sector_buf[1], 0x0000FF00);
        kputs("\n", 0x0000FF00);
    } else {
        kputs("[TEST] READ SECTOR 0 FAILED!\n", 0x00FF0000);
    }

    if (fat32_init() == 0) {
        fat32_list_root();
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
