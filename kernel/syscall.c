#include "syscall.h"

extern void syscall_entry(void);
extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

// MSR 읽기/쓰기 도우미 함수
static inline void wrmsr(unsigned int msr, unsigned long long val) {
    unsigned int low = (unsigned int)val;
    unsigned int high = (unsigned int)(val >> 32);
    __asm__ __volatile__("push %%rax\n\t"
                         "push %%rcx\n\t"
                         "push %%rdx\n\t"
                         "movl %0, %%ecx\n\t"
                         "movl %1, %%eax\n\t"
                         "movl %2, %%edx\n\t"
                         "wrmsr\n\t"
                         "pop %%rdx\n\t"
                         "pop %%rcx\n\t"
                         "pop %%rax\n\t"
                         :
                         : "r"(msr), "r"(low), "r"(high)
                         : "memory");
}

static inline unsigned long long rdmsr(unsigned int msr) {
    unsigned int low, high;
    __asm__ __volatile__("push %%rcx\n\t"
                         "movl %2, %%ecx\n\t"
                         "rdmsr\n\t"
                         "movl %%eax, %0\n\t"
                         "movl %%edx, %1\n\t"
                         "pop %%rcx\n\t"
                         : "=r"(low), "=r"(high)
                         : "r"(msr)
                         : "memory");
    return ((unsigned long long)high << 32) | low;
}

// C 언어 시스템 콜 디스패처
unsigned long long do_syscall(unsigned long long sys_num, unsigned long long arg1) {
    kputs("[SYSCALL] HANDLED SYSTEM CALL ID: ", 0x0000FFFF);
    kput_dec(sys_num, 0x0000FFFF);
    kputs("\n", 0x0000FFFF);

    if (sys_num == 1) {
        kputs("[SYSCALL] MACH_MSG REQUEST RECEIVED!\n", 0x0000FF00);
        return 0;
    }
    return -1;
}

void syscall_init(void) {
    // EFER (0xC0000080) -> SCE (System Call Enable) 비트 0 설정
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | 1);

    // STAR: SYSCALL 커널 CS=0x08. SYSRET의 CS는 user_base+16이므로
    // user_base=0x13을 넣어 user CS(0x23), SS(0x1B)를 만들게 한다.
    unsigned long long star = ((unsigned long long)0x08 << 32) | ((unsigned long long)0x13 << 48);
    wrmsr(IA32_STAR, star);

    // LSTAR (0xC0000082) -> syscall 핸들러 진입점 등록
    wrmsr(IA32_LSTAR, (unsigned long long)syscall_entry);

    // FMASK (0xC0000084) -> IF(Interrupt Flag, 0x200) 클리어
    wrmsr(IA32_FMASK, 0x200);

    kputs("[SYSCALL] FAST SYSCALL (MSR LSTAR) INITIALIZED\n", 0x00FFFF00);
}
