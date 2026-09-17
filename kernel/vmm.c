#include "vmm.h"
#include "pmm.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void *pmm_alloc_page(void);

page_table_t *kernel_pml4 = 0;

static void clear_page_table(page_table_t *table) {
    for (int index = 0; index < 512; index++) {
        table->entries[index] = 0;
    }
}

page_table_t *vmm_create_user_pml4(void) {
    if (!kernel_pml4) {
        return 0;
    }

    page_table_t *user_pml4 = (page_table_t *)pmm_alloc_page();
    if (!user_pml4) {
        return 0;
    }

    for (int index = 0; index < 512; index++) {
        user_pml4->entries[index] = kernel_pml4->entries[index];
    }

    // 현재 사용자 이미지의 4 GiB 영역은 PML4[0]을 사용한다. 이 항목을
    // 복제하지 않으면 사용자 매핑을 추가할 때 커널의 PDPT까지 변경된다.
    if (kernel_pml4->entries[0] & PAGE_PRESENT) {
        page_table_t *kernel_pdpt = (page_table_t *)(kernel_pml4->entries[0] & ~0xFFFULL);
        page_table_t *user_pdpt = (page_table_t *)pmm_alloc_page();
        if (!user_pdpt) {
            pmm_free_page(user_pml4);
            return 0;
        }
        for (int index = 0; index < 512; index++) {
            user_pdpt->entries[index] = kernel_pdpt->entries[index];
        }
        user_pml4->entries[0] = (unsigned long long)user_pdpt |
                                (kernel_pml4->entries[0] & 0xFFFULL);
    }

    return user_pml4;
}

int vmm_user_mapping_is_private(page_table_t *user_pml4, unsigned long long user_virtual) {
    unsigned long long pml4_index = (user_virtual >> 39) & 0x1FF;
    unsigned long long pdpt_index = (user_virtual >> 30) & 0x1FF;
    unsigned long long pd_index = (user_virtual >> 21) & 0x1FF;
    unsigned long long pt_index = (user_virtual >> 12) & 0x1FF;

    if (!user_pml4 || !kernel_pml4 || user_pml4 == kernel_pml4 ||
        !(user_pml4->entries[pml4_index] & PAGE_PRESENT) ||
        !(user_pml4->entries[pml4_index] & PAGE_USER) ||
        (user_pml4->entries[pml4_index] & ~0xFFFULL) ==
            (kernel_pml4->entries[pml4_index] & ~0xFFFULL)) {
        return 0;
    }

    page_table_t *user_pdpt = (page_table_t *)(user_pml4->entries[pml4_index] & ~0xFFFULL);
    if (!(user_pdpt->entries[pdpt_index] & PAGE_PRESENT) ||
        !(user_pdpt->entries[pdpt_index] & PAGE_USER)) {
        return 0;
    }
    page_table_t *user_pd = (page_table_t *)(user_pdpt->entries[pdpt_index] & ~0xFFFULL);
    if (!(user_pd->entries[pd_index] & PAGE_PRESENT) ||
        !(user_pd->entries[pd_index] & PAGE_USER)) {
        return 0;
    }
    page_table_t *user_pt = (page_table_t *)(user_pd->entries[pd_index] & ~0xFFFULL);
    if (!(user_pt->entries[pt_index] & PAGE_PRESENT) ||
        !(user_pt->entries[pt_index] & PAGE_USER)) {
        return 0;
    }

    page_table_t *kernel_pdpt = (page_table_t *)(kernel_pml4->entries[pml4_index] & ~0xFFFULL);
    // 커널 PML4[0]은 하위 identity mapping을 보유할 수 있다. 대상 PDPT
    // 슬롯에 사용자 페이지가 생기지 않았는지를 별도로 검사한다.
    return !(kernel_pdpt->entries[pdpt_index] & PAGE_PRESENT);
}

void vmm_map_page(page_table_t *pml4, unsigned long long virt, unsigned long long phys, unsigned long long flags) {
    unsigned long long pml4_idx = (virt >> 39) & 0x1FF;
    unsigned long long pdpt_idx = (virt >> 30) & 0x1FF;
    unsigned long long pd_idx = (virt >> 21) & 0x1FF;
    unsigned long long pt_idx = (virt >> 12) & 0x1FF;

    // 사용자 페이지는 모든 상위 페이지 테이블 엔트리에도 USER 비트가 있어야 한다.
    unsigned long long table_flags = PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);

    // 1. PML4 -> PDPT
    if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) {
        page_table_t *pdpt = (page_table_t *)pmm_alloc_page();
        clear_page_table(pdpt);
        pml4->entries[pml4_idx] = (unsigned long long)pdpt | table_flags;
    } else if (flags & PAGE_USER) {
        pml4->entries[pml4_idx] |= PAGE_USER;
    }
    page_table_t *pdpt = (page_table_t *)(pml4->entries[pml4_idx] & ~0xFFFULL);

    // 2. PDPT -> PD
    if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) {
        page_table_t *pd = (page_table_t *)pmm_alloc_page();
        clear_page_table(pd);
        pdpt->entries[pdpt_idx] = (unsigned long long)pd | table_flags;
    } else if (flags & PAGE_USER) {
        pdpt->entries[pdpt_idx] |= PAGE_USER;
    }
    page_table_t *pd = (page_table_t *)(pdpt->entries[pdpt_idx] & ~0xFFFULL);

    // 3. PD -> PT
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) {
        page_table_t *pt = (page_table_t *)pmm_alloc_page();
        clear_page_table(pt);
        pd->entries[pd_idx] = (unsigned long long)pt | table_flags;
    } else if (flags & PAGE_USER) {
        pd->entries[pd_idx] |= PAGE_USER;
    }
    page_table_t *pt = (page_table_t *)(pd->entries[pd_idx] & ~0xFFFULL);

    // 4. PT -> Phys
    pt->entries[pt_idx] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

int vmm_protect_page(page_table_t *pml4, unsigned long long virt, unsigned long long flags) {
    unsigned long long pml4_idx = (virt >> 39) & 0x1FF;
    unsigned long long pdpt_idx = (virt >> 30) & 0x1FF;
    unsigned long long pd_idx = (virt >> 21) & 0x1FF;
    unsigned long long pt_idx = (virt >> 12) & 0x1FF;

    if (!pml4 || !(pml4->entries[pml4_idx] & PAGE_PRESENT)) {
        return -1;
    }
    page_table_t *pdpt = (page_table_t *)(pml4->entries[pml4_idx] & ~0xFFFULL);
    if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) {
        return -1;
    }
    page_table_t *pd = (page_table_t *)(pdpt->entries[pdpt_idx] & ~0xFFFULL);
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) {
        return -1;
    }
    page_table_t *pt = (page_table_t *)(pd->entries[pd_idx] & ~0xFFFULL);
    if (!(pt->entries[pt_idx] & PAGE_PRESENT)) {
        return -1;
    }

    pt->entries[pt_idx] = (pt->entries[pt_idx] & ~0xFFFULL) | flags | PAGE_PRESENT;
    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

page_table_t *vmm_kernel_pml4(void) { return kernel_pml4; }

void vmm_init(BootInfo *boot_info) {
    // PML4 테이블 생성
    kernel_pml4 = (page_table_t *)pmm_alloc_page();
    clear_page_table(kernel_pml4);

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

    vmm_map_page(kernel_pml4, 0xFEE00000ULL, 0xFEE00000ULL, PAGE_WRITABLE);

    kputs("[VMM] RAM & FRAMEBUFFER MAPPING COMPLETE\n", 0x00FFFF00);

    // CR3 레지스터 로드 (새로운 페이징 매핑으로 전환)
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(kernel_pml4) : "memory");

    kputs("[VMM] CR3 SWITCH SUCCESS! LOADED AT: ", 0x0000FF00);
    kput_hex((unsigned long long)kernel_pml4, 0x0000FF00);
    kputs("\n", 0x0000FF00);
}
