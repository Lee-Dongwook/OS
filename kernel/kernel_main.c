
#include "vmm.h"
#include "mach_ipc.h"

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void vmm_init(BootInfo *boot_info);
extern void gdt_init(void);
extern void idt_init(void);

void kstrcpy(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void kernel_main(BootInfo *boot_info) {
  unsigned int *fb = boot_info->framebuffer;
  unsigned int stride = boot_info->pixels_per_scan_line;

  for (unsigned int y = 0; y < boot_info->height; y++) {
        for (unsigned int x = 0; x < boot_info->width; x++) {
            fb[y * stride + x] = 0x00000000; // Black
        }
  }

    console_init(boot_info);

    kputs("XNU OS KERNEL INIT...\n\n", 0x0000FF00);

    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);

    vmm_init(boot_info);

    gdt_init();
    idt_init();

    mach_ipc_init();
    unsigned int my_port = mach_port_allocate();

    kputs("[MACH IPC] ALLOCATED PORT ID: ", 0x00FFFFFF);
    kput_dec(my_port, 0x00FFFFFF);
    kputs("\n", 0x00FFFFFF);

    mach_message_t send_msg;
    send_msg.header.msgh_remote_port = my_port;
    send_msg.header.msgh_local_port = 0;
    send_msg.header.msgh_id = 1001; // 예: 서비스 요청 ID
    kstrcpy(send_msg.body, "Hello, XNU Mach IPC Message!");

    mach_msg_send(&send_msg);
    kputs("[MACH IPC] MESSAGE SENT TO PORT ", 0x0000FF00);
    kput_dec(my_port, 0x0000FF00);
    kputs("\n", 0x0000FF00);

    mach_message_t recv_msg;
    if (mach_msg_receive(my_port, &recv_msg) == 0) {
        kputs("[MACH IPC] RECV MSG ID: ", 0x0000FFFF);
        kput_dec(recv_msg.header.msgh_id, 0x0000FFFF);
        kputs("\n[MACH IPC] RECV BODY: ", 0x0000FFFF);
        kputs(recv_msg.body, 0x0000FFFF);
        kputs("\n", 0x0000FFFF);
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
