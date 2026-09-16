#include "fat32.h"
#include "macho.h"
#include "pmm.h"
#include "virtio_blk.h"
#include "vmm.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);
extern void enter_user_mode(unsigned long long entry_point, unsigned long long user_stack);
extern page_table_t *kernel_pml4;

void kernel_main(BootInfo *boot_info) {
    console_init(boot_info);
    kputs("XNU OS KERNEL INIT...\n\n", 0x0000FF00);

    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);
    vmm_init(boot_info);
    gdt_init();
    idt_init();

    kputs("[DEBUG] VIRTIO BLK INIT START...\n", 0x00FFFF00);
    virtio_blk_init();
    kputs("[DEBUG] VIRTIO BLK INIT DONE!\n", 0x0000FF00);

    kputs("[DEBUG] FAT32 INIT START...\n", 0x00FFFF00);
    if (fat32_init() == 0) {
        kputs("[DEBUG] FAT32 INIT SUCCESS!\n", 0x0000FF00);
        fat32_list_root();

        unsigned long long user_entry = macho_load_from_fat32("USERAPP");
        if (user_entry != 0) {
            kputs("\n[KERNEL] JUMPING TO USERLAND AT: ", 0x0000FF00);
            kput_hex(user_entry, 0x0000FF00);
            kputs("\n", 0x0000FF00);

            unsigned long long user_stack_top = 0x7FFFF0000000;
            void *user_stack_page = pmm_alloc_page();
            vmm_map_page(kernel_pml4, user_stack_top - PAGE_SIZE, (unsigned long long)user_stack_page, 0x07);

            enter_user_mode(user_entry, user_stack_top);
        }
    } else {
        kputs("[DEBUG] FAT32 INIT FAILED!\n", 0x00FF0000);
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
