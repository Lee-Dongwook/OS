#include "apic.h"

#define LAPIC_BASE            0xFEE00000ULL
#define LAPIC_EOI             0x0B0
#define LAPIC_SPURIOUS        0x0F0
#define LAPIC_TIMER           0x320
#define LAPIC_TIMER_INITCNT   0x380
#define LAPIC_TIMER_DIV       0x3E0

extern void kputs(const char *str, unsigned int color);

// LAPIC 레지스터 읽기/쓰기
static inline unsigned int lapic_read(unsigned int reg) {
    return *(volatile unsigned int *)(LAPIC_BASE + reg);
}

static inline void lapic_write(unsigned int reg, unsigned int value) {
    *(volatile unsigned int *)(LAPIC_BASE + reg) = value;
}

void apic_send_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

void apic_init(void) {
    // 1. Spurious Interrupt Vector Register 활성화 (0x100 = Enable)
    lapic_write(LAPIC_SPURIOUS, lapic_read(LAPIC_SPURIOUS) | 0x100 | 0xFF);

    // 2. 타이머 분주비(Divide Ratio) 설정: 16
    lapic_write(LAPIC_TIMER_DIV, 0x3);

    // 3. LAPIC 타이머 설정: Vector 32, Periodic Mode (0x20000 | 32)
    lapic_write(LAPIC_TIMER, 0x20000 | 32);

    // 4. 타이머 카운트 시작 (주기적 인터럽트 발생)
    lapic_write(LAPIC_TIMER_INITCNT, 10000000);

    kputs("[APIC] LOCAL APIC TIMER INITIALIZED\n", 0x00FFFF00);
}
