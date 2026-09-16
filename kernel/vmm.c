#include "vmm.h"
#include "pmm.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static page_table_t *kernel_pml4 = 0;

void vmm_map_page(page_table_t *pml4, unsigned long long virt, unsigned long long phys, unsigned long long flags) {
    unsigned long long pml4_idx = (virt >> 39) & 0x1FF;
    unsigned long long pdpt_idx = (virt >> 30) & 0x1FF;
    unsigned long long pd_idx   = (virt >> 21) & 0x1FF;
    unsigned long long pt_idx   = (virt >> 12) & 0x1FF;

    // 1. PML4 -> PDPT
    if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) {
        page_table_t *pdpt = (page_table_t *)pmm_alloc_page();
        for (int i = 0; i < 512; i++) pdpt->entries[i] = 0;
        pml4->entries[pml4_idx] = (unsigned long long)pdpt | PAGE_PRESENT | PAGE_WRITABLE;
    }
    page_table_t *pdpt = (page_table_t *)(pml4->entries[pml4_idx] & ~0xFFFULL);

    // 2. PDPT -> PD
    if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) {
        page_table_t *pd = (page_table_t *)pmm_alloc_page();
        for (int i = 0; i < 512; i++) pd->entries[i] = 0;
        pdpt->entries[pdpt_idx] = (unsigned long long)pd | PAGE_PRESENT | PAGE_WRITABLE;
    }
    page_table_t *pd = (page_table_t *)(pdpt->entries[pdpt_idx] & ~0xFFFULL);

    // 3. PD -> PT
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) {
        page_table_t *pt = (page_table_t *)pmm_alloc_page();
        for (int i = 0; i < 512; i++) pt->entries[i] = 0;
        pd->entries[pd_idx] = (unsigned long long)pt | PAGE_PRESENT | PAGE_WRITABLE;
    }
    page_table_t *pt = (page_table_t *)(pd->entries[pd_idx] & ~0xFFFULL);

    // 4. PT -> Phys
    pt->entries[pt_idx] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

void vmm_init(BootInfo *boot_info) {
    // PML4 테이블 생성
    kernel_pml4 = (page_table_t *)pmm_alloc_page();
    for (int i = 0; i < 512; i++) {
        kernel_pml4->entries[i] = 0;
    }

    // A. 하위 512MB RAM 영역 Identity Mapping
    unsigned long long mem_limit = 512 * 1024 * 1024;
    for (unsigned long long addr = 0; addr < mem_limit; addr += PAGE_SIZE) {
        vmm_map_page(kernel_pml4, addr, addr, PAGE_WRITABLE);
    }

    // B. 프레임버퍼 (GOP VRAM) 영역 Identity Mapping (화면 출력을 지속하기 위함)
    unsigned long long fb_base = (unsigned long long)boot_info->framebuffer;
    unsigned long long fb_size = boot_info->height * boot_info->pixels_per_scan_line * sizeof(unsigned int);
    
    for (unsigned long long addr = fb_base; addr < fb_base + fb_size; addr += PAGE_SIZE) {
        vmm_map_page(kernel_pml4, addr, addr, PAGE_WRITABLE);
    }

    kputs("[VMM] RAM & FRAMEBUFFER MAPPING COMPLETE\n", 0x00FFFF00);

    // CR3 레지스터 로드 (새로운 페이징 매핑으로 전환)
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(kernel_pml4) : "memory");

    kputs("[VMM] CR3 SWITCH SUCCESS! LOADED AT: ", 0x0000FF00);
    kput_hex((unsigned long long)kernel_pml4, 0x0000FF00);
    kputs("\n", 0x0000FF00);
}
