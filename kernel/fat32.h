#ifndef FAT32_H
#define FAT32_H

// FAT32 Boot Sector / BPB 구조체
typedef struct {
    unsigned char  jmp_boot[3];
    unsigned char  oem_name[8];
    unsigned short bytes_per_sector;      // 보통 512
    unsigned char  sectors_per_cluster;    // 클러스터당 섹터 수
    unsigned short reserved_sector_count;  // 예약된 섹터 수
    unsigned char  num_fats;               // FAT 테이블 개수 (보통 2)
    unsigned short root_entry_count;      // FAT32에서는 0
    unsigned short total_sectors_16;
    unsigned char  media_type;
    unsigned short fat_size_16;
    unsigned short sectors_per_track;
    unsigned short num_heads;
    unsigned int   hidden_sectors;
    unsigned int   total_sectors_32;

    // FAT32 Extended Structure
    unsigned int   fat_size_32;            // FAT 테이블당 섹터 수
    unsigned short ext_flags;
    unsigned short fs_version;
    unsigned int   root_cluster;           // 루트 디렉터리 시작 클러스터 (보통 2)
    unsigned short fs_info;
    unsigned short backup_boot_sector;
    unsigned char  reserved[12];
    unsigned char  drive_number;
    unsigned char  reserved1;
    unsigned char  boot_signature;
    unsigned int   volume_id;
    unsigned char  volume_label[11];
    unsigned char  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

// Directory Entry (32 바이트)
typedef struct {
    unsigned char  name[11];               // 8.3 파일명 레거시 포맷
    unsigned char  attr;                   // 파일 속성 (0x10 = Directory, 0x20 = Archive/File)
    unsigned char  nt_reserved;
    unsigned char  creation_time_tenth;
    unsigned short creation_time;
    unsigned short creation_date;
    unsigned short last_access_date;
    unsigned short first_cluster_high;     // 상위 16비트 클러스터
    unsigned short write_time;
    unsigned short write_date;
    unsigned short first_cluster_low;      // 하위 16비트 클러스터
    unsigned int   file_size;              // 파일 크기 (바이트)
} __attribute__((packed)) fat32_dir_entry_t;

int fat32_init(void);
void fat32_list_root(void);
int fat32_read_file(const char *filename, void *buffer, unsigned int max_len);

#endif
