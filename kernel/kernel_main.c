typedef struct {
  unsigned int *framebuffer;
  unsigned int width;
  unsigned int height;
  unsigned int pixels_per_scan_line;
} BootInfo;

void kernel_main(BootInfo *boot_info) {
  unsigned int *fb = boot_info->framebuffer;
  unsigned int stride = boot_info->pixels_per_scan_line;

  unsigned int start_x = boot_info->width / 2 - 100;
  unsigned int start_y = boot_info->height / 2 - 100;

  for (unsigned int y = start_y; y < start_y + 200; y++) {
        for (unsigned int x = start_x; x < start_x + 200; x++) {
            fb[y * stride + x] = 0x0000FF00; // Green
        }
  }

  while (1) {
    __asm__ __volatile__("hlt");
  }
}
