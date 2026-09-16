#include "scheduler.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

#define MAX_THREADS 4

static thread_t threads[MAX_THREADS];
static int current_thread_index = -1;
static int thread_count = 0;

void thread_create(void (*entry_point)(void)) {
    if (thread_count >= MAX_THREADS) return;

    thread_t *t = &threads[thread_count];
    t->id = thread_count + 1;
    t->state = THREAD_READY;

    // 스택 최상단 설정 (16바이트 정렬)
    unsigned long long *sp = (unsigned long long *)&t->stack[4096];

    // 스택에 초기 context 구조 배치
    sp--; *sp = 0x10;             // SS (Kernel Data)
    sp--; *sp = (unsigned long long)&t->stack[4000]; // RSP
    sp--; *sp = 0x202;            // RFLAGS (Interrupt enabled)
    sp--; *sp = 0x08;             // CS (Kernel Code)
    sp--; *sp = (unsigned long long)entry_point; // RIP

    // General Registers (RAX ~ R15)
    for (int i = 0; i < 15; i++) {
        sp--; *sp = 0;
    }

    t->rsp = (unsigned long long)sp;
    thread_count++;

    kputs("[SCHEDULER] CREATED THREAD ID: ", 0x00FFFF00);
    kput_dec(t->id, 0x00FFFF00);
    kputs("\n", 0x00FFFF00);
}

void scheduler_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].state = THREAD_TERMINATED;
    }
    kputs("[SCHEDULER] MULTITASKING SCHEDULER INITIALIZED\n", 0x00FFFF00);
}

unsigned long long schedule(unsigned long long current_rsp) {
    if (thread_count == 0) return current_rsp;

    // 현재 스레드의 스택 포인터 저장
    if (current_thread_index >= 0) {
        threads[current_thread_index].rsp = current_rsp;
    }

    // 다음 스레드 선택
    current_thread_index = (current_thread_index + 1) % thread_count;
    thread_t *next = &threads[current_thread_index];

    return next->rsp;
}
