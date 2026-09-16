#include "debug.h"
#include "io.h"

// QEMU의 isa-debugcon 장치가 수신하는 포트다. 화면 없이도 부팅 로그를
// 확인할 수 있도록 콘솔 출력과 같은 내용을 전달한다.
#define QEMU_DEBUG_PORT 0xE9

void debug_putc(char c) { outb(QEMU_DEBUG_PORT, (unsigned char)c); }

void debug_write(const char *str) {
    while (*str) {
        debug_putc(*str++);
    }
}
