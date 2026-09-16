#ifndef VMM_H
#define VMM_H

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)

typedef unsigned long long pt_entry_t;

typedef struct {
    pt_entry_t entries[512];
} page_table_t;

typedef struct {
    unsigned int *framebuffer;
    unsigned int width;
    unsigned int height;
    unsigned int pixels_per_scan_line;
    void *memory_map;
    unsigned long long memory_map_size;
    unsigned long long descriptor_size;
    const char *boot_file_data;
    unsigned long long boot_file_size;
} BootInfo;

void vmm_init(BootInfo *boot_info);
void vmm_map_page(page_table_t *pml4, unsigned long long virt, unsigned long long phys, unsigned long long flags);
page_table_t *vmm_kernel_pml4(void);

#endif
