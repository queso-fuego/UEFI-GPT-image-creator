#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// -------------------------------------
// Global Typedefs
// -------------------------------------
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

// -------------------------------------
// Global Variables
// -------------------------------------
char *image_name = "test.img";

uint64_t lba_size = 512;
uint64_t esp_size = 1024*1024*33;   // 33 MiB
uint64_t data_size = 1024*1024*1;   // 1 MiB
uint64_t image_size = 0;
uint64_t esp_lbas, data_lbas, image_size_lbas;

// =====================================
// Convert bytes to LBAs
// =====================================
uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes / lba_size) + (bytes % lba_size > 0 ? 1 : 0);
}

// =====================================
// Write protective MBR
// =====================================
bool write_mbr(FILE *image) {
    if (image_size_lbas > 0xFFFFFFFF) image_size_lbas = 0x100000000;

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
            .size_lba = image_size_lbas - 1,
        },
        .boot_signature = 0xAA55,
    };

    // Write to file
    if (fwrite(&mbr, 1, sizeof mbr, image) != sizeof mbr)
        return false;

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

    // Set sizes
    image_size = esp_size + data_size + (1024*1024); // Add some extra padding for GPTs/MBR
    image_size_lbas = bytes_to_lbas(image_size);

    if (!write_mbr(image)) {
        fprintf(stderr, "Error: could not protective MBR for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
