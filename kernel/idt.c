#include "idt.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static struct {
  gdt_entry_t null_desc;
  gdt_entry_t kcode;
  gdt_entry_t kdata;
  gdt_entry_t udata;
  gdt_entry_t ucode;
  gdt_tss_entry_t tss_desc;
} __attribute__((packed)) gdt;

static gdtr_t gdtr;
static tss_entry_t tss;

static idt_entry_t idt[256];
static idtr_t idtr;

static unsigned char kernel_tss_stack[8192] __attribute__((aligned(16)));

void gdt_init(void) {
  // Null Descriptor
  gdt.null_desc = (gdt_entry_t){0, 0, 0, 0, 0, 0};

  gdt.kcode = (gdt_entry_t){
      .limit_low = 0x0000,
      .base_low = 0x0000,
      .base_middle = 0x00,
      .access = 0x9A,      // Present, Ring 0, Code, Executable, Readable
      .granularity = 0x20, // Long Mode (64-bit)
      .base_high = 0x00};

  gdt.kdata = (gdt_entry_t){.limit_low = 0x0000,
                            .base_low = 0x0000,
                            .base_middle = 0x00,
                            .access = 0x92, // Present, Ring 0, Data, Writable
                            .granularity = 0x00,
                            .base_high = 0x00};

  gdt.udata = (gdt_entry_t){.limit_low = 0x0000,
                            .base_low = 0x0000,
                            .base_middle = 0x00,
                            .access = 0xF2, // Present, Ring 3, Data, Writable
                            .granularity = 0x00,
                            .base_high = 0x00};

  gdt.ucode = (gdt_entry_t){
      .limit_low = 0x0000,
      .base_low = 0x0000,
      .base_middle = 0x00,
      .access = 0xFA,      // Present, Ring 3, Code, Executable, Readable
      .granularity = 0x20, // Long Mode (64-bit)
      .base_high = 0x00};

  for (int i = 0; i < (int)sizeof(tss_entry_t); i++) {
    ((char *)&tss)[i] = 0;
  }

  tss.rsp0 = (unsigned long long)&kernel_tss_stack[sizeof(kernel_tss_stack)];
  tss.iomap_base = sizeof(tss_entry_t);

  unsigned long long tss_addr = (unsigned long long)&tss;
  unsigned int tss_size = sizeof(tss_entry_t) - 1;

  gdt.tss_desc.low.limit_low = (unsigned short)(tss_size & 0xFFFF);
  gdt.tss_desc.low.base_low = (unsigned short)(tss_addr & 0xFFFF);
  gdt.tss_desc.low.base_middle = (unsigned char)((tss_addr >> 16) & 0xFF);
  gdt.tss_desc.low.access = 0x89; // Present, Ring 0, Available 64-bit TSS
  gdt.tss_desc.low.granularity = (unsigned char)((tss_size >> 16) & 0x0F);
  gdt.tss_desc.low.base_high = (unsigned char)((tss_addr >> 24) & 0xFF);
  gdt.tss_desc.base_upper = (unsigned int)(tss_addr >> 32);
  gdt.tss_desc.reserved = 0;

  gdtr.limit = sizeof(gdt) - 1;
  gdtr.base = (unsigned long long)&gdt;
  __asm__ __volatile__("lgdt %0" : : "m"(gdtr));

  // LTR은 16비트 메모리 피연산자를 받을 수 있다. 레지스터 clobber를
  // 사용하지 않아 Clang과 IDE의 인라인 어셈블리 검사 모두에 호환된다.
  const unsigned short tss_selector = 0x28;
  __asm__ __volatile__("ltr %0" : : "m"(tss_selector) : "memory");

  kputs("[GDT/TSS] EXTENDED GDT & TSS LOADED (RSP0 SET)\n", 0x00FFFF00);
}

__attribute__((interrupt)) void
default_exception_handler(struct interrupt_frame *frame) {
  kputs("\n[IDT] EXCEPTION / INTERRUPT OCCURRED!\n", 0x00FF0000); // Red
  kputs("RIP: ", 0x00FF0000);
  kput_hex(frame->ip, 0x00FF0000);
  kputs("\n", 0x00FF0000);

  while (1) {
    __asm__ __volatile__("hlt");
  }
}

__attribute__((interrupt)) void
double_fault_handler(struct interrupt_frame *frame,
                     unsigned long long error_code) {
  kputs("\n[IDT] TRAP: DOUBLE FAULT OCCURRED! ERROR CODE: ", 0x00FF0000);
  kput_hex(error_code, 0x00FF0000);
  kputs("\n", 0x00FF0000);

  while (1) {
    __asm__ __volatile__("hlt");
  }
}

// Page fault는 error code를 포함하므로 일반 예외 핸들러와 다른 ABI를 사용한다.
// 현재는 복구/스케줄링 경로가 없으므로 사용자·커널 fault 모두 안전하게 정지한다.
__attribute__((interrupt)) void
page_fault_handler(struct interrupt_frame *frame,
                   unsigned long long error_code) {
  (void)frame;
  (void)error_code;
  while (1) {
    __asm__ __volatile__("hlt");
  }
}

// IDT 엔트리 설정 함수
void idt_set_gate(unsigned char vector, void *handler, unsigned char flags) {
  unsigned long long addr = (unsigned long long)handler;

  idt[vector].offset_low = (unsigned short)(addr & 0xFFFF);
  idt[vector].selector = 0x08; // Kernel Code Segment Selector
  idt[vector].ist = 0;
  idt[vector].type_attributes = flags;
  idt[vector].offset_mid = (unsigned short)((addr >> 16) & 0xFFFF);
  idt[vector].offset_high = (unsigned int)((addr >> 32) & 0xFFFFFFFF);
  idt[vector].zero = 0;
}

void idt_init(void) {
  idtr.limit = sizeof(idt) - 1;
  idtr.base = (unsigned long long)&idt;

  // 0~255번 인터럽트 엔트리를 기본 핸들러로 설정
  for (int i = 0; i < 256; i++) {
    idt_set_gate(i, (void *)default_exception_handler, 0x8E);
  }

  // Double Fault (8번) 핸들러 등록
  idt_set_gate(8, (void *)double_fault_handler, 0x8E);
  idt_set_gate(14, (void *)page_fault_handler, 0x8E);
  // IDTR 로드
  __asm__ __volatile__("lidt %0" : : "m"(idtr));
  kputs("[IDT] INTERRUPT DESCRIPTOR TABLE LOADED\n", 0x00FFFF00);
}
