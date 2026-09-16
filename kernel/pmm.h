#ifndef PMM_H
#define PMM_H

#define PAGE_SIZE 4096

typedef struct {
    unsigned int type;
    unsigned int pad;
    unsigned long long physical_start;
    unsigned long long virtual_start;
    unsigned long long number_of_pages;
    unsigned long long attribute;
} EFI_MEMORY_DESCRIPTOR;

#define EFI_CONVENTIONAL_MEMORY 7

void pmm_init(void *memory_map, unsigned long long map_size, unsigned long long descriptor_size);
void *pmm_alloc_page(void);
void pmm_free_page(void *ptr);
unsigned long long pmm_total_usable_memory(void);
unsigned long long pmm_free_memory(void);

#endif
