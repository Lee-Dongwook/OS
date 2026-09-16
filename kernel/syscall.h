#ifndef SYSCALL_H
#define SYSCALL_H

#define IA32_EFER       0xC0000080
#define IA32_STAR       0xC0000081
#define IA32_LSTAR      0xC0000082
#define IA32_FMASK      0xC0000084

void syscall_init(void);

#endif
