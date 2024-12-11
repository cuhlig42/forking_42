#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <immintrin.h> // For AVX2 intrinsics
#include <stdlib.h>

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)

#pragma pack(1)
struct bmp_header {
    i8 signature[2]; // "BM" signature
    u32 file_size;
    u32 unused_0;
    u32 data_offset;

    u32 info_header_size;
    u32 width;
    u32 height;
    u16 number_of_planes;
    u16 bit_per_pixel;
    u32 compression_type;
    u32 compressed_image_size;
};

struct file_content {
    i8* data;
    u32 size;
};

struct file_content read_entire_file(char* filename) {
    char* file_data = NULL;
    unsigned long file_size = 0;
    int input_file_fd = open(filename, O_RDONLY);
    if (input_file_fd >= 0) {
        struct stat input_file_stat = {0};
        stat(filename, &input_file_stat);
        file_size = input_file_stat.st_size;
        file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
        close(input_file_fd);
    }
    return (struct file_content){file_data, file_size};
}

void decode_message(struct file_content file_content) {
    struct bmp_header* header = (struct bmp_header*)file_content.data;
    if (header->bit_per_pixel != 32) {
        PRINT_ERROR("Unsupported BMP format. Only 32-bit BMP files are supported.\n");
        return;
    }

    u8* pixel_data = (u8*)file_content.data + header->data_offset;
    u32 width = header->width;
    u32 height = header->height;
    u32 row_size = ((width * 4 + 3) & ~3);

    u8* header_pixel = NULL;
    u32 header_x = 0, header_y = 0;

    // Find the header pixel (RGB: 127, 188, 217)
    for (u32 y = height - 1; y < height; --y) {
        for (u32 x = 0; x < width; ++x) {
            u8* current_pixel = &pixel_data[y * row_size + x * 4];
            if (current_pixel[0] == 127 && current_pixel[1] == 188 && current_pixel[2] == 217) {
                header_pixel = current_pixel;
                header_x = x;
                header_y = y;
                break;
            }
        }
        if (header_pixel) break;
    }

    if (!header_pixel) {
        PRINT_ERROR("Header not found.\n");
        return;
    }

    // Decode the message length from the red and blue channels of the rightmost pixel in the header row
    u8* length_pixel = &pixel_data[header_y * row_size + (width - 1) * 4];
    u32 message_length = length_pixel[0] + length_pixel[2]; // Red + Blue channels

    printf("Detected message length: %u\n", message_length);

    u32 max_message_length = (width * height) - (header_y * width + header_x);
    if (message_length > max_message_length) {
        PRINT_ERROR("Invalid message length detected.\n");
        return;
    }

    char* message = (char*)malloc(message_length + 1);
    if (!message) {
        PRINT_ERROR("Memory allocation failed for message\n");
        return;
    }

    u32 current_x = header_x + 1;
    u32 current_y = header_y;

    // Decode the message from the image
    for (u32 i = 0; i < message_length; ++i) {
        if (current_x >= width) {
            current_x = 0;
            if (current_y == 0) {
                PRINT_ERROR("Reached the top of the image unexpectedly.\n");
                free(message);
                return;
            }
            current_y--;
        }

        u8* message_pixel = &pixel_data[current_y * row_size + current_x * 4];
        message[i] = message_pixel[0]; // Use the red channel for ASCII
        current_x++;
    }

    message[message_length] = '\0'; // Null-terminate the string
    printf("Decoded message: %s\n", message);
    free(message);
}



int main(int argc, char** argv) {
    if (argc != 2) {
        PRINT_ERROR("Usage: decode <input_filename>\n");
        return 1;
    }
    struct file_content file_content = read_entire_file(argv[1]);
    if (file_content.data == NULL) {
        PRINT_ERROR("Failed to read file\n");
        return 1;
    }
    struct bmp_header* header = (struct bmp_header*)file_content.data;
    printf("signature: %.2s\nfile_size: %u\ndata_offset: %u\ninfo_header_size: %u\nwidth: %u\nheight: %u\nplanes: %i\nbit_per_px: %i\ncompression_type: %u\ncompression_size: %u\n", header->signature, header->file_size, header->data_offset, header->info_header_size, header->width, header->height, header->number_of_planes, header->bit_per_pixel, header->compression_type, header->compressed_image_size);
    decode_message(file_content);
    return 0;
}
