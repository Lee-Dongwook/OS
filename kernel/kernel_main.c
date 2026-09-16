#include "vmm.h"
#include "mach_ipc.h"
#include "scheduler.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);
extern void apic_init(void);

void thread_a(void) {
    while (1) {
        kputs("[THREAD A] RUNNING...\n", 0x00FF00FF);
        for (volatile int i = 0; i < 50000000; i++);
    }
}

void thread_b(void) {
    while (1) {
        kputs("[THREAD B] RUNNING...\n", 0x00FFFF00);
        for (volatile int i = 0; i < 50000000; i++);
    }
}

void kernel_main(BootInfo *boot_info) {
    // 1. 화면 초기화
    unsigned int *fb = boot_info->framebuffer;
    unsigned int stride = boot_info->pixels_per_scan_line;

    for (unsigned int y = 0; y < boot_info->height; y++) {
        for (unsigned int x = 0; x < boot_info->width; x++) {
            fb[y * stride + x] = 0x00000000;
        }
    }

    // 2. 서브시스템 초기화
    console_init(boot_info);
    kputs("XNU OS KERNEL INIT...\n\n", 0x0000FF00);

    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);
    vmm_init(boot_info);
    gdt_init();
    idt_init();
    mach_ipc_init();

    // 3. 스케줄러 초기화 & 스레드 생성
    scheduler_init();
    thread_create(thread_a);
    thread_create(thread_b);
    apic_init();

    kputs("\n[KERNEL] ENABLING INTERRUPTS & MULTITASKING...\n", 0x00FFFFFF);

    __asm__ __volatile__("sti");

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
