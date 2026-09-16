#include "idt.h"
#include "scheduler.h"

extern void apic_send_eoi(void);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static gdt_entry_t gdt[3];
static gdtr_t gdtr;

static idt_entry_t idt[256];
static idtr_t idtr;

// Vector 32 (타이머) 핸들러
__attribute__((interrupt))
void timer_interrupt_handler(struct interrupt_frame *frame) {
    context_t *ctx = (context_t *)frame;
    
    // EOI 전송 (인터럽트 처리 완료 알림)
    apic_send_eoi();

    // 스케줄러 호출
    schedule(ctx);
}

void gdt_init(void) {
    // Null Descriptor
    gdt[0] = (gdt_entry_t){0, 0, 0, 0, 0, 0};

    // Kernel Code Segment (0x08)
    gdt[1].limit_low   = 0x0000;
    gdt[1].base_low    = 0x0000;
    gdt[1].base_middle = 0x00;
    gdt[1].access      = 0x9A; // Present, Ring 0, Code, Executable, Readable
    gdt[1].granularity = 0x20; // Long Mode (64-bit)
    gdt[1].base_high   = 0x00;

    // Kernel Data Segment (0x10)
    gdt[2].limit_low   = 0x0000;
    gdt[2].base_low    = 0x0000;
    gdt[2].base_middle = 0x00;
    gdt[2].access      = 0x92; // Present, Ring 0, Data, Writable
    gdt[2].granularity = 0x00;
    gdt[2].base_high   = 0x00;

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (unsigned long long)&gdt;

    __asm__ __volatile__("lgdt %0" : : "m"(gdtr));
    kputs("[GDT] GLOBAL DESCRIPTOR TABLE LOADED\n", 0x00FFFF00);
}

__attribute__((interrupt))
void default_exception_handler(struct interrupt_frame *frame) {
    kputs("\n[IDT] EXCEPTION / INTERRUPT OCCURRED!\n", 0x00FF0000); // Red
    kputs("RIP: ", 0x00FF0000);
    kput_hex(frame->ip, 0x00FF0000);
    kputs("\n", 0x00FF0000);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}

__attribute__((interrupt))
void double_fault_handler(struct interrupt_frame *frame, unsigned long long error_code) {
    kputs("\n[IDT] TRAP: DOUBLE FAULT OCCURRED! ERROR CODE: ", 0x00FF0000);
    kput_hex(error_code, 0x00FF0000);
    kputs("\n", 0x00FF0000);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}

// IDT 엔트리 설정 함수
void idt_set_gate(unsigned char vector, void *handler, unsigned char flags) {
    unsigned long long addr = (unsigned long long)handler;

    idt[vector].offset_low       = (unsigned short)(addr & 0xFFFF);
    idt[vector].selector         = 0x08; // Kernel Code Segment Selector
    idt[vector].ist              = 0;
    idt[vector].type_attributes  = flags;
    idt[vector].offset_mid       = (unsigned short)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high      = (unsigned int)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].zero             = 0;
}

void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (unsigned long long)&idt;

    // 0~255번 인터럽트 엔트리를 기본 핸들러로 설정
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (void *)default_exception_handler, 0x8E);
    }

    // Double Fault (8번) 핸들러 등록
    idt_set_gate(8, (void *)double_fault_handler, 0x8E);

    idt_set_gate(32, (void *)timer_interrupt_handler, 0x8E);

    // IDTR 로드
    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    kputs("[IDT] INTERRUPT DESCRIPTOR TABLE LOADED\n", 0x00FFFF00);
}
