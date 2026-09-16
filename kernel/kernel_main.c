typedef struct {
  unsigned int *framebuffer;
  unsigned int width;
  unsigned int height;
  unsigned int pixels_per_scan_line;
  void *memory_map;
  unsigned long long memory_map_size;
  unsigned long long descriptor_size;
} BootInfo;

extern void console_init(BootInfo *boot_info);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

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
    
    kputs("FRAMEBUFFER ADDR : ", 0x00FFFFFF);
    kput_hex((unsigned long long)boot_info->framebuffer, 0x00FFFFFF);
    kputs("\n", 0x00FFFFFF);

    kputs("MEMORY MAP SIZE  : ", 0x00FFFF00);
    kput_dec(boot_info->memory_map_size, 0x00FFFF00);
    kputs(" BYTES\n", 0x00FFFF00);

  while (1) {
    __asm__ __volatile__("hlt");
  }
}
