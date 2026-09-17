#include "fat32.h"
#include "idt.h"
#include "macho.h"
#include "pmm.h"
#include "task.h"
#include "virtio_blk.h"
#include "vmm.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void enter_user_mode(unsigned long long entry_point,
                            unsigned long long user_stack,
                            page_table_t *user_pml4);

enum { LOG_INFO = 0x0000FF00, LOG_WARN = 0x00FFFF00, LOG_ERROR = 0x00FF0000 };

static __attribute__((noreturn)) void halt_forever(void) {
  while (1) {
    __asm__ __volatile__("hlt");
  }
}

static __attribute__((noreturn)) void boot_failed(const char *message) {
  kputs("[KERNEL] ERROR: ", LOG_ERROR);
  kputs(message, LOG_ERROR);
  kputs("\n", LOG_ERROR);
  halt_forever();
}

static void initialize_kernel(BootInfo *boot_info) {
  console_init(boot_info);
  kputs("XNU OS KERNEL INIT...\n\n", LOG_INFO);

  pmm_init(boot_info->memory_map, boot_info->memory_map_size,
           boot_info->descriptor_size);
  vmm_init(boot_info);
  gdt_init();
  idt_init();
  task_init();
}

static void initialize_storage(void) {
  kputs("[BOOT] VIRTIO BLK INIT START...\n", LOG_WARN);
  virtio_blk_init();
  kputs("[BOOT] VIRTIO BLK INIT DONE\n", LOG_INFO);

  kputs("[BOOT] FAT32 INIT START...\n", LOG_WARN);
  if (fat32_init() != 0) {
    boot_failed("FAT32 INITIALIZATION FAILED");
  }
  kputs("[BOOT] FAT32 INIT DONE\n", LOG_INFO);
  fat32_list_root();
}

static __attribute__((noreturn)) void start_user_program(void) {
  task_t *user_task = task_create();
  if (!user_task) {
    boot_failed("FAILED TO CREATE USER TASK");
  }

  unsigned long long user_entry =
      macho_load_from_fat32("USERAPP", user_task->pml4);
  if (user_entry == 0) {
    boot_failed("FAILED TO LOAD USERAPP");
  }

  unsigned long long user_stack_top = 0x7FFFF0000000;
  void *user_stack_page = pmm_alloc_page();
  if (!user_stack_page) {
    boot_failed("FAILED TO ALLOCATE USER STACK");
  }
  vmm_map_page(user_task->pml4, user_stack_top - PAGE_SIZE,
               (unsigned long long)user_stack_page,
               PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  if (!vmm_user_mapping_is_private(user_task->pml4, user_entry)) {
    boot_failed("USER VSPACE IS NOT PRIVATE");
  }
  kputs("[VMM] USER VSPACE ISOLATION VERIFIED\n", LOG_INFO);

  kputs("\n[KERNEL] JUMPING TO USERLAND AT: ", LOG_INFO);
  kput_hex(user_entry, LOG_INFO);
  kputs("\n", LOG_INFO);
  enter_user_mode(user_entry, user_stack_top, user_task->pml4);
  boot_failed("USER PROGRAM RETURNED UNEXPECTEDLY");
}

void kernel_main(BootInfo *boot_info) {
  initialize_kernel(boot_info);
  initialize_storage();
  start_user_program();
}
