#ifndef IDT_H
#define IDT_H

typedef struct {
  unsigned int reserved0;
  unsigned long long rsp0;
  unsigned long long rsp1;
  unsigned long long rsp2;
  unsigned long long reserved1;
  unsigned long long ist[7];
  unsigned long long reserved2;
  unsigned short reserved3;
  unsigned short iomap_base;
} __attribute__((packed)) tss_entry_t;

typedef struct {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_middle;
    unsigned char  access;
    unsigned char  granularity;
    unsigned char  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    gdt_entry_t low;
    unsigned int base_upper;
    unsigned int reserved;
} __attribute__((packed)) gdt_tss_entry_t;

typedef struct {
    unsigned short limit;
    unsigned long long base;
} __attribute__((packed)) gdtr_t;

// IDT 엔트리 구조체 (x86_64는 16바이트)
typedef struct {
    unsigned short offset_low;   // Offset bits 0..15
    unsigned short selector;     // Selector (Code Segment)
    unsigned char  ist;          // Interrupt Stack Table offset
    unsigned char  type_attributes; // Gate Type, DPL, Present
    unsigned short offset_mid;   // Offset bits 16..31
    unsigned int   offset_high;  // Offset bits 32..63
    unsigned int   zero;         // Reserved
} __attribute__((packed)) idt_entry_t;

typedef struct {
    unsigned short limit;
    unsigned long long base;
} __attribute__((packed)) idtr_t;

struct interrupt_frame {
    unsigned long long ip;
    unsigned long long cs;
    unsigned long long flags;
    unsigned long long sp;
    unsigned long long ss;
};

void gdt_init(void);
void idt_init(void);

#endif
