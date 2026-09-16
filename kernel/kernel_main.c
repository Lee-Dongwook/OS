typedef struct {
  unsigned int *framebuffer;
  unsigned int width;
  unsigned int height;
  unsigned int pixels_per_scan_line;
} BootInfo;

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);

void kernel_main(BootInfo *boot_info) {
  unsigned int *fb = boot_info->framebuffer;
  unsigned int stride = boot_info->pixels_per_scan_line;

  for (unsigned int y = 0; y < boot_info->height; y++) {
        for (unsigned int x = 0; x < boot_info->width; x++) {
            fb[y * stride + x] = 0x00000000; // Black
        }
  }

  console_init(boot_info);

  kputs("XNU OS KERNEL INIT...\n", 0x0000FF00); // Green
  kputs("WELCOME TO MY BARE METAL OS!\n", 0x00FFFFFF); // White

  while (1) {
    __asm__ __volatile__("hlt");
  }
}
