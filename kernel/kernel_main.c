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
extern void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
extern void *pmm_alloc_page(void);

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
    pmm_init(boot_info->memory_map, boot_info->memory_map_size, boot_info->descriptor_size);

    void *page1 = pmm_alloc_page();
    void *page2 = pmm_alloc_page();
    
    kputs("[PMM] ALLOCATED PAGE 1: ", 0x00FFFFFF);
    kput_hex((unsigned long long)page1, 0x00FFFFFF);
    kputs("\n", 0x00FFFFFF);

    kputs("[PMM] ALLOCATED PAGE 2: ", 0x00FFFFFF);
    kput_hex((unsigned long long)page2, 0x00FFFFFF);
    kputs("\n", 0x00FFFFFF);

  while (1) {
    __asm__ __volatile__("hlt");
  }
}
