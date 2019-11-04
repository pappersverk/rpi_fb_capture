#include <err.h>
#include <stdlib.h>

#include "capture.h"
#include "dithering.h"

static inline uint8_t get_greyscale_from_rgb565(uint16_t color) {
    uint8_t r = ((color & 0xF800) >> 11) << 3;
    uint8_t g = ((color & 0x7E0) >> 5) << 2;
    uint8_t b = ((color & 0x1F)) << 3;

    // Fast greyscale conversion
    return  ((r * 77) + (g * 151) + (b * 30)) >> 8;
}

static void alg_grayscale(const struct capture_info *info) {
    int width = info->capture_width;
    int height = info->capture_height;
    int stride = info->capture_stride;
    const uint16_t *image = info->buffer;
    int16_t * buffer = info->dithering_buffer;

    // Calc greyscale and calculate threshold
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            buffer[x] = get_greyscale_from_rgb565(image[x]);
        }

        buffer += width;
        image += stride;
    }
}

static void alg_floyd_steingberg(const struct capture_info *info) {
    int width = info->capture_width;
    int height = info->capture_height;
    int16_t * buffer = info->dithering_buffer;

    alg_grayscale(info);

    // Apply Floyd-Steinberg dithering
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel_offset = y * width + x;
            int16_t old_pixel = buffer[pixel_offset];
            int16_t new_pixel = old_pixel < 127 ? 0 : 255;
            int16_t q_err = old_pixel - new_pixel;

            buffer[pixel_offset] = new_pixel;

            if (x + 1 < width) buffer[pixel_offset + 1] += (q_err * 7) >> 4;
            if (y + 1 == height) continue;
            if (x > 0) buffer[pixel_offset + width - 1] += (q_err * 3) >> 4;
            buffer[pixel_offset + width] += (q_err * 5) >> 4;
            if (x + 1 < width) buffer[pixel_offset + width + 1] += (q_err * 1) >> 4;
        }
    }
}

static void alg_sierra(const struct capture_info *info) {
    int width = info->capture_width;
    int height = info->capture_height;
    int16_t * buffer = info->dithering_buffer;

    alg_grayscale(info);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel_offset = y * width + x;
            int16_t old_pixel = buffer[pixel_offset];
            int16_t new_pixel = old_pixel < 127 ? 0 : 255;
            int16_t q_err = old_pixel - new_pixel;

            buffer[pixel_offset] = new_pixel;

            if (x + 1 < width) buffer[pixel_offset + 1] += (q_err * 5) >> 5;
            if (x + 2 < width) buffer[pixel_offset + 2] += (q_err * 3) >> 5;
            if (y + 1 == height) continue;
            if (x > 1) buffer[pixel_offset + width - 2] += (q_err * 2) >> 5;
            if (x > 0) buffer[pixel_offset + width - 1] += (q_err * 4) >> 5;
            buffer[pixel_offset + width] += (q_err * 5) >> 5;
            if (x + 1 < width) buffer[pixel_offset + width + 1] += (q_err * 4) >> 5;
            if (x + 2 < width) buffer[pixel_offset + width + 2] += (q_err * 2) >> 5;
            if (y + 2 == height) continue;
            if (x > 0) buffer[pixel_offset + (width * 2) - 1] += (q_err * 2) >> 5;
            buffer[pixel_offset + (width * 2)] += (q_err * 3) >> 5;
            if (x + 1 < width) buffer[pixel_offset + (width * 2) + 1] += (q_err * 4) >> 5;
        }
    }
}

static void alg_sierra_2row(const struct capture_info *info) {
    int width = info->capture_width;
    int height = info->capture_height;
    int16_t * buffer = info->dithering_buffer;

    alg_grayscale(info);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel_offset = y * width + x;
            int16_t old_pixel = buffer[pixel_offset];
            int16_t new_pixel = old_pixel < 127 ? 0 : 255;
            int16_t q_err = old_pixel - new_pixel;

            buffer[pixel_offset] = new_pixel;

            if (x + 1 < width) buffer[pixel_offset + 1] += (q_err * 4) >> 4;
            if (x + 2 < width) buffer[pixel_offset + 2] += (q_err * 3) >> 4;
            if (y + 1 == height) continue;
            if (x > 1) buffer[pixel_offset + width - 2] += (q_err * 1) >> 4;
            if (x > 0) buffer[pixel_offset + width - 1] += (q_err * 2) >> 4;
            buffer[pixel_offset + width] += (q_err * 3) >> 4;
            if (x + 1 < width) buffer[pixel_offset + width + 1] += (q_err * 2) >> 4;
            if (x + 2 < width) buffer[pixel_offset + width + 2] += (q_err * 1) >> 4;
        }
    }
}

static void alg_sierra_lite(const struct capture_info *info) {
    int width = info->capture_width;
    int height = info->capture_height;
    int16_t * buffer = info->dithering_buffer;

    alg_grayscale(info);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel_offset = y * width + x;
            int16_t old_pixel = buffer[pixel_offset];
            int16_t new_pixel = old_pixel < 127 ? 0 : 255;
            int16_t q_err = old_pixel - new_pixel;

            buffer[pixel_offset] = new_pixel;

            if (x + 1 < width) buffer[pixel_offset + 1] += (q_err * 2) >> 2;
            if (y + 1 == height) continue;
            if (x > 0) buffer[pixel_offset + width - 1] += (q_err * 1) >> 2;
            buffer[pixel_offset + width] += (q_err * 1) >> 2;
        }
    }
}

void dithering_apply(const struct capture_info *info) {
    switch (info->dithering) {
    case DITHERING_NONE:
        break;

    case DITHERING_FLOYD_STEINBERG:
        alg_floyd_steingberg(info);
        break;

    case DITHERING_SIERRA:
        alg_sierra(info);
        break;

    case DITHERING_SIERRA_2ROW:
        alg_sierra_2row(info);
        break;

    case DITHERING_SIERRA_LITE:
        alg_sierra_lite(info);
        break;

    default:
        errx(EXIT_FAILURE, "Unknown dithering algorithm");
        break;
    }
}
