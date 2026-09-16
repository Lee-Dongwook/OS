#include "pmm.h"
#include "vmm.h"
#include "virtio_blk.h"
#include "fat32.h"
#include "macho.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);
extern void enter_userland(unsigned long long entry_point,
                           unsigned long long user_stack);
extern page_table_t *kernel_pml4;

void kernel_main(BootInfo *boot_info) {
    console_init(boot_info);
    kputs("XNU OS KERNEL INIT...\n\n", 0x0000FF00);

    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);
    vmm_init(boot_info);
    gdt_init();
    idt_init();

    virtio_blk_init();

    if (fat32_init() == 0) {
      fat32_list_root();

      unsigned long long user_entry = macho_load_from_fat32("USERAPP");

      if (user_entry != 0) {
            kputs("[KERNEL] JUMPING TO MACH-O USERLAND AT: ", 0x0000FF00);
            kput_hex(user_entry, 0x0000FF00);
            kputs("\n\n", 0x0000FF00);

            // 유저 스택 영역 할당 및 매핑 (0x7FFFF0000000)
            void *user_stack_page = pmm_alloc_page();
            unsigned long long user_stack_top = 0x7FFFF0000000;
            vmm_map_page(kernel_pml4, user_stack_top - PAGE_SIZE, (unsigned long long)user_stack_page, 0x07);

            enter_userland(user_entry, user_stack_top);
        }
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
