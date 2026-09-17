#include "gdt.h"

static struct {
  gdt_entry_t null_desc;
  gdt_entry_t kcode;
  gdt_entry_t kdata;
  gdt_entry_t udata;
  gdt_entry_t ucode;
  gdt_tss_entry_t tss_desc;
} __attribute__((packed)) gdt_table;

static tss_entry_t sys_tss;

typedef struct __attribute__((packed)) {
  uint16_t limit;
  uint64_t base;
} gdtr_t;

void gdt_tss_init(void) {
  gdt_table.null_desc = (gdt_entry_t){0, 0, 0, 0, 0, 0};
  gdt_table.kcode =
      (gdt_entry_t){0xFFFF, 0, 0, 0x9A, 0xAF, 0}; // 64-bit Code (DPL 0)
  gdt_table.kdata = (gdt_entry_t){0xFFFF, 0, 0, 0x92, 0xCF, 0}; // Data (DPL 0)
  gdt_table.udata = (gdt_entry_t){0xFFFF, 0, 0, 0xF2, 0xCF, 0}; // Data (DPL 3)
  gdt_table.ucode = (gdt_entry_t){0xFFFF, 0, 0, 0xFA, 0xAF, 0};

  uint64_t tss_base = (uint64_t)&sys_tss;
  uint32_t tss_size = sizeof(tss_entry_t) - 1;

  sys_tss.iomap_base = sizeof(tss_entry_t);

  gdt_table.tss_desc.limit_low = tss_size & 0xFFFF;
  gdt_table.tss_desc.base_low = tss_base & 0xFFFF;
  gdt_table.tss_desc.base_middle = (tss_base >> 16) & 0xFF;
  gdt_table.tss_desc.access = 0x89; // Present, Executable, Access (TSS)
  gdt_table.tss_desc.granularity = (tss_size >> 16) & 0x0F;
  gdt_table.tss_desc.base_high = (tss_base >> 24) & 0xFF;
  gdt_table.tss_desc.base_upper = (tss_base >> 32) & 0xFFFFFFFF;

  gdtr_t gdtr = {.limit = sizeof(gdt_table) - 1, .base = (uint64_t)&gdt_table};

  __asm__ __volatile__("lgdt %0\n\t"
                       "pushq $0x08\n\t"
                       "leaq 1f(%%rip), %%rax\n\t"
                       "pushq %%rax\n\t"
                       "lretq\n\t"
                       "1:\n\t"
                       "mov $0x10, %%ax\n\t"
                       "mov %%ax, %%ds\n\t"
                       "mov %%ax, %%es\n\t"
                       "mov %%ax, %%fs\n\t"
                       "mov %%ax, %%gs\n\t"
                       "mov %%ax, %%ss\n\t"
                       "mov $0x28, %%ax\n\t"
                       "ltr %%ax\n\t"
                       :
                       : "m"(gdtr)
                       : "rax", "memory");
}

void tss_set_kernel_stack(uint64_t stack_ptr) { sys_tss.rsp0 = stack_ptr; }
