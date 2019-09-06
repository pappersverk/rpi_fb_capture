#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>

#define MAX_REQUEST_BUFFER_SIZE     256

struct capture_info {
    char backend_name[16];

    int display_id;
    int display_width;
    int display_height;

    int capture_width;
    int capture_height;
    int capture_stride;

    uint16_t mono_threshold_r5;
    uint16_t mono_threshold_g6;
    uint16_t mono_threshold_b5;

    uint16_t *buffer;
    uint8_t *work;

    uint8_t request_buffer[MAX_REQUEST_BUFFER_SIZE];
    int request_buffer_ix;

    int send_snapshot;

    int dithering;
    int16_t *dithering_buffer;
};

int capture_initialize(uint32_t device, int width, int height, struct capture_info *info);
void capture_finalize();
int capture(struct capture_info *info);

#endif
