#include "capability.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

#define MAKE_HANDLE(slot, gen) (((uint32_t)(slot) << 16) | ((gen) & 0xFFFF))
#define HANDLE_SLOT(handle) ((uint32_t)(handle) >> 16)
#define HANDLE_GEN(handle) ((uint32_t)(handle) & 0xFFFF)

void cspace_init(cspace_t *cspace) {
  if (!cspace)
    return;

  for (int i = 0; i < MAX_CSPACE_ENTRIES; i++) {
    cspace->entries[i].type = CAP_TYPE_NONE;
    cspace->entries[i].rights = 0;
    cspace->entries[i].generation = 1;
    cspace->entries[i].object_ptr = 0;
  }

  cspace->active_count = 0;
}

int32_t cap_alloc(cspace_t *cspace, cap_type_t type, uint32_t rights,
                  void *obj_ptr, uint32_t *out_handle) {
  if (!cspace || !out_handle)
    return -1;

  for (int slot = 0; slot < MAX_CSPACE_ENTRIES; slot++) {
    if (cspace->entries[slot].type == CAP_TYPE_NONE) {
      cspace->entries[slot].type = type;
      cspace->entries[slot].rights = rights;
      cspace->entries[slot].object_ptr = obj_ptr;

      // Handle = (Slot << 16) | Generation
      *out_handle = MAKE_HANDLE(slot, cspace->entries[slot].generation);
      cspace->active_count++;
      return 0; // Success
    }
  }
  return -2; // CSpace Full
}

capability_entry_t *cap_lookup(cspace_t *cspace, uint32_t handle,
                               cap_type_t expected_type,
                               uint32_t required_rights) {
  if (!cspace)
    return 0;

  uint32_t slot = HANDLE_SLOT(handle);
  uint32_t gen = HANDLE_GEN(handle);

  if (slot >= MAX_CSPACE_ENTRIES)
    return 0;

  capability_entry_t *entry = &cspace->entries[slot];

  if (entry->type == CAP_TYPE_NONE)
    return 0;

  if (entry->generation != gen) {
    kputs("[CAPABILITY ERROR] STALE HANDLE DETECTED!\n", 0x00FF0000);
    return 0;
  }

  if (expected_type != CAP_TYPE_NONE && entry->type != expected_type)
    return 0;

  if ((entry->rights & required_rights) != required_rights) {
    kputs("[CAPABILITY ERROR] INSUFFICIENT RIGHTS!\n", 0x00FF0000);
    return 0;
  }

  return entry;
}

int cap_revoke(cspace_t *cspace, uint32_t handle) {
  if (!cspace)
    return -1;

  uint32_t slot = HANDLE_SLOT(handle);
  uint32_t gen = HANDLE_GEN(handle);

  if (slot >= MAX_CSPACE_ENTRIES)
    return -1;

  capability_entry_t *entry = &cspace->entries[slot];
  if (entry->type == CAP_TYPE_NONE || entry->generation != gen)
    return -2;

  // 엔트리 초기화 및 Generation Increment (이전 Handle 무효화)
  entry->type = CAP_TYPE_NONE;
  entry->rights = 0;
  entry->object_ptr = 0;
  entry->generation++; // 세대 번호 상승으로 이전 핸들 재접근 차단
  cspace->active_count--;

  return 0;
}
