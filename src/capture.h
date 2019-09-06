#ifndef CAPTURE_H
#define CAPTURE_H

#include <bcm_host.h>

#define MAX_REQUEST_BUFFER_SIZE     256

struct capture_info {
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

    DISPMANX_DISPLAY_HANDLE_T display_handle;
    DISPMANX_RESOURCE_HANDLE_T capture_resource;

    uint8_t request_buffer[MAX_REQUEST_BUFFER_SIZE];
    int request_buffer_ix;

    int send_snapshot;

    int dithering;
    int16_t *dithering_buffer;
};
#endif
