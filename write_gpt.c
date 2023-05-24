#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <uchar.h> 
#include <string.h>
#include <inttypes.h>

// -------------------------------------
// Global Typedefs
// -------------------------------------
// Globally Unique IDentifier (aka UUID) 
typedef struct {
    uint32_t time_lo;
    uint16_t time_mid;
    uint16_t time_hi_and_ver;       // Highest 4 bits are version #
    uint8_t clock_seq_hi_and_res;   // Highest bits are variant #
    uint8_t clock_seq_lo;
    uint8_t node[6];
} __attribute__ ((packed)) Guid;

// MBR Partition
typedef struct {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_lba;
} __attribute__ ((packed)) Mbr_Partition;

// Master Boot Record
typedef struct {
    uint8_t boot_code[440];
    uint32_t mbr_signature;
    uint16_t unknown;
    Mbr_Partition partition[4];
    uint16_t boot_signature;
} __attribute__ ((packed)) Mbr;

// GPT Header
typedef struct {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved_1;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    Guid disk_guid;
    uint64_t partition_table_lba;
    uint32_t number_of_entries;
    uint32_t size_of_entry;
    uint32_t partition_table_crc32;

    uint8_t reserved_2[512-92];
} __attribute__ ((packed)) Gpt_Header;

// GPT Partition Entry
typedef struct {
    Guid partition_type_guid;
    Guid unique_guid;
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    char16_t name[36];  // UCS-2 (UTF-16 limited to code points 0x0000 - 0xFFFF)
} __attribute__ ((packed)) Gpt_Partition_Entry;

// FAT32 Volume Boot Record (VBR)
typedef struct {
    uint8_t  BS_jmpBoot[3];
    uint8_t  BS_OEMName[8];
    uint16_t BPB_BytesPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint8_t  BS_VolID[4];
    uint8_t  BS_VolLab[11];
    uint8_t  BS_FilSysType[8];

    // Not in fatgen103.doc tables
    uint8_t  boot_code[510-90];
    uint16_t bootsect_sig;      // 0xAA55
} __attribute__ ((packed)) Vbr;

// FAT32 File System Info Sector
typedef struct {
    uint32_t FSI_LeadSig;
    uint8_t  FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Nxt_Free;
    uint8_t  FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
} __attribute__ ((packed)) FSInfo;

// FAT32 Directory Entry (Short Name)
typedef struct {
    uint8_t  DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} __attribute__ ((packed)) FAT32_Dir_Entry_Short;

// FAT32 Directory Entry Attributes
typedef enum {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN    = 0x02,
    ATTR_SYSTEM    = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE   = 0x20,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN |
                     ATTR_SYSTEM    | ATTR_VOLUME_ID,
} FAT32_Dir_Attr;


// FAT32 File "types"
typedef enum {
    TYPE_DIR,   // Directory
    TYPE_FILE,  // Regular file
} File_Type;

// -------------------------------------
// Global constants, enums
// -------------------------------------
// EFI System Partition GUID
const Guid ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, 
                        { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } };

// (Microsoft) Basic Data GUID
const Guid BASIC_DATA_GUID = { 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0,
                                { 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };

enum {
    GPT_TABLE_ENTRY_SIZE = 128,
    NUMBER_OF_GPT_TABLE_ENTRIES = 128,
    GPT_TABLE_SIZE = 16384,             // Minimum size per UEFI spec 2.10
    ALIGNMENT = 1048576,                // 1 MiB alignment value
};

// -------------------------------------
// Global Variables
// -------------------------------------
char *image_name = "test.img";

uint64_t lba_size = 512;
uint64_t esp_size = 1024*1024*33;   // 33 MiB
uint64_t data_size = 1024*1024*1;   // 1 MiB
uint64_t image_size = 0;
uint64_t esp_size_lbas = 0, data_size_lbas = 0, image_size_lbas = 0,  
         gpt_table_lbas = 0;                              // Sizes in lbas
uint64_t align_lba = 0, esp_lba = 0, data_lba = 0,
         fat32_fats_lba = 0, fat32_data_lba = 0;          // Starting LBA values

// =====================================
// Convert bytes to LBAs
// =====================================
uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes / lba_size) + (bytes % lba_size > 0 ? 1 : 0);
}

// =====================================
// Pad out 0s to full lba size
// =====================================
void write_full_lba_size(FILE *image) {
    uint8_t zero_sector[512];
    for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++)
        fwrite(zero_sector, sizeof zero_sector, 1, image);
}

// =====================================
// Get next highest aligned lba value after input lba
// =====================================
uint64_t next_aligned_lba(const uint64_t lba) {
    return lba - (lba % align_lba) + align_lba;
}

// =====================================
// Create a new Version 4 Variant 2 GUID
// =====================================
Guid new_guid(void) {
    uint8_t rand_arr[16] = { 0 };

    for (uint8_t i = 0; i < sizeof rand_arr; i++)
        rand_arr[i] = rand() % (UINT8_MAX + 1);

    // Fill out GUID
    Guid result = {
        .time_lo         = *(uint32_t *)&rand_arr[0],
        .time_mid        = *(uint16_t *)&rand_arr[4],
        .time_hi_and_ver = *(uint16_t *)&rand_arr[6],
        .clock_seq_hi_and_res = rand_arr[8],
        .clock_seq_lo = rand_arr[9],
        .node = { rand_arr[10], rand_arr[11], rand_arr[12], rand_arr[13],
                  rand_arr[14], rand_arr[15] },
    };

    // Fill out version bits - version 4
    result.time_hi_and_ver &= ~(1 << 15); // 0b_0_111 1111
    result.time_hi_and_ver |= (1 << 14);  // 0b0_1_00 0000
    result.time_hi_and_ver &= ~(1 << 13); // 0b11_0_1 1111
    result.time_hi_and_ver &= ~(1 << 12); // 0b111_0_ 1111

    // Fill out variant bits
    result.clock_seq_hi_and_res |= (1 << 7);    // 0b_1_000 0000
    result.clock_seq_hi_and_res |= (1 << 6);    // 0b0_1_00 0000
    result.clock_seq_hi_and_res &= ~(1 << 5);   // 0b11_0_1 1111

    return result;
}

// =====================================
// Create CRC32 table values
// =====================================
uint32_t crc_table[256];

void create_crc32_table(void) {
    uint32_t c = 0;

    for (int32_t n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for (uint8_t k = 0; k < 8; k++) {
            if (c & 1) 
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
}

// =====================================
// Calculate CRC32 value for range of data
// =====================================
uint32_t calculate_crc32(void *buf, int32_t len) {
    static bool made_crc_table = false;

    uint8_t *bufp = buf;
    uint32_t c = 0xFFFFFFFFL;

    if (!made_crc_table) {
        create_crc32_table();
        made_crc_table = true;
    }

    for (int32_t n = 0; n < len; n++) 
        c = crc_table[(c ^ bufp[n]) & 0xFF] ^ (c >> 8);

    // Invert bits for return value
    return c ^ 0xFFFFFFFFL;
}

// =====================================
// Get new date/time values for FAT32 directory entries
// ===================================== 
void get_fat_dir_entry_time_date(uint16_t *in_time, uint16_t *in_date) {
    time_t curr_time;
    curr_time = time(NULL);
    struct tm tm = *localtime(&curr_time);

    // FAT32 needs # of years since 1980, localtime returns tm_year as # years since 1900,
    //   subtract 80 years for correct year value. Also convert month of year from 0-11 to 1-12
    //   by adding 1
    *in_date = ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday;

    // Seconds is # 2-second count, 0-29
    if (tm.tm_sec == 60) tm.tm_sec = 59;
    *in_time = tm.tm_hour << 11 | tm.tm_min << 5 | (tm.tm_sec / 2);
}

// =====================================
// Write protective MBR
// ===================================== 
bool write_mbr(FILE *image) {
    uint64_t mbr_image_lbas = image_size_lbas;
    if (mbr_image_lbas > 0xFFFFFFFF) mbr_image_lbas = 0x100000000;

    Mbr mbr = {
        .boot_code = { 0 },
        .mbr_signature = 0,
        .unknown = 0,
        .partition[0] = {
            .boot_indicator = 0,
            .starting_chs = { 0x00, 0x02, 0x00 },
            .os_type = 0xEE,        // Protective GPT
            .ending_chs = { 0xFF, 0xFF, 0xFF },
            .starting_lba = 0x00000001,
            .size_lba = mbr_image_lbas - 1,
        },
        .boot_signature = 0xAA55,
    };

    if (fwrite(&mbr, 1, sizeof mbr, image) != sizeof mbr)
        return false;
    write_full_lba_size(image);

    return true;
}
// =====================================
// Write GPT headers & tables, primary & secondary
// =====================================
bool write_gpts(FILE *image) {
    // Fill out primary GPT header
    Gpt_Header primary_gpt = {
        .signature = { "EFI PART" },
        .revision = 0x00010000,   // Version 1.0
        .header_size = 92,
        .header_crc32 = 0,      // Will calculate later
        .reserved_1 = 0,
        .my_lba = 1,            // LBA 1 is right after MBR
        .alternate_lba = image_size_lbas - 1,
        .first_usable_lba = 1 + 1 + gpt_table_lbas, // MBR + GPT header + primary gpt table
        .last_usable_lba = image_size_lbas - 1 - gpt_table_lbas - 1, // 2nd GPT header + table
        .disk_guid = new_guid(),
        .partition_table_lba = 2,   // After MBR + GPT header
        .number_of_entries = 128,   
        .size_of_entry = 128,
        .partition_table_crc32 = 0, // Will calculate later
        .reserved_2 = { 0 },
    };

    // Fill out primary table partition entries
    Gpt_Partition_Entry gpt_table[NUMBER_OF_GPT_TABLE_ENTRIES] = {
        // EFI System Paritition
        {
            .partition_type_guid = ESP_GUID,
            .unique_guid = new_guid(),
            .starting_lba = esp_lba,
            .ending_lba = esp_lba + esp_size_lbas,
            .attributes = 0,
            .name = u"EFI SYSTEM",
        },

        // Basic Data Paritition
        {
            .partition_type_guid = BASIC_DATA_GUID,
            .unique_guid = new_guid(),
            .starting_lba = data_lba,
            .ending_lba = data_lba + data_size_lbas,
            .attributes = 0,
            .name = u"BASIC DATA",
        },
    };

    // Fill out primary header CRC values
    primary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    primary_gpt.header_crc32 = calculate_crc32(&primary_gpt, primary_gpt.header_size);

    // Write primary gpt header to file
    if (fwrite(&primary_gpt, 1, sizeof primary_gpt, image) != sizeof primary_gpt)
        return false;
    write_full_lba_size(image);

    // Write primary gpt table to file
    if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table)
        return false;

    // Fill out secondary GPT header
    Gpt_Header secondary_gpt = primary_gpt;

    secondary_gpt.header_crc32 = 0;
    secondary_gpt.partition_table_crc32 = 0;
    secondary_gpt.my_lba = primary_gpt.alternate_lba;
    secondary_gpt.alternate_lba = primary_gpt.my_lba;
    secondary_gpt.partition_table_lba = image_size_lbas - 1 - gpt_table_lbas;

    // Fill out secondary header CRC values
    secondary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    secondary_gpt.header_crc32 = calculate_crc32(&secondary_gpt, secondary_gpt.header_size);

    // Go to position of secondary table
    fseek(image, secondary_gpt.partition_table_lba * lba_size, SEEK_SET);
    
    // Write secondary gpt table to file
    if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table)
        return false;

    // Write secondary gpt header to file
    if (fwrite(&secondary_gpt, 1, sizeof secondary_gpt, image) != sizeof secondary_gpt)
        return false;
    write_full_lba_size(image);

    return true;
}

// =====================================
// Write EFI System Partition (ESP) w/FAT32 filesystem
// =====================================
bool write_esp(FILE *image) {
    // Reserved sectors region --------------------------
    // Fill out Volume Boot Record (VBR)
    const uint8_t reserved_sectors = 32; // FAT32
    Vbr vbr = {
        .BS_jmpBoot = { 0xEB, 0x00, 0x90 },
        .BS_OEMName = { "THISDISK" },
        .BPB_BytesPerSec = lba_size,     // This is limited to only 512/1024/2048/4096
        .BPB_SecPerClus = 1,
        .BPB_RsvdSecCnt = reserved_sectors,
        .BPB_NumFATs = 2,
        .BPB_RootEntCnt = 0,
        .BPB_TotSec16 = 0,
        .BPB_Media = 0xF8,               // "Fixed" non-removable media; Could also be 0xF0 for e.g. flash drive
        .BPB_FATSz16 = 0,
        .BPB_SecPerTrk = 0,
        .BPB_NumHeads = 0,
        .BPB_HiddSec = esp_lba - 1,      // # of sectors before this partition/volume
        .BPB_TotSec32 = esp_size_lbas,   // Size of this partition
        .BPB_FATSz32 = (align_lba - reserved_sectors) / 2,  // Align data region on alignment value
        .BPB_ExtFlags = 0,               // Mirrored FATs
        .BPB_FSVer = 0,
        .BPB_RootClus = 2,              // Clusters 0 & 1 are reserved; root dir cluster starts at 2
        .BPB_FSInfo = 1,                // Sector 0 = this VBR; FS Info sector follows it
        .BPB_BkBootSec = 6,
        .BPB_Reserved = { 0 },
        .BS_DrvNum = 0x80,              // 1st hard drive
        .BS_Reserved1 = 0,
        .BS_BootSig = 0x29,
        .BS_VolID = { 0 },
        .BS_VolLab = { "NO NAME    " }, // No volume label 
        .BS_FilSysType = { "FAT32   " },

        // Not in fatgen103.doc tables
        .boot_code = { 0 },
        .bootsect_sig = 0xAA55,     
    };

    // Fill out file system info sector
    FSInfo fsinfo = {
        .FSI_LeadSig = 0x41615252,
        .FSI_Reserved1 = { 0 },
        .FSI_StrucSig = 0x61417272,
        .FSI_Free_Count = 0xFFFFFFFF,
        .FSI_Nxt_Free = 5,              // First available cluster (value = 0) after /EFI/BOOT
        .FSI_Reserved2 = { 0 },
        .FSI_TrailSig = 0xAA550000,
    };

    fat32_fats_lba = esp_lba + vbr.BPB_RsvdSecCnt;
    fat32_data_lba = fat32_fats_lba + (vbr.BPB_NumFATs * vbr.BPB_FATSz32);

    // Write VBR and FSInfo sector
    fseek(image, esp_lba * lba_size, SEEK_SET);
    if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not write ESP VBR to image\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP File System Info Sector to image\n");
        return false;
    }
    write_full_lba_size(image);

    // Go to backup boot sector location
    fseek(image, (esp_lba + vbr.BPB_BkBootSec) * lba_size, SEEK_SET);

    // Write VBR and FSInfo at backup location
    fseek(image, esp_lba * lba_size, SEEK_SET);
    if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not write VBR to image\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP File System Info Sector to image\n");
        return false;
    }
    write_full_lba_size(image);

    // FAT region --------------------------
    // Write FATs (NOTE: FATs will be mirrored)
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
        fseek(image, 
              (fat32_fats_lba + (i*vbr.BPB_FATSz32)) * lba_size, 
              SEEK_SET);

        uint32_t cluster = 0;

        // Cluster 0; FAT identifier, lowest 8 bits are the media type/byte
        cluster = 0xFFFFFF00 | vbr.BPB_Media;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 1; End of Chain (EOC) marker
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 2; Root dir '/' cluster start, if end of file/dir data then write EOC marker
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 3; '/EFI' dir cluster
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 4; '/EFI/BOOT' dir cluster
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 5+; Other files/directories...
        // e.g. if adding a file with a size = 5 sectors/clusters
        //cluster = 6;    // Point to next cluster containing file data
        //cluster = 7;    // Point to next cluster containing file data
        //cluster = 8;    // Point to next cluster containing file data
        //cluster = 9;    // Point to next cluster containing file data
        //cluster = 0xFFFFFFFF; // EOC marker, no more file data after this cluster
    }

    // Data region --------------------------
    // Write File/Dir data...
    fseek(image, fat32_data_lba * lba_size, SEEK_SET);

    // Root '/' Directory entries
    // "/EFI" dir entry 
    FAT32_Dir_Entry_Short dir_ent = {
        .DIR_Name = { "EFI        " },
        .DIR_Attr = ATTR_DIRECTORY,
        .DIR_NTRes = 0,
        .DIR_CrtTimeTenth = 0,
        .DIR_CrtTime = 0,
        .DIR_CrtDate = 0,
        .DIR_LstAccDate = 0,
        .DIR_FstClusHI = 0,
        .DIR_WrtTime = 0,
        .DIR_WrtDate = 0,
        .DIR_FstClusLO = 3,
        .DIR_FileSize = 0,  // Directories have 0 file size
    };

    uint16_t create_time = 0, create_date = 0;
    get_fat_dir_entry_time_date(&create_time, &create_date);

    dir_ent.DIR_CrtTime = create_time;
    dir_ent.DIR_CrtDate = create_date;
    dir_ent.DIR_WrtTime = create_time;
    dir_ent.DIR_WrtDate = create_date;

    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    // /EFI Directory entries
    fseek(image, (fat32_data_lba + 1) * lba_size, SEEK_SET);

    memcpy(dir_ent.DIR_Name, ".          ", 11);    // "." dir entry, this directory itself
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "..         ", 11);    // ".." dir entry, parent dir (ROOT dir)
    dir_ent.DIR_FstClusLO = 0;                      // Root directory does not have a cluster value
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "BOOT       ", 11);    // /EFI/BOOT directory
    dir_ent.DIR_FstClusLO = 4;                      // /EFI/BOOT cluster
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    // /EFI/BOOT Directory entries
    fseek(image, (fat32_data_lba + 2) * lba_size, SEEK_SET);

    memcpy(dir_ent.DIR_Name, ".          ", 11);    // "." dir entry, this directory itself
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "..         ", 11);    // ".." dir entry, parent dir (/EFI dir)
    dir_ent.DIR_FstClusLO = 3;                      // /EFI directory cluster
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    return true;
}

// =============================
// Add a new directory or file to a given parent directory
// =============================
bool add_file_to_esp(char *file_name, FILE *image, File_Type type, uint32_t *parent_dir_cluster) {
    // First grab FAT32 filesystem info for VBR and File System info
    Vbr vbr = { 0 };
    fseek(image, esp_lba * lba_size, SEEK_SET);
    fread(&vbr, sizeof vbr, 1, image);

    FSInfo fsinfo = { 0 };
    fseek(image, (esp_lba + 1) * lba_size, SEEK_SET);
    fread(&fsinfo, sizeof fsinfo, 1, image);

    // Get file size if file
    FILE *new_file = NULL;
    uint64_t file_size_bytes = 0, file_size_lbas = 0;
    if (type == TYPE_FILE) {
        new_file = fopen(file_name, "rb");
        if (!new_file) return false;

        fseek(new_file, 0, SEEK_END);
        file_size_bytes = ftell(new_file);
        file_size_lbas = bytes_to_lbas(file_size_bytes);
        rewind(new_file);
    }

    // Get next free cluster in FATs
    uint32_t next_free_cluster = fsinfo.FSI_Nxt_Free;
    const uint32_t starting_cluster = next_free_cluster;  // Starting cluster for new dir/file

    // Add new clusters to FATs
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
        fseek(image, (fat32_fats_lba + (i * vbr.BPB_FATSz32)) * lba_size, SEEK_SET);
        fseek(image, next_free_cluster * sizeof next_free_cluster, SEEK_CUR);

        uint32_t cluster = fsinfo.FSI_Nxt_Free;
        next_free_cluster = cluster;
        if (type == TYPE_FILE) {
            for (uint64_t lba = 0; lba < file_size_lbas - 1; lba++) {
                cluster++;  // Each cluster points to next cluster of file data
                next_free_cluster++;
                fwrite(&cluster, sizeof cluster, 1, image);
            }
        }

        // Write EOC marker cluster, this would be the only cluster added for a directory
        //   (type == TYPE_DIR)
        cluster = 0xFFFFFFFF;
        next_free_cluster++;
        fwrite(&cluster, sizeof cluster, 1, image);
    }

    // Update next free cluster in FS Info
    fsinfo.FSI_Nxt_Free = next_free_cluster;
    fseek(image, (esp_lba + 1) * lba_size, SEEK_SET);
    fwrite(&fsinfo, sizeof fsinfo, 1, image); 

    // Go to Parent Directory's data location in data region
    fseek(image, (fat32_data_lba + *parent_dir_cluster - 2) * lba_size, SEEK_SET);

    // Add new directory entry for this new dir/file at end of current dir_entrys 
    FAT32_Dir_Entry_Short dir_entry = { 0 };

    fread(&dir_entry, 1, sizeof dir_entry, image);
    while (dir_entry.DIR_Name[0] != '\0')
        fread(&dir_entry, 1, sizeof dir_entry, image);

    // sizeof dir_entry = 32, back up to overwrite this empty spot
    fseek(image, -32, SEEK_CUR);    

    // Check name length for FAT 8.3 naming
    const char *dot_pos = strchr(file_name, '.');
    const uint32_t name_len = strlen(file_name);
    if ((!dot_pos && name_len > 11) || 
        (dot_pos && name_len > 12)  || 
        (dot_pos && dot_pos - file_name > 8)) {
        return false;   // Name is too long or invalid
    }
    
    // Convert name to FAT 8.3 naming
    // e.g. "FOO.BAR"  -> "FOO     BAR",
    //      "BA.Z"     -> "BA      Z  ",
    //      "ELEPHANT" -> "ELEPHANT   "
    memset(dir_entry.DIR_Name, ' ', 11);    // Start with all spaces, name/ext will be space padded

    if (dot_pos) {
        uint8_t i = 0;
        // Name 8 portion of 8.3
        for (i = 0; i < (dot_pos - file_name); i++)
            dir_entry.DIR_Name[i] = file_name[i];

        uint8_t j = i;
        while (i < 8) dir_entry.DIR_Name[i++] = ' ';

        if (file_name[j] == '.') j++;   // Skip dot to get to extension

        // Extension 3 portion of 8.3
        while (file_name[j])
            dir_entry.DIR_Name[i++] = file_name[j++];

        while (i < 11) dir_entry.DIR_Name[i++] = ' ';
    } else {
        memcpy(dir_entry.DIR_Name, file_name, name_len);
    }

    if (type == TYPE_DIR) 
        dir_entry.DIR_Attr = ATTR_DIRECTORY;

    uint16_t fat_time, fat_date;
    get_fat_dir_entry_time_date(&fat_time, &fat_date);
    dir_entry.DIR_CrtTime = fat_time;
    dir_entry.DIR_CrtDate = fat_date;
    dir_entry.DIR_WrtTime = fat_time;
    dir_entry.DIR_WrtDate = fat_date;

    dir_entry.DIR_FstClusHI = (starting_cluster >> 16) & 0xFFFF;
    dir_entry.DIR_FstClusLO = starting_cluster & 0xFFFF;

    if (type == TYPE_FILE)
        dir_entry.DIR_FileSize = file_size_bytes;

    fwrite(&dir_entry, 1, sizeof dir_entry, image);

    // Go to this new file's cluster's data location in data region
    fseek(image, (fat32_data_lba + starting_cluster - 2) * lba_size, SEEK_SET);

    // Add new file data
    // For directory add dir_entrys for "." and ".."
    if (type == TYPE_DIR) {
        memcpy(dir_entry.DIR_Name, ".          ", 11);  // "." dir_entry; this directory itself
        fwrite(&dir_entry, 1, sizeof dir_entry, image);

        memcpy(dir_entry.DIR_Name, "..         ", 11);  // ".." dir_entry; parent directory
        dir_entry.DIR_FstClusHI = (*parent_dir_cluster >> 16) & 0xFFFF;
        dir_entry.DIR_FstClusLO = *parent_dir_cluster & 0xFFFF;
        fwrite(&dir_entry, 1, sizeof dir_entry, image);
    } else {
        // For file, add file data
        uint8_t *file_buf = calloc(1, lba_size);
        for (uint64_t i = 0; i < file_size_lbas; i++) {
            // In case last lba is less than a full lba in size, use actual bytes read
            //   to write file to disk image
            size_t bytes_read = fread(file_buf, 1, lba_size, new_file);
            fwrite(file_buf, 1, bytes_read, image);
        }
        free(file_buf);
    }

    // Set dir_cluster for new parent dir, if a directory was just added
    if (type == TYPE_DIR)
        *parent_dir_cluster = starting_cluster;

    return true;
}

// =============================
// Add a file path to the EFI System Partition;
//   will add new directories if not found, and
//   new file at end of path
// =============================
bool add_path_to_esp(char *path, FILE *image) {
    // Parse input path for each name
    if (*path != '/') return false; // Path must begin with root '/'

    File_Type type = TYPE_DIR;
    char *start = path + 1; // Skip initial slash
    char *end = start;
    uint32_t dir_cluster = 2;   // Next directory's cluster location; start at root

    // Get next name from path, until reached end of path for file to add
    while (type == TYPE_DIR) {
        while (*end != '/' && *end != '\0') end++;

        if (*end == '/') type = TYPE_DIR;
        else             type = TYPE_FILE;  // Reached end of path

        *end = '\0';    // Null terminate next name in case of directory

        // Search for name in current directory's file data (dir_entrys)
        FAT32_Dir_Entry_Short dir_entry = { 0 };
        bool found = false;
        fseek(image, (fat32_data_lba + dir_cluster - 2) * lba_size, SEEK_SET);
        do {
            fread(&dir_entry, 1, sizeof dir_entry, image);
            if (!memcmp(dir_entry.DIR_Name, start, strlen(start))) {
                // Found name in directory, save cluster for last directory found
                dir_cluster = (dir_entry.DIR_FstClusHI << 16) | dir_entry.DIR_FstClusLO;
                found = true;
                break;
            }
        } while (dir_entry.DIR_Name[0] != '\0');

        if (!found) {
            // Add new directory or file to last found directory
            if (!add_file_to_esp(start, image, type, &dir_cluster))
                return false;
        }

        *end++ = '/';
        start = end;
    }

    // Show info to user
    printf("Added '%s'\n", path);

    return true;
}

// =============================
// Add disk image info file to hold at minimum the size of this disk image
// =============================
bool add_disk_image_info_file(FILE *image) {
    char *file_buf = calloc(1, lba_size);
    snprintf(file_buf, 
             lba_size,
             "DISK_SIZE=%"PRIu64"\n", 
             image_size);

    FILE *fp = fopen("DSKIMG.INF", "wb");
    if (!fp) return false;

    fwrite(file_buf, strlen(file_buf), 1, fp);
    fclose(fp);
    free(file_buf);

    char path[25] = { 0 };
    strcpy(path, "/EFI/BOOT/DSKIMG.INF");
    if (!add_path_to_esp(path, image)) return false;

    return true;
}

// =============================
// MAIN
// =============================
int main(void) {
    FILE *image = fopen(image_name, "wb+");
    if (!image) {
        fprintf(stderr, "Error: could not open file %s\n", image_name);
        return EXIT_FAILURE;
    }

    FILE *fp = NULL;

    // Set sizes & LBA values
    gpt_table_lbas = GPT_TABLE_SIZE / lba_size;
    const uint64_t padding = (ALIGNMENT*2 + (lba_size * ((gpt_table_lbas*2) + 1 + 2))); 
    image_size = esp_size + data_size + padding; 
    image_size_lbas = bytes_to_lbas(image_size);
    align_lba = ALIGNMENT / lba_size;
    esp_lba = align_lba;
    esp_size_lbas = bytes_to_lbas(esp_size);
    data_size_lbas = bytes_to_lbas(data_size);
    data_lba = next_aligned_lba(esp_lba + esp_size_lbas);

    // Seed random number generation
    srand(time(NULL));

    // Write protective MBR
    if (!write_mbr(image)) {
        fprintf(stderr, "Error: could not write protective MBR for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Write GPT headers & tables
    if (!write_gpts(image)) {
        fprintf(stderr, "Error: could not write GPT headers & tables for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Write EFI System Partition w/FAT32 filesystem
    if (!write_esp(image)) {
        fprintf(stderr, "Error: could not write ESP for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Check if "BOOTX64.EFI" file exists in current directory, if so automatically
    //   add it to the ESP
    fp = fopen("BOOTX64.EFI", "rb"); 
    if (fp) {
        fclose(fp);
        char *path = calloc(1, 25);
        strcpy(path, "/EFI/BOOT/BOOTX64.EFI");
        if (!add_path_to_esp(path, image)) 
            fprintf(stderr, "Error: Could not add file '%s'\n", path);
        free(path);
    }

    // Add disk image info file to hold at minimum the size of this disk image;
    //   this could be used in an EFI application later as part of an installer, for example
    if (!add_disk_image_info_file(image)) 
        fprintf(stderr, "Error: Could not add disk image info file to '%s'\n", image_name);

    return EXIT_SUCCESS;
}













