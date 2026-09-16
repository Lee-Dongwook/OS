#include "font.h"

typedef struct {
    unsigned int *framebuffer;
    unsigned int width;
    unsigned int height;
    unsigned int pixels_per_scan_line;
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
