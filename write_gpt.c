#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* 
 * Sources:
 * --------
 * https://en.wikipedia.org/wiki/GUID_Partition_Table
 * https://en.wikipedia.org/wiki/Universally_unique_identifier
 * https://en.wikipedia.org/wiki/Master_boot_record
 * https://www.rfc-editor.org/rfc/rfc4122
 * https://en.wikipedia.org/wiki/Cyclic_redundancy_check
 * https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks
 * https://www.w3.org/TR/PNG/#D-CRCAppendix
 * https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
 * https://wiki.osdev.org/FAT
 * UEFI 2.9 spec 
 * and 'mkgpt' source 
 */ 

// MBR partition entry
typedef struct {
    uint8_t status_or_phys_drive;
    uint8_t first_abs_sector_CHS[3];
    uint8_t partition_type;             // Should be 0xEE Protective MBR, 0xEF for EFI System Partition
    uint8_t last_abs_sector_CHS[3];
    uint32_t first_abs_sector_LBA;
    uint32_t num_sectors;
} __attribute__ ((packed)) mbr_partition_entry_t;

// MBR 
typedef struct {
    uint8_t boot_code[446]; 
    mbr_partition_entry_t partition_entries[4];
    uint16_t boot_signature;    // Should be 0xAA55
} __attribute__ ((packed)) mbr_t;

// GPT GUID - BE values are in byte arrays, LE values are not
typedef struct {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t  clock_seq_hi_and_reserved;
    uint8_t  clock_seq_low;
    uint8_t  node[6];
} __attribute__ ((packed)) gpt_guid_t;

// Partition table header
typedef struct {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved_1;
    uint64_t current_LBA;
    uint64_t backup_LBA;
    uint64_t first_usable_LBA;
    uint64_t last_usable_LBA;
    gpt_guid_t disk_GUID;
    uint64_t partition_entries_start_LBA;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
    uint8_t reserved_2[420];                            // 420 bytes, 420 + 92 byte header = 512 bytes or 1 sector/LBA
} __attribute__ ((packed)) partition_table_header_t;

// Partition table entries
typedef struct {
    gpt_guid_t partition_type_GUID;
    gpt_guid_t partition_GUID;
    uint64_t first_LBA;
    uint64_t last_LBA;
    uint64_t attribute_flags;
    uint16_t partition_name[36];    // UTF-16LE
} __attribute__ ((packed)) partition_entry_t;

// FAT32 boot sector/VBR
typedef struct {
    uint8_t jmp_inst[3];
    uint8_t oem_name[8];

    // DOS 2.0 BPB
    uint16_t bytes_per_logical_sector;  
    uint8_t logical_sectors_per_cluster;
    uint16_t reserved_logical_sectors;
    uint8_t  num_FATs;
    uint16_t max_fat12_fat16_root_dir_entries;  // FAT32 should = 0
    uint16_t total_logical_sectors_20;          // FAT32 should = 0
    uint8_t media_descriptor;
    uint16_t logical_sectors_per_FAT_BPB;       // Sectors per FAT - FAT12/16. FAT32 should = 0

    // DOS 3.31 BPB
    uint16_t phys_sectors_per_track;
    uint16_t heads_per_disk;
    uint32_t hidden_sectors_before_FAT;
    uint32_t total_logical_sectors_331;

    // FAT32 EPBP
    uint32_t logical_sectors_per_FAT_EBPB;      // Sectors per FAT - FAT32
    uint16_t drive_desc_mirror_flags;
    uint16_t version;
    uint32_t cluster_of_root_dir_start;
    uint16_t logical_sector_of_FS_info;
    uint16_t first_logical_sector_of_FAT32_boot_sector_copies;
    uint8_t reserved_1[12];
    uint8_t phys_drive_number;
    uint8_t reserved_2;
    uint8_t extended_boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t file_system_type[8];

    uint8_t boot_code[420];

    uint16_t boot_sector_signature;        // 0xAA55
} __attribute__ ((packed)) vbr_t;

// FAT32 FS Info sector
typedef struct {
    uint8_t signature_1[4];   // Should be "RRaA"
    uint8_t reserved_1[480];
    uint8_t signature_2[4];   // Should be "rrAa"
    uint32_t last_known_free_data_clusters; // Can be 0xFFFFFFFF if unknown or on format
    uint32_t most_recent_known_allocated_data_cluster;
    uint8_t reserved_2[12];
    uint8_t signature_3[4];   // Should be 0x00 0x00 0x55 0xAA
} __attribute__ ((packed)) fs_info_t;

// FAT32 directory entries
typedef struct {
    uint8_t file_name[8];
    uint8_t file_ext[3];
    uint8_t attributes;
    uint8_t windowsNT_reserved;
    uint8_t create_time_tenths_of_second;
    uint16_t create_time;                   // hour - 5 bits, minute - 6 bits, seconds - 5 bits (multiply by 2!)
    uint16_t create_date;                   // year since 1980 - 7 bits, month - 4 bits, day - 5 bits
    uint16_t last_accessed_date;            // year since 1980  - 7 bits, month - 4 bits, day - 5 bits
    uint16_t first_cluster_hi; 
    uint16_t last_modified_time;            // hour - 5 bits, minute - 6 bits, seconds - 5 bits (multiply by 2!)
    uint16_t last_modified_date;            // year since 1980 - 7 bits, month - 4 bits, day - 5 bits
    uint16_t first_cluster_low;
    uint32_t file_size_in_bytes;
} __attribute__ ((packed)) fat32_dir_entry_short_name_t;

typedef struct {
    uint8_t order;      // If masked with 0x40, this is the last long name entry before the dir entry
    uint8_t name_1[10]; // First 5 2-byte characters
    uint8_t attribute;  // Always = 0x0F (Long file name attribute)
    uint8_t type;       // 0 = name entry
    uint8_t checksum;   // Checksum of name in short name dir entry
    uint8_t name_2[12]; // 2-byte characters 6-11 of name
    uint16_t zero;      // Must be 0
    uint8_t name_3[4];  // 2-byte characters 12-13 of name
} __attribute__ ((packed)) fat32_dir_entry_long_name_t;

typedef enum {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN    = 0x02,
    ATTR_SYSTEM    = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE   = 0x20,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID,
} fat32_dir_entry_file_attributes;

// Command line arguments/options
typedef struct {
    bool error;
    bool help;
    bool update_efi;
    bool update_data;
    bool add_data;
    char efi_file_name[8]; // FAT32 8.3 short file name
    FILE *image_file;
    FILE *efi_file;
    FILE *data_file;
} options_t;

const gpt_guid_t efi_system_partition = {0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, {0x00,0xA0,0xC9,0x3E,0xC9,0x3B}};
const gpt_guid_t microsoft_basic_data_partition = {0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, {0x68,0xB6,0xB7,0x26,0x99,0xC7}};
const size_t sector_size = 512;

// Instead of uint64_t, this is needed to use %llu on both windows and linux
unsigned long long efi_size_sectors = 0x14000;  // Default EFI system partition size in sectors: 40MB
unsigned long long data_size_sectors = 0x6B800; // Default data partition size in sectors: 215MB

// Can also use premade table
//uint32_t crc32_table[256] = {
//    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
//    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
//    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
//    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
//    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
//    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
//    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
//    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
//    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
//    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
//    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
//    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
//    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
//    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
//    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
//    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
//    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
//    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
//    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
//    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
//    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
//    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
//    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
//    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
//    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
//    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
//    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
//    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
//    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
//    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
//    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
//    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
//};

uint32_t crc32_table[256];

// Generate CRC32 table
void gen_crc32_table(void) {
    uint32_t crc32 = 1;
    int i = 128;

    do {
        if (crc32 & 1) 
            crc32 = (crc32 >> 1) ^ 0xEDB88320; // The CRC polynomial - 32 bit
        else 
            crc32 >>= 1;

        // crc32 is the value of little endian table [i]; let j iterate over the already initialized entries
        for (int j = 0; j < 256; j += 2*i)
            crc32_table[i+j] = crc32 ^ crc32_table[j];

        i >>= 1;
    } while (i > 0);
}

// Return CRC32 value for chunk of data
uint32_t crc32(void *data, size_t data_length) {
    static bool made_table = false;
    uint8_t *ptr;
    size_t i;
    uint32_t crc32 = 0xFFFFFFFF;

    if (!made_table) {
        gen_crc32_table();
        made_table = true;
    }

    for (i = 0, ptr = data; i < data_length; i++, ptr++) 
        crc32 = (crc32 >> 8) ^ crc32_table[(uint8_t) crc32 ^ *ptr];

    // Finalize value by inverting bits
    return crc32 ^ 0xFFFFFFFF;
}

// Get a pseudo-randomly generated UUID (UUID v4)
gpt_guid_t get_guid(void) {
    uint8_t rand_array[16];

    for (int i = 0; i < 16; i++)
        rand_array[i] = rand() % (UINT8_MAX + 1);

    gpt_guid_t guid = {
        // Little endian
        .time_low = (rand_array[0] << 24) | (rand_array[1] << 16) | (rand_array[2] << 8) | rand_array[3],
        // Little endian
        .time_mid = (rand_array[4] << 8) | rand_array[5],
        // Little endian
        .time_hi_and_version = (rand_array[6] << 8) | rand_array[7],
        // Big endian
        .clock_seq_hi_and_reserved = rand_array[8],
        .clock_seq_low = rand_array[9],

        // Big endian
        .node = {rand_array[10], rand_array[11], rand_array[12], rand_array[13], rand_array[14], 
                 rand_array[15]},
    };

    // Set top 2 bits to 1 and 0
    guid.clock_seq_hi_and_reserved = (2 << 6) | (guid.clock_seq_hi_and_reserved >> 2);

    // Set top 4 bits to version (UUID v4, version = 4)
    guid.time_hi_and_version = (4 << 12) | (guid.time_hi_and_version >> 4);

    return guid;
}

// Get current datetime to fill out serial number/VBR volume ID
uint32_t get_volume_id(void) {
    time_t timestamp = time(NULL);
    struct tm *now = localtime(&timestamp);

    uint16_t top_1 = (now->tm_hour << 8) | now->tm_min;
    uint16_t top_2 = 1900 + now->tm_year;
    uint16_t bottom_1 = ((now->tm_mon + 1) << 8) | now->tm_mday;
    uint16_t bottom_2 = now->tm_sec << 8;

    return ((top_1 + top_2) << 16) | (bottom_1 + bottom_2);
}

// Get command line arguments/options
void get_opts(int argc, char *argv[], options_t *opts) {
    int image_argc = 0;

    if (argc == 1) return;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (opts->image_file != NULL) {
                fprintf(stderr, "Only 1 image_name can be specified\n");
                opts->error = true;
                return;
            }

            opts->image_file = fopen(argv[i], "r+"); // Open file at beginning, for reading & writing, but DO NOT TRUNCATE!

            if (!opts->image_file)
                opts->image_file = fopen(argv[i], "wb"); // New file

            image_argc = i; // Save argc value for later opts in case those opts work for new files

            printf("Writing image '%s'\n", argv[i]);
            continue;
        }

        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            opts->help = true;

        } else if (!strcmp(argv[i], "-ue") || !strcmp(argv[i], "--update-efi")) {
            // Update file in GPT image EFI system partition, /EFI/BOOT/ folder
            i++;
            if (!argv[i]) {
                fprintf(stderr, "Must provide file_name to update in EFI system partition\n");
                opts->error = true;
                return;
            } 

            opts->efi_file = fopen(argv[i], "rb");
            if (!opts->efi_file) {
                fprintf(stderr, "File '%s' does not exist.\n", argv[i]);
                opts->error = true;
                return;
            }

            if (image_argc == 0) {
                fprintf(stderr, "Must provide image_name to update file_name in\n");
                opts->error = true;
                return;
            }

            opts->update_efi = true;

            // Save file name for update_efi_file()
            char *dot = strchr(argv[i], '.');
            if (dot && (dot - argv[i] <= 8))
                strncpy(opts->efi_file_name, argv[i], dot - argv[i]); 
            else
                memcpy(opts->efi_file_name, argv[i], 8); // Save file name for update_efi_file()

            printf("Overwriting EFI/BOOT/ with file '%s'\n", argv[i]);

        } else if (!strcmp(argv[i], "-ud") || !strcmp(argv[i], "--update-data")) {
            // Update file in GPT image basic data partition
            i++;
            if (!argv[i]) {
                fprintf(stderr, "Must provide file_name to update in basic data partition\n");
                opts->error = true;
                return;
            } 

            opts->data_file = fopen(argv[i], "rb");
            if (!opts->data_file) {
                fprintf(stderr, "File '%s' does not exist.\n", argv[i]);
                opts->error = true;
                return;
            }

            if (image_argc == 0) {
                fprintf(stderr, "Must provide image_name to update file_name in\n");
                opts->error = true;
                return;
            }

            opts->update_data = true;
            printf("Overwriting data partition with file '%s'\n", argv[i]);

        } else if (!strcmp(argv[i], "-ad") || !strcmp(argv[i], "--add-data")) {
            // Add file to GPT image basic data partition at image creation
            i++;
            if (!argv[i]) {
                fprintf(stderr, "Must provide file_name to add to basic data partition\n");
                opts->error = true;
                return;
            }

            opts->data_file = fopen(argv[i], "rb");
            if (!opts->data_file) {
                fprintf(stderr, "File '%s' does not exist.\n", argv[i]);
                opts->error = true;
                return;
            }

            if (image_argc == 0) {
                // Did not provide name for image file yet
                fprintf(stderr, "Must provide image_name to add file_name to\n");
                opts->error = true;
                return;
            }

            if (opts->update_efi || opts->update_data) {
                fprintf(stderr, "Cannot update an image and add to new image. Try only updating or adding.\n");
                opts->error = true;
                return;
            }

            opts->add_data = true;
            printf("Adding file '%s' to basic data partition\n", argv[i]);

        } else if (!strcmp(argv[i], "-es") || !strcmp(argv[i], "--efi-size")) {
            // Set the size of the EFI system partition, in MB
            // MINIMUM SIZE OF ~32MB for FAT32! 512 bytes/sector * 1 sector/cluster * 65525 clusters = 33,548,800; 32MB = 33,554,432
            i++;
            if (!argv[i]) {
                fprintf(stderr, "Must provide efi_size in MB\n");
                opts->error = true;
                return;
            }

            const uint64_t size = strtol(argv[i], NULL, 0);

            if (size < 32) {
                fprintf(stderr, "efi_size must be >= 32 as 32MB is the minimum size of a valid FAT32 partition\n");
                opts->error = true;
                return;
            }

            // Set efi_size_sectors; Convert input MB to sectors (convert to MB and divide by 512 bytes)
            efi_size_sectors = (size*1048576)/512;
            
        } else if (!strcmp(argv[i], "-ds") || !strcmp(argv[i], "--data-size")) {
            // Set the size of the basic data partition, in MB (for your OS or other things)
            i++;
            if (!argv[i]) {
                fprintf(stderr, "Must provide data_size in MB\n");
                opts->error = true;
                return;
            }

            const uint64_t size = strtol(argv[i], NULL, 0);

            // Set data_size_sectors; Convert input MB to sectors (convert to MB and divide by 512 bytes)
            data_size_sectors = (size*1048576)/512;
        }
    }
}

void update_efi_file(FILE *image_file, FILE *efi_file, char *file_name) {
    /* 
     * NOTE: Assuming sectors per cluster is 1 and bytes per sector is 512. 
     * Change later to get these values as needed
     */
    partition_table_header_t gpt_header;
    partition_entry_t part;
    vbr_t vbr;
    uint64_t file_size, file_size_sectors;
    uint32_t cluster = 0;
    uint8_t file_buf[sector_size];
    fat32_dir_entry_short_name_t dir_entry;

    // Get LBA of EFI system partitition
    fseek(image_file, sizeof(mbr_t), SEEK_SET);
    assert(fread(&gpt_header, 1, sizeof gpt_header, image_file) == sizeof gpt_header);
    fseek(image_file, gpt_header.partition_entries_start_LBA*512, SEEK_SET);

    assert(fread(&part, 1, sizeof part, image_file) == sizeof part);
    while (memcmp(&part.partition_type_GUID, &efi_system_partition, sizeof(gpt_guid_t)) != 0) 
        assert(fread(&part, 1, sizeof part, image_file) == sizeof part);
    
    fseek(image_file, part.first_LBA*512, SEEK_SET);

    // Go to FATs
    assert(fread(&vbr, 1, sizeof vbr, image_file) == sizeof vbr);
    const uint64_t FAT1_LBA = part.first_LBA + vbr.reserved_logical_sectors;
    const uint64_t FAT2_LBA = FAT1_LBA + vbr.logical_sectors_per_FAT_EBPB;
    fseek(image_file, FAT1_LBA*512, SEEK_SET);

    // Write FAT32 clusters for efi_file data
    fseek(image_file, (sizeof cluster)*5, SEEK_CUR);    // Location of cluster 5+, after /EFI/BOOT/ directories

    fseek(efi_file, 0, SEEK_END);
    file_size = ftell(efi_file);
    file_size_sectors = file_size / 512;
    rewind(efi_file);

    // FAT1
    cluster = 0x00000006;   // Starting cluster for efi_file, this is cluster 5, but it contains the next cluster of file data
    for (uint32_t i = 0; i < file_size_sectors; i++, cluster++) 
        fwrite(&cluster, sizeof cluster, 1, image_file); 

    if (file_size % 512 > 0) // Partial sector data
        fwrite(&cluster, sizeof cluster, 1, image_file); 

    // Overwrite remaining clusters with 0s, in case this efi_file is smaller than previous one
    assert(fread(&cluster, 1, sizeof cluster, image_file) == sizeof cluster);
    while (cluster != 0) {
        fseek(image_file, -4, SEEK_CUR); // Move back 1 cluster
        cluster = 0;
        fwrite(&cluster, sizeof cluster, 1, image_file);   // Overwrite with 0s
        assert(fread(&cluster, 1, sizeof cluster, image_file) == sizeof cluster);    // Move forward to next cluster
    }

    // FAT2
    fseek(image_file, FAT2_LBA*512, SEEK_SET);
    fseek(image_file, (sizeof cluster)*5, SEEK_CUR);    // Location of cluster 5+, after /EFI/BOOT/ directories

    cluster = 0x00000006;   // Starting cluster for efi_file, this is cluster 5, but it contains the next cluster of file data
    for (uint32_t i = 0; i < file_size_sectors; i++, cluster++) 
        fwrite(&cluster, sizeof cluster, 1, image_file); 

    if (file_size % 512 > 0) // Partial sector data
        fwrite(&cluster, sizeof cluster, 1, image_file); 

    // Overwrite remaining clusters with 0s, in case this efi_file is smaller than previous one
    assert(fread(&cluster, 1, sizeof cluster, image_file) == sizeof cluster);
    while (cluster != 0) {
        fseek(image_file, -4, SEEK_CUR); // Move back 1 cluster
        cluster = 0;
        fwrite(&cluster, sizeof cluster, 1, image_file);   // Overwrite with 0s
        assert(fread(&cluster, 1, sizeof cluster, image_file) == sizeof cluster);    // Move forward to next cluster
    }

    // Go to /EFI/BOOT/ directory in image_file
    const uint64_t root_dir_LBA = FAT2_LBA + vbr.logical_sectors_per_FAT_EBPB;
    fseek(image_file, (root_dir_LBA+2)*512, SEEK_SET);

    // Write directory entry for efi_file so it shows in UEFI shell and/or auto boots on startup
    time_t timestamp = time(NULL);
    struct tm *now = localtime(&timestamp);

    memcpy(dir_entry.file_name, file_name, 8);
    memcpy(dir_entry.file_ext, "EFI", 3);
    dir_entry.attributes = 0;
    dir_entry.create_time = (now->tm_hour << 11) | (now->tm_min << 6) | (now->tm_sec/2);
    dir_entry.create_date = (((now->tm_year + 1900)-1980) << 9) | ((now->tm_mon + 1) << 5) | now->tm_mday;
    dir_entry.first_cluster_hi  = 0;
    dir_entry.first_cluster_low = 0x5;
    dir_entry.file_size_in_bytes = file_size;

    fwrite(&dir_entry, sizeof dir_entry, 1, image_file);

    // Go to /EFI/BOOT/<efi_file> location in image_file
    fseek(image_file, (root_dir_LBA+3)*512, SEEK_SET); 

    // Write efi_file data
    for (uint32_t i = 0; i < file_size_sectors; i++) {
        assert(fread(&file_buf, 1, sizeof file_buf, efi_file) == sizeof file_buf);
        fwrite(&file_buf, sizeof file_buf, 1, image_file);
    }

    if (file_size % 512 > 0) { // Partial sector data
        assert(fread(&file_buf, 1, (file_size % 512), efi_file) == (file_size % 512));
        fwrite(&file_buf, (file_size % 512), 1, image_file);
    }

    // NOTE: Check if following sectors are non-0 and overwrite with 0s, or not needed?
}

void update_data_file(FILE *image_file, FILE *data_file) {
    partition_table_header_t gpt_header;
    partition_entry_t part;
    uint64_t file_size, file_size_sectors;
    uint8_t file_buf[512] = {0};
    gpt_guid_t tmp_guid = microsoft_basic_data_partition;

    // Get LBA of basic data partitition
    fseek(image_file, sizeof(mbr_t), SEEK_SET);
    assert(fread(&gpt_header, 1, sizeof gpt_header, image_file) == sizeof gpt_header);
    fseek(image_file, gpt_header.partition_entries_start_LBA*512, SEEK_SET);

    assert(fread(&part, 1, sizeof part, image_file) == sizeof part);
    while (memcmp(&part.partition_type_GUID, &tmp_guid, sizeof(gpt_guid_t)) != 0) 
        assert(fread(&part, 1, sizeof part, image_file) == sizeof part);
    
    fseek(image_file, part.first_LBA*512, SEEK_SET);

    // Overwrite data partition with 0s
    const uint64_t data_size_LBAs = part.last_LBA - part.first_LBA + 1;
    for (uint64_t i = 0; i < data_size_LBAs; i++)
        fwrite(&file_buf, sizeof file_buf, 1, image_file);

    // Write data_file to data partition
    fseek(data_file, 0, SEEK_END);
    file_size = ftell(data_file);
    file_size_sectors = file_size / 512;
    rewind(data_file);

    fseek(image_file, part.first_LBA*512, SEEK_SET);

    for (uint64_t i = 0; i < file_size_sectors; i++) {
        assert(fread(&file_buf, 1, sizeof file_buf, data_file) == sizeof file_buf);
        fwrite(&file_buf, sizeof file_buf, 1, image_file);
    }

    if (file_size % 512 > 0) { // Partial sector data
        assert(fread(&file_buf, 1, (file_size % 512), data_file) == (file_size % 512));
        fwrite(&file_buf, (file_size % 512), 1, image_file);
    }
}

int main(int argc, char *argv[]) {
    /* NOTE: fopen uses "rb" & "wb" to read/write binary, which fixes some odd issues on byte conversions and file seeking here and there */
    const int first_usable_sector = 0x22;
    const int header_sectors = 16384/sector_size;   // Minimum size of GPT partition array
    const int part_count = 128;                     // Number of partition entries
    const uint64_t first_part_LBA = 0x80;
    const int max_opts = 9;
    const char help_text[] = "Usage: %s [-h --help] [image_name [-ue --update-efi file_name] [-ud --update-data file_name] [-ad --add-data file_name]\n"
                             "          [-es --efi-size efi_size] [-ds --data-size data_size]]\n"
                             "-h --help:         Print this message\n"
                             "image_name:        Name of output GPT disk image file\n"
                             "-ue --update-efi:  Update/overwrite file_name in the /EFI/BOOT/ directory of the EFI system partition\n"
                             "-ud --update-data: Update/overwrite file_name in the basic data partition\n"
                             "-ad --add-data:    Add file_name to the basic data partition in the created image file\n"
                             "-es --efi-size:    Set the size of the EFI System Partition in MB; Minimum size of 32 for FAT32\n"
                             "-ds --data-size:   Set the size of the Basic Data Partition in MB; Minimum size of 0 for empty/no data partition\n";

    uint32_t bootloader_file_size;
    int bootloader_file_size_sectors;
    options_t opts = {
        .error = false,
        .help = false,
        .update_efi = false,
        .update_data = false,
        .add_data = false,
        .image_file = NULL,
        .efi_file = NULL,
        .data_file = NULL,
    };

    if (argc > max_opts) {
        fprintf(stderr, help_text, argv[0]);
        return EXIT_FAILURE;
    }

    // Grab command line args
    get_opts(argc, argv, &opts);

    uint64_t image_sectors = efi_size_sectors + data_size_sectors + 0x800;    // 1MB buffer
    int secondary_headers_sector = image_sectors - 1 - header_sectors;
    int secondary_gpt_sector = image_sectors-1;

    if (opts.help) {
        fprintf(stderr, help_text, argv[0]);
        return EXIT_SUCCESS;
    }

    if (opts.error) return EXIT_FAILURE;

    if (!opts.image_file) {
        // Set default output image file name
        opts.image_file = fopen("test.img", "wb");
        puts("Writing default image 'test.img'");
    }

    if (opts.update_efi) {
        update_efi_file(opts.image_file, opts.efi_file, opts.efi_file_name);

        fclose(opts.efi_file);
        fclose(opts.image_file);
        return EXIT_SUCCESS;
    }

    if (opts.update_data) {
        update_data_file(opts.image_file, opts.data_file);

        fclose(opts.data_file);
        fclose(opts.image_file);
        return EXIT_SUCCESS;
    }

    // Writing new file from this point on, show size of partitions to user
    printf("EFI System Partition size: %lluMB, Basic Data Partition size: %lluMB\n", (efi_size_sectors*512)/1048576, (data_size_sectors*512)/1048576);

    // Seed rng for GUID values later
    time_t t;
    srand((unsigned) time(&t));

    // Write MBR (LBA 0) -----------------------------------
    mbr_t mbr = {
        .partition_entries[0] = {
            .status_or_phys_drive = 0,                  // Non-bootable partition
            .first_abs_sector_CHS = {0x00, 0x02, 0x00}, // 0x000200 or 512
            .partition_type = 0xEE,                     // GPT Protective MBR 
            .last_abs_sector_CHS = {0xFF, 0xFF, 0xFF},  // As large as can go to cover whole disk
            .first_abs_sector_LBA = 1,                  // LBA of GPT partition table header
            .num_sectors = image_sectors - 1,           // Size of disk in sectors - 1
        },
        .boot_signature = 0xAA55,
    };

    assert(fwrite(&mbr, 1, sizeof mbr, opts.image_file) == sector_size);

    // Define GPT headers ----------------------------------------
    partition_table_header_t partition_table_header = {
        .signature = "EFI PART",
        .revision = 0x00010000,                         // Header version 1.0
        .header_size = 92,
        .header_crc32 = 0,                              // Will be filled out later
        .current_LBA = 1,                               // LBA of this header on disk
        .backup_LBA = secondary_gpt_sector,             // Alternate header LBA (LBA -1)
        .first_usable_LBA = first_usable_sector,        
        .last_usable_LBA = secondary_headers_sector-1,  // LBA -34
        .disk_GUID = get_guid(),
        .partition_entries_start_LBA = 2,
        .num_partition_entries = part_count,
        .partition_entry_size = 128,
        .partition_entries_crc32 = 0,                   // Will be filled out later
    };

    partition_table_header_t backup_header;

    // Define GPT partition table entries --------------------------------
    partition_entry_t partition_entries[128] = {
        {
            .partition_type_GUID = efi_system_partition,
            .partition_GUID = get_guid(),
            .first_LBA = first_part_LBA,
            .last_LBA = first_part_LBA + efi_size_sectors - 1,
            .attribute_flags = 0,
            .partition_name = u"EFI System Partition",
        },
        {
            .partition_type_GUID = microsoft_basic_data_partition,
            .partition_GUID = get_guid(),
            .first_LBA = first_part_LBA + efi_size_sectors,
            .last_LBA = first_part_LBA + efi_size_sectors + data_size_sectors - 1, 
            .attribute_flags = 0,
            .partition_name = u"Empty Space",
        },
    };
    
    // CRC calculations for partition entries and headers
    partition_table_header.partition_entries_crc32 = crc32(partition_entries, part_count * 128);
    partition_table_header.header_crc32 = crc32(&partition_table_header, 92);  // Only run CRC for 92 byte header
    
    backup_header = partition_table_header;

    backup_header.header_crc32 = 0;                         // Zero out CRC before/during calculation
    backup_header.current_LBA = secondary_gpt_sector;
    backup_header.backup_LBA = 1;
    backup_header.partition_entries_start_LBA = secondary_headers_sector;
    backup_header.header_crc32 = crc32(&backup_header, 92); // Only run CRC for 92 byte header

    // Write primary GPT (LBA 1) and headers (LBA 2-33) --------------------------------
    assert(fwrite(&partition_table_header, 1, sizeof partition_table_header, opts.image_file) == sector_size);                                     
    assert(fwrite(partition_entries, 1, sizeof partition_entries, opts.image_file) == header_sectors * sector_size);

    // Write partitions ----------------------------------------
    // EFI system partition: FAT32 VBR
    vbr_t vbr = {
        .jmp_inst = {0xEB, 0xFE, 0x90},                 // jmp 0; nop 
        .oem_name = "Some OS ",

        // DOS 2.0 BPB
        .bytes_per_logical_sector = 512,
        .logical_sectors_per_cluster = 1,
        .reserved_logical_sectors = 32,
        .num_FATs = 2,
        .max_fat12_fat16_root_dir_entries = 0,
        .total_logical_sectors_20 = 0,
        .media_descriptor = 0xF8,                       // Used for "any fixed or removable media, where geometry is defined in the BPB
        .logical_sectors_per_FAT_BPB = 0,

        // DOS 3.31 BPB
        .phys_sectors_per_track = 32,
        .heads_per_disk = 64,
        .hidden_sectors_before_FAT = 0,
        .total_logical_sectors_331 = 0x14000,

        // FAT32 EPBP
        .drive_desc_mirror_flags = 0,
        .version = 0,
        .cluster_of_root_dir_start = 2,
        .logical_sector_of_FS_info = 1,
        .first_logical_sector_of_FAT32_boot_sector_copies = 6,
        .phys_drive_number = 0x80,
        .extended_boot_signature = 41,
        .volume_id = get_volume_id(),
        .volume_label = "Volume 0   ",
        .file_system_type = "FAT32   ",
        .boot_sector_signature = 0xAA55,
    };

    const uint32_t tmp_1 = vbr.total_logical_sectors_331 - vbr.reserved_logical_sectors;
    const uint32_t tmp_2 = ((256 * vbr.logical_sectors_per_cluster) + vbr.num_FATs) / 2;
    vbr.logical_sectors_per_FAT_EBPB = (tmp_1 + (tmp_2 - 1)) / tmp_2;   // Sectors per FAT

    // Write FAT32 VBR
    fseek(opts.image_file, first_part_LBA*512, SEEK_SET);   // Partition 0 First LBA
    assert(fwrite(&vbr, 1, sizeof(vbr_t), opts.image_file) == sector_size);

    // Write FAT32 FS Info sector
    fs_info_t fs_info = {
        .signature_1 = "RRaA",
        .signature_2 = "rrAa",
        .last_known_free_data_clusters = vbr.total_logical_sectors_331 - vbr.reserved_logical_sectors - 
            (vbr.logical_sectors_per_FAT_EBPB * vbr.num_FATs) - 3 - 12,
        .most_recent_known_allocated_data_cluster = 17,  // Next free sector is 2 + root dir + EFI/ + EFI/BOOT/ + EFI/BOOT/BOOTX64.EFI size in sectors/clusters
        .signature_3 = {0x00, 0x00, 0x55, 0xAA},
    };
    assert(fwrite(&fs_info, 1, sizeof(fs_info_t), opts.image_file) == sector_size);

    // Write Backup VBR, will skip over reserved sectors in between (they will be 0s)
    fseek(opts.image_file, (first_part_LBA + vbr.first_logical_sector_of_FAT32_boot_sector_copies)*512, SEEK_SET);
    assert(fwrite(&vbr, 1, sizeof vbr, opts.image_file) == sector_size);

    // Get bootloader file size
    FILE *bootloader = fopen("BOOTX64.EFI", "rb");
    bool add_bootloader = false;

    if (bootloader) {
        add_bootloader = true;
        puts("BOOTX64.EFI found. Adding to 'EFI/BOOT/' folder.");
    } else {
        puts("BOOTX64.EFI not found in current directory. ESP will have empty 'EFI/BOOT/' folder.");
    }

    // Write FAT values/clusters
    // Top 4 bits of each cluster are reserved in FAT32
    const uint64_t first_fat_sector = first_part_LBA + vbr.reserved_logical_sectors;
    fseek(opts.image_file, first_fat_sector*512, SEEK_SET);

    uint32_t cluster = 0;
    cluster = 0x0FFFFF00 | vbr.media_descriptor;        // Cluster 0 - bits 7-0 = fat ID
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);
    cluster = 0x0FFFFFFF;                               // Cluster 1 - end of cluster chain marker, or typically all bits set
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);

    cluster = 0x0FFFFFF8;
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);   // Cluster 2 - first available data cluster, root directory, end of cluster chain marker

    cluster = 0x0FFFFFFF;
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);   // Cluster 3 - EFI directory, end of cluster chain marker
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);   // Cluster 4 - BOOT directory, end of cluster chain marker

    if (add_bootloader) {
        // Get file size
        fseek(bootloader, 0, SEEK_END);
        bootloader_file_size = ftell(bootloader);
        bootloader_file_size_sectors = bootloader_file_size / 512;
        rewind(bootloader);

        // Clusters 5-size of BOOTX64.EFI in sectors - value of cluster = next cluster with file data
        cluster = 0x00000006;
        for (int i = 0; i < bootloader_file_size_sectors; i++) { 
            fwrite(&cluster, sizeof cluster, 1, opts.image_file);
            cluster++;    
        }

        if ((bootloader_file_size % 512) > 0) 
            fwrite(&cluster, sizeof cluster, 1, opts.image_file);

        // Overwrite last cluster with end of cluster chain
        fseek(opts.image_file, -4, SEEK_CUR);
        cluster = 0x0FFFFFFF; 
        fwrite(&cluster, sizeof cluster, 1, opts.image_file);
    }

    // Write 2nd FAT
    const uint64_t second_fat_sector = first_fat_sector + vbr.logical_sectors_per_FAT_EBPB;
    fseek(opts.image_file, second_fat_sector*512, SEEK_SET);

    cluster = 0x0FFFFF00 | vbr.media_descriptor;        // Cluster 0 - bits 7-0 = fat ID
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);
    cluster = 0x0FFFFFFF;                               // Cluster 1 - end of cluster chain marker, or typically all bits set
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);

    cluster = 0x0FFFFFF8;
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);   // Cluster 2 - first available data cluster, root directory, end of cluster chain marker

    cluster = 0x0FFFFFFF;
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);   // Cluster 3 - EFI directory, end of cluster chain marker
    fwrite(&cluster, sizeof cluster, 1, opts.image_file);   // Cluster 4 - BOOT directory, end of cluster chain marker

    if (add_bootloader) {
        // Clusters 5-size of BOOTX64.EFI in sectors - value of cluster = next cluster with file data
        cluster = 0x00000006;
        for (int i = 0; i < bootloader_file_size_sectors; i++) { 
            fwrite(&cluster, sizeof cluster, 1, opts.image_file);
            cluster++;    
        }

        if ((bootloader_file_size % 512) > 0) 
            fwrite(&cluster, sizeof cluster, 1, opts.image_file);

        // Overwrite last cluster with end of cluster chain
        fseek(opts.image_file, -4, SEEK_CUR);
        cluster = 0x0FFFFFFF; 
        fwrite(&cluster, sizeof cluster, 1, opts.image_file);
    }

    // Write FAT32 root dir entries --------------------------
    const uint64_t first_data_sector = second_fat_sector + vbr.logical_sectors_per_FAT_EBPB;

    // EFI subdirectory - cluster 3
    fseek(opts.image_file, first_data_sector*512, SEEK_SET);
    time_t timestamp = time(NULL);
    struct tm *now = localtime(&timestamp);
    fat32_dir_entry_short_name_t dir_entry = {
        .file_name = "EFI     ",
        .file_ext = "   ",
        .attributes = ATTR_DIRECTORY,   // EFI/ is a subdirectory
        .create_time = (now->tm_hour << 11) | (now->tm_min << 6) | (now->tm_sec/2),
        .create_date = (((now->tm_year + 1900)-1980) << 9) | ((now->tm_mon + 1) << 5) | now->tm_mday, 
        .first_cluster_hi = 0,
        .first_cluster_low = 0x3,
        .file_size_in_bytes = 0,
    };

    fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

    // EFI subdirectory entries --------------------------
    // EFI/BOOT/ subdirectory - cluster 4
    fseek(opts.image_file, (first_data_sector+1)*512, SEEK_SET);
    memcpy(dir_entry.file_name, ".       ", 8);             // Current directory
    dir_entry.first_cluster_low = 0x3;                      // This cluster
    fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

    memcpy(dir_entry.file_name, "..      ", 8);             // Parent directory
    dir_entry.first_cluster_low = 0x2;                      // Root directory
    fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

    memcpy(dir_entry.file_name, "BOOT    ", 8);  
    dir_entry.first_cluster_low = 0x4;
    fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

    // EFI/BOOT/ subdirectory entries -----------------------
    // BOOTX64.EFI file clusters 5+
    fseek(opts.image_file, (first_data_sector+2)*512, SEEK_SET);
    memcpy(dir_entry.file_name, ".       ", 8);             // Current directory
    dir_entry.first_cluster_low = 0x4;                      // This cluster
    fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

    memcpy(dir_entry.file_name, "..      ", 8);             // Parent directory
    dir_entry.first_cluster_low = 0x3;                      // EFI/ directory
    fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

    if (add_bootloader) {
        memcpy(dir_entry.file_name, "BOOTX64 ", 8);  
        memcpy(dir_entry.file_ext, "EFI", 3);
        dir_entry.attributes = 0;
        dir_entry.first_cluster_low = 0x5;
        dir_entry.file_size_in_bytes = bootloader_file_size;

        fwrite(&dir_entry, sizeof dir_entry, 1, opts.image_file);

        // Write BOOTX64.EFI file data
        fseek(opts.image_file, (first_data_sector+3)*512, SEEK_SET);

        uint8_t file_buf[512];
        for (int i = 0; i < bootloader_file_size_sectors; i++) {
            assert(fread(file_buf, 1, sizeof file_buf, bootloader) == sizeof file_buf);
            fwrite(file_buf, sizeof file_buf, 1, opts.image_file);
        }

        if ((bootloader_file_size % 512) > 0) {     // Write rest of partial sector data
            assert(fread(file_buf, 1, bootloader_file_size % 512, bootloader) == bootloader_file_size % 512);
            fwrite(file_buf, bootloader_file_size % 512, 1, opts.image_file);
        }

        // File cleanup
        fclose(bootloader);
    }

    // Write optional file to basic data partition
    if (opts.add_data) {
        uint8_t file_buf[512];
        uint64_t file_size, file_size_sectors;

        // Go to Data partition start
        fseek(opts.image_file, partition_entries[1].first_LBA*512, SEEK_SET); 

        // Get size of data file
        fseek(opts.data_file, 0, SEEK_END);
        file_size = ftell(opts.data_file);
        file_size_sectors = file_size / 512;
        rewind(opts.data_file);

        for (uint64_t i = 0; i < file_size_sectors; i++) {
            assert(fread(&file_buf, 1, sizeof file_buf, opts.data_file) == sizeof file_buf);
            fwrite(&file_buf, sizeof file_buf, 1, opts.image_file);
        }

        if (file_size % 512 > 0) { // Partial sector data
            assert(fread(&file_buf, 1, (file_size % 512), opts.data_file) == (file_size % 512));
            fwrite(&file_buf, (file_size % 512), 1, opts.image_file);
        }

        fclose(opts.data_file);
    }

    // Write secondary headers (LBA -2 to -33) & GPT (LBA -1) ---------------------------------------
    fseek(opts.image_file, secondary_headers_sector * sector_size, SEEK_SET);
    assert(fwrite(partition_entries, 1, sizeof partition_entries, opts.image_file) == header_sectors * sector_size);

    fseek(opts.image_file, secondary_gpt_sector * sector_size, SEEK_SET);
    assert(fwrite(&backup_header, 1, sizeof backup_header, opts.image_file) == sector_size);

    printf("Wrote %ld bytes successfully.\n", ftell(opts.image_file));
    
    // Final Cleanup -------------------------------------
    fclose(opts.image_file);
    
    // End program
    return EXIT_SUCCESS;
}
