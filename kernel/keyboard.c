#include "keyboard.h"
#include "io.h"

extern void shell_handle_char(char c);
extern void kputs(const char *str, unsigned int color);

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_OUTPUT_BUFFER_FULL 0x01
#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMMAND 0xA0
#define PIC_SLAVE_DATA 0xA1

static const char scancode_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' ', [0x1C] = '\n'
};

void keyboard_init(void) {
    // PIC를 IRQ 0~7 -> IDT 32~39, IRQ 8~15 -> IDT 40~47로 재배치한다.
    outb(PIC_MASTER_COMMAND, 0x11);
    outb(PIC_SLAVE_COMMAND, 0x11);
    outb(PIC_MASTER_DATA, 0x20);
    outb(PIC_SLAVE_DATA, 0x28);
    outb(PIC_MASTER_DATA, 0x04);
    outb(PIC_SLAVE_DATA, 0x02);
    outb(PIC_MASTER_DATA, 0x01);
    outb(PIC_SLAVE_DATA, 0x01);

    // APIC가 타이머를 담당하므로 IRQ0은 차단하고 키보드 IRQ1만 허용한다.
    outb(PIC_MASTER_DATA, 0xFD);
    outb(PIC_SLAVE_DATA, 0xFF);
    kputs("[KEYBOARD] PS/2 IRQ1 INPUT ENABLED\n", 0x00FFFF00);
}

static void handle_scancode(unsigned char scancode) {
    // Break code와 확장 키는 현재 셸 입력에서 무시한다.
    if (scancode & 0x80 || scancode >= sizeof(scancode_map)) {
        return;
    }

    char c = scancode_map[scancode];
    if (c) {
        shell_handle_char(c);
    }
}

void keyboard_handle_interrupt(void) {
    if (inb(PS2_STATUS_PORT) & PS2_OUTPUT_BUFFER_FULL) {
        handle_scancode(inb(PS2_DATA_PORT));
    }
    // 키보드는 master PIC의 IRQ1이므로 master PIC에만 EOI를 전달한다.
    outb(PIC_MASTER_COMMAND, 0x20);
}

void keyboard_poll(void) {
    if (inb(PS2_STATUS_PORT) & PS2_OUTPUT_BUFFER_FULL) {
        handle_scancode(inb(PS2_DATA_PORT));
    }
}
