#include "task.h"
#include "pmm.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

static task_t task_table[MAX_TASKS];
static int next_task_id = 1;

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_table[i].task_id = 0;
        task_table[i].is_active = 0;
        task_table[i].pml4 = 0;
        task_table[i].port_count = 0;
    }
    kputs("[TASK] MACH TASK SUBSYSTEM INITIALIZED\n", 0x00FFFF00);
}

// 새로운 Mach Task 생성
task_t *task_create(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!task_table[i].is_active) {
            task_t *t = &task_table[i];
            t->task_id = next_task_id++;
            t->is_active = 1;
            t->port_count = 0;

            // Task 전용 PML4 (페이지 테이블) 할당
            t->pml4 = (page_table_t *)pmm_alloc_page();
            if (!t->pml4) {
                t->task_id = 0;
                t->is_active = 0;
                return 0;
            }
            for (int j = 0; j < 512; j++) {
                t->pml4->entries[j] = 0;
            }

            kputs("[TASK] CREATED TASK ID: ", 0x00FFFF00);
            kput_dec(t->task_id, 0x00FFFF00);
            kputs("\n", 0x00FFFF00);

            return t;
        }
    }
    return 0; // Task 공간 부족
}

// Task에 Mach Port 권한 추가
int task_add_port(task_t *task, unsigned int port_id) {
    if (!task || !task->is_active || port_id == 0 || task->port_count >= MAX_TASK_PORTS) return -1;
    task->ports[task->port_count++] = port_id;
    return 0;
}

int task_destroy(task_t *task) {
    if (!task || !task->is_active) {
        return -1;
    }
    if (task->pml4) {
        pmm_free_page(task->pml4);
    }
    task->task_id = 0;
    task->is_active = 0;
    task->pml4 = 0;
    task->port_count = 0;
    return 0;
}

int task_active_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].is_active) {
            count++;
        }
    }
    return count;
}
