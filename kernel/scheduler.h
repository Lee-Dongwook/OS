#ifndef SCHEDULER_H
#define SCHEDULER_H

// 스레드 CPU 문맥 레지스터 (x86_64)
typedef struct {
    unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long long rbp, rdi, rsi, rdx, rcx, rbx, rax;
    unsigned long long rip;
    unsigned long long cs;
    unsigned long long rflags;
    unsigned long long rsp;
    unsigned long long ss;
} context_t;

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    int id;
    thread_state_t state;
    unsigned long long rsp; // 저장된 스택 포인터
    unsigned char stack[4096]; // 4KB 스레드 스택
} thread_t;

void scheduler_init(void);
void thread_create(void (*entry_point)(void));
unsigned long long schedule(unsigned long long current_rsp);

#endif
