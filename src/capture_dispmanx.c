#include "capture.h"

#include <bcm_host.h>
#include <syslog.h>

// Only one display is supported now.
static DISPMANX_DISPLAY_HANDLE_T display_handle;
static DISPMANX_RESOURCE_HANDLE_T capture_resource;

int capture_initialize(uint32_t device, int width, int height, struct capture_info *info)
{
    strcpy(info->backend_name, "dispmanx");

    bcm_host_init();

    info->request_buffer_ix = 0;
    info->display_id = device;
    display_handle = vc_dispmanx_display_open(device);
    if (!display_handle) {
        syslog(LOG_ERR, "Unable to open primary display");
        return -1;
    }
    DISPMANX_MODEINFO_T display_info;
    int ret = vc_dispmanx_display_get_info(display_handle, &display_info);
    if (ret) {
        syslog(LOG_ERR, "Unable to get primary display information");
        return -1;
    }

    info->display_width = display_info.width;
    info->display_height = display_info.height;

    // If capture width or height are out of bounds, set them to reasonable sizes.
    // This lets users capture the entire display without knowing how big it is.
    info->capture_width =
        (width <= 0 || width > info->display_width) ? info->display_width : width;
    info->capture_height =
        (height <= 0 || height > info->display_height) ? info->display_height : height;

    // vc_dispmanx_resource_read_data seems to be implemented as memcpy. That means
    // that copies are 1 dimensional rather than 2 dimensional so if we want to make
    // one call to vc_dispmanx_resource_read_data then our destination buffer needs
    // to be the same width as the display. Otherwise, we'd need to make a call for
    // each line.
    info->capture_stride = info->display_width;

    uint32_t image_prt;
    capture_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, display_info.width, display_info.height, &image_prt);
    if (!capture_resource) {
        syslog(LOG_ERR, "Unable to create screen buffer");
        vc_dispmanx_display_close(display_handle);
        return -1;
    }

}

void capture_finalize()
{
    vc_dispmanx_resource_delete(capture_resource);
    vc_dispmanx_display_close(display_handle);
}

int capture(struct capture_info *info)
{
    vc_dispmanx_snapshot(display_handle, capture_resource, DISPMANX_NO_ROTATE);
    // Don't check the result since I don't know what it means.

    // Be careful on vc_dispmanx_resource_read_data(). See the source code
    // when in doubt, since it looks like someone tried to disguise a memcpy
    // as a rectangular copy.
    VC_RECT_T rect;
    vc_dispmanx_rect_set(&rect, 0, 0, info->capture_stride, info->capture_height);
    vc_dispmanx_resource_read_data(capture_resource, &rect, info->buffer, info->capture_stride * sizeof(uint16_t));
    return 0;
}
