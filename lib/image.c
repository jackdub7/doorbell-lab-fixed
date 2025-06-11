#include <stdlib.h>
#include <string.h>

#include "image.h"

uint8_t create_bmp(Bitmap *dst, uint8_t *src) {
    int offset = 0;
    dst->img = NULL;

    // Copy in the bitmap file header
    memcpy(dst->file_header, src, BMP_FILE_HEADER_SIZE);
    offset += BMP_FILE_HEADER_SIZE;

    // Get filesize
    dst->file_size = (dst->file_header[5] << 8 * 3) | (dst->file_header[4] << 8 * 2) |
                     (dst->file_header[3] << 8 * 1) | (dst->file_header[2]);

    // Get start location of pixel data
    dst->pxl_data_offset = (dst->file_header[13] << 8 * 3) | (dst->file_header[12] << 8 * 2) |
                           (dst->file_header[11] << 8 * 1) | (dst->file_header[10]);

    // // Get DIB header data
    uint8_t dib_header_size = dst->pxl_data_offset - BMP_FILE_HEADER_SIZE;
    dst->dib_header = (uint8_t *)malloc((dib_header_size) * sizeof(uint8_t));
    memcpy(dst->dib_header, src + offset, dib_header_size);
    offset += dib_header_size;

    // Get image width
    dst->img_width = (dst->dib_header[7] << 8 * 3) | (dst->dib_header[6] << 8 * 2) |
                     (dst->dib_header[5] << 8 * 1) | (dst->dib_header[4]);

    // Get image height
    dst->img_height = (dst->dib_header[11] << 8 * 3) | (dst->dib_header[10] << 8 * 2) |
                      (dst->dib_header[9] << 8 * 1) | (dst->dib_header[8]);

    if (dst->img_height < 0) {
        dst->img_height *= -1;
    }

    // Get image data
    dst->pxl_data_size = dst->img_width * dst->img_height * 3;
    dst->pxl_data = malloc(sizeof(uint8_t) * dst->pxl_data_size);
    memcpy(dst->pxl_data, src + offset, dst->pxl_data_size);

    // Create copy of image data for reset
    dst->pxl_data_cpy = malloc(sizeof(uint8_t) * dst->pxl_data_size);
    memcpy(dst->pxl_data_cpy, dst->pxl_data, dst->pxl_data_size);

    return LOAD_SUCCESS;
}

void destroy_bmp(Bitmap *bmp) {
    free(bmp->dib_header);
    free(bmp->pxl_data);
    free(bmp->pxl_data_cpy);
}

void reset_pixel_data(Bitmap *bmp) {
    bmp->pxl_data = memcpy(bmp->pxl_data, bmp->pxl_data_cpy, bmp->pxl_data_size);
}

uint8_t *get_pxl_data(Bitmap *bmp) { return bmp->pxl_data; }

uint8_t *get_original_pxl_data(Bitmap *bmp) { return bmp->pxl_data_cpy; }

void remove_color_channel(Color color, Bitmap *bmp) {
    for (uint32_t i = 0; i < bmp->pxl_data_size; i += 3) {
        if (color == RED_CHANNEL) {
            bmp->pxl_data[i + 2] = 0; // Red
        } else if (color == GREEN_CHANNEL) {
            bmp->pxl_data[i + 1] = 0; // Green
        } else if (color == BLUE_CHANNEL) {
            bmp->pxl_data[i + 0] = 0; // Blue
        }
    }
}

void or_filter(Bitmap *bmp) {
    for (uint32_t y = 0; y < bmp->img_height; y++) {
        for (uint32_t x = 0; x < bmp->img_width; x++) {
            uint32_t idx = (y * bmp->img_width + x) * 3;

            uint8_t r = bmp->pxl_data_cpy[idx + 2];
            uint8_t g = bmp->pxl_data_cpy[idx + 1];
            uint8_t b = bmp->pxl_data_cpy[idx + 0];

            // Top neighbor
            if (y > 0) {
                uint32_t t_idx = ((y - 1) * bmp->img_width + x) * 3;
                r |= bmp->pxl_data_cpy[t_idx + 2];
                g |= bmp->pxl_data_cpy[t_idx + 1];
                b |= bmp->pxl_data_cpy[t_idx + 0];
            }

            // Bottom neighbor
            if (y < bmp->img_height - 1) {
                uint32_t b_idx = ((y + 1) * bmp->img_width + x) * 3;
                r |= bmp->pxl_data_cpy[b_idx + 2];
                g |= bmp->pxl_data_cpy[b_idx + 1];
                b |= bmp->pxl_data_cpy[b_idx + 0];
            }

            // Write back to actual pixel data
            bmp->pxl_data[idx + 2] = r;
            bmp->pxl_data[idx + 1] = g;
            bmp->pxl_data[idx + 0] = b;
        }
    }
}
