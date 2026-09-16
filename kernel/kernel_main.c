#include "vmm.h"
#include "mach_ipc.h"
#include "scheduler.h"
#include "task.h"
#include "keyboard.h"
#include "shell.h"
#include "initramfs.h"
#include "macho.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);
extern void apic_init(void);
extern void syscall_init(void);
extern void *pmm_alloc_page(void);
extern void enter_user_mode(unsigned long long rip, unsigned long long rsp);

static void ipc_self_test(void) {
    unsigned int port = mach_port_allocate();
    // 프리스탠딩 환경에는 libc memset이 없으므로 BSS의 정적 버퍼를 사용한다.
    static mach_message_t message;
    static mach_message_t received;

    if (port == 0) {
        kputs("[TEST] IPC PORT ALLOCATION FAILED\n", 0x00FF0000);
        return;
    }

    message.header.msgh_remote_port = port;
    message.header.msgh_id = 1;
    message.body[0] = 'O';
    message.body[1] = 'K';

    if (mach_msg_send(&message) == 0 &&
        mach_msg_receive(port, &received) == 0 &&
        received.header.msgh_id == 1 &&
        received.body[0] == 'O' && received.body[1] == 'K') {
        kputs("[TEST] IPC SEND/RECEIVE SUCCESSFUL!\n", 0x0000FF00);
    } else {
        kputs("[TEST] IPC SEND/RECEIVE FAILED\n", 0x00FF0000);
    }

    mach_port_destroy(port);
}

void thread_a(void) {
    while (1) {
        for (volatile int i = 0; i < 5000000; i++);
    }
}

void thread_b(void) {
    while (1) {
        for (volatile int i = 0; i < 5000000; i++);
    }
}

void idle_thread(void) {
    while (1) {
        __asm__ __volatile__("hlt");
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
    task_init();
    syscall_init();
    initramfs_init(boot_info->boot_file_data, boot_info->boot_file_size);
    ipc_self_test();
    keyboard_init();
    shell_init();

    thread_create(idle_thread);
    thread_create(thread_a);
    thread_create(thread_b);
    apic_init();

    // IDT와 APIC 타이머가 준비된 뒤에만 인터럽트를 허용한다.
    __asm__ __volatile__("sti" : : : "memory");

    kputs("\n[TEST] PREPARING RING 3 USERLAND ENTRY...\n", 0x00FFFF00);

    unsigned long long user_entry = 0;

    void *user_stack_phys = pmm_alloc_page();
    vmm_map_page(vmm_kernel_pml4(), 0x500000, (unsigned long long)user_stack_phys,
                 PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    if (user_entry == 0) {
        void *user_code_phys = pmm_alloc_page();
        vmm_map_page(vmm_kernel_pml4(), 0x400000, (unsigned long long)user_code_phys,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

        unsigned char *uprog = (unsigned char *)0x400000;
        uprog[0] = 0x48; uprog[1] = 0xC7; uprog[2] = 0xC0; uprog[3] = 0x01; uprog[4] = 0x00; uprog[5] = 0x00; uprog[6] = 0x00; // mov $1, %rax
        uprog[7] = 0x0F; uprog[8] = 0x05;                                                                                    // syscall
        uprog[9] = 0xEB; uprog[10] = 0xFE;                                                                                   // jmp .

        user_entry = 0x400000;
    }

    kputs("\n[USERLAND] SWITCHING TO RING 3 USER MODE...\n", 0x0000FF00);
    enter_user_mode(user_entry, 0x501000);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
