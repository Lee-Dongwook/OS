#include "fat32.h"
#include "macho.h"
#include "pmm.h"
#include "task.h"
#include "virtio_blk.h"
#include "vmm.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);
extern void enter_user_mode(unsigned long long entry_point, unsigned long long user_stack,
                            page_table_t *user_pml4);

__attribute__((noreturn)) void user_fault_recovery(void) {
    // iretq가 Ring 0 trampoline으로 돌아온 뒤에는 즉시 전역 커널 VSpace로
    // 복귀한다. fault를 일으킨 Task의 페이지 테이블을 계속 사용하지 않는다.
    extern page_table_t *kernel_pml4;
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(kernel_pml4) : "memory");
    __asm__ __volatile__("mov $0x10, %%ax\n\t"
                         "mov %%ax, %%ds\n\t"
                         "mov %%ax, %%es"
                         :
                         :
                         : "rax", "memory");
    kputs("[KERNEL] USER FAULT RECOVERED TO KERNEL\n", 0x0000FF00);
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

void kernel_main(BootInfo *boot_info) {
    console_init(boot_info);
    kputs("XNU OS KERNEL INIT...\n\n", 0x0000FF00);

    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);
    vmm_init(boot_info);
    gdt_init();
    idt_init();
    task_init();

    kputs("[DEBUG] VIRTIO BLK INIT START...\n", 0x00FFFF00);
    virtio_blk_init();
    kputs("[DEBUG] VIRTIO BLK INIT DONE!\n", 0x0000FF00);

    kputs("[DEBUG] FAT32 INIT START...\n", 0x00FFFF00);
    if (fat32_init() == 0) {
        kputs("[DEBUG] FAT32 INIT SUCCESS!\n", 0x0000FF00);
        fat32_list_root();

        task_t *user_task = task_create();
        if (!user_task) {
            kputs("[KERNEL] ERROR: FAILED TO CREATE USER TASK\n", 0x00FF0000);
            while (1) {
                __asm__ __volatile__("hlt");
            }
        }

        unsigned long long user_entry = macho_load_from_fat32("USERAPP", user_task->pml4);
        if (user_entry != 0) {
            kputs("\n[KERNEL] JUMPING TO USERLAND AT: ", 0x0000FF00);
            kput_hex(user_entry, 0x0000FF00);
            kputs("\n", 0x0000FF00);

            unsigned long long user_stack_top = 0x7FFFF0000000;
            void *user_stack_page = pmm_alloc_page();
            if (!user_stack_page) {
                kputs("[KERNEL] ERROR: FAILED TO ALLOCATE USER STACK\n", 0x00FF0000);
                while (1) {
                    __asm__ __volatile__("hlt");
                }
            }
            vmm_map_page(user_task->pml4, user_stack_top - PAGE_SIZE,
                         (unsigned long long)user_stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            if (!vmm_user_mapping_is_private(user_task->pml4, user_entry)) {
                kputs("[KERNEL] ERROR: USER VSPACE IS NOT PRIVATE\n", 0x00FF0000);
                while (1) {
                    __asm__ __volatile__("hlt");
                }
            }
            kputs("[VMM] USER VSPACE ISOLATION VERIFIED\n", 0x0000FF00);

            enter_user_mode(user_entry, user_stack_top, user_task->pml4);
        }
    } else {
        kputs("[DEBUG] FAT32 INIT FAILED!\n", 0x00FF0000);
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
