#ifndef TASK_H
#define TASK_H

#include "vmm.h"

#define MAX_TASKS 8

typedef struct task {
    int task_id;
    int is_active;
    page_table_t *pml4; // Task 전용 독립 가상 메모리 공간(CR3)
} task_t;

void task_init(void);
task_t *task_create(void);

#endif
