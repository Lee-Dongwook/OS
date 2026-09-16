#include "font.h"

typedef struct {
    unsigned int *framebuffer;
    unsigned int width;
    unsigned int height;
    unsigned int pixels_per_scan_line;
    void *memory_map;
    unsigned long long memory_map_size;
    unsigned long long descriptor_size;
} BootInfo;

static BootInfo *g_boot_info;
static int g_cursor_x = 0;
static int g_cursor_y = 0;

void console_init(BootInfo *boot_info) {
  g_boot_info = boot_info;
  g_cursor_x = 0;
  g_cursor_y = 0;
}

void put_char(char c, unsigned int color) {
  if (c == '\n') {
        g_cursor_x = 0;
        g_cursor_y += 16;
        return;
  }

  const unsigned char *font = font_8x16[(unsigned char)c];
  unsigned int *fb = g_boot_info->framebuffer;
  unsigned int stride = g_boot_info->pixels_per_scan_line;

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 8; x++) {
      if (font[y] & (0x80 >> x)) {
        fb[(g_cursor_y + y) * stride + (g_cursor_x + x)] = color;
      }
    }
  }

  g_cursor_x += 8;
    if (g_cursor_x >= g_boot_info->width) {
        g_cursor_x = 0;
        g_cursor_y += 16;
    }
}

void kputs(const char *str, unsigned int color) {
  while (*str) {
    put_char(*str, color);
    str++;
  }
}

void kput_hex(unsigned long long val, unsigned int color) {
    char hex_chars[] = "0123456789ABCDEF";
    kputs("0x", color);
    
    int started = 0;
    for (int i = 15; i >= 0; i--) {
        unsigned char digit = (val >> (i * 4)) & 0xF;
        if (digit != 0 || started || i == 0) {
            started = 1;
            char c[2] = {hex_chars[digit], '\0'};
            kputs(c, color);
        }
    }
}

void kput_dec(unsigned long long val, unsigned int color) {
  if (val == 0) {
        kputs("0", color);
        return;
    }
    char buf[21];
    int i = 19;
    buf[20] = '\0';
    
    while (val > 0) {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    }
    kputs(&buf[i + 1], color);
}

void kprintf(const char *fmt, unsigned long long arg, unsigned int color) {
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'x' || *fmt == 'p') {
                kput_hex(arg, color);
            } else if (*fmt == 'd') {
                kput_dec(arg, color);
            } else if (*fmt == 's') {
                kputs((const char *)arg, color);
            }
        } else {
            char c[2] = {*fmt, '\0'};
            kputs(c, color);
        }
        fmt++;
    }
}

