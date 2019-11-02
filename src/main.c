#include <syslog.h>
#include <err.h>
#include <poll.h>
#include <stdlib.h>

#include <stdio.h>
#include <fcntl.h>

#include <bcm_host.h>

#define MAX_REQUEST_BUFFER_SIZE 256

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
};

static void set_mono_threshold(struct capture_info *info, uint8_t threshold)
{
    // Convert the 8-bit threshold to the number of bits for rgb565 comparisons
    // and pre-shift.
    info->mono_threshold_r5 = threshold >> 3;
    info->mono_threshold_g6 = (threshold >> 2) << 5;
    info->mono_threshold_b5 = (threshold >> 3) << 11;
}

static int initialize(uint32_t device, int width, int height, struct capture_info *info)
{
    memset(info, 0, sizeof(*info));

    info->request_buffer_ix = 0;
    info->display_id = device;
    info->display_handle = vc_dispmanx_display_open(device);
    if (!info->display_handle) {
        syslog(LOG_ERR, "Unable to open primary display");
        return -1;
    }
    DISPMANX_MODEINFO_T display_info;
    int ret = vc_dispmanx_display_get_info(info->display_handle, &display_info);
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

    // This is an arbitrary value that looks relatively good for a program that wasn't
    // designed for monochrome.
    set_mono_threshold(info, 25);

    info->buffer = (uint16_t *) malloc(info->capture_stride * info->capture_height * sizeof(uint16_t));
    info->work = (uint8_t *) malloc(info->capture_width * info->capture_height * 4);

    uint32_t image_prt;
    info->capture_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, display_info.width, display_info.height, &image_prt);
    if (!info->capture_resource) {
        syslog(LOG_ERR, "Unable to create screen buffer");
        vc_dispmanx_display_close(info->display_handle);
        return -1;
    }
    return 0;
}

// NOTE: Resources *should* be cleaned up on process exit...
static void finalize(struct capture_info *info)
{
    free(info->buffer);
    free(info->work);
    vc_dispmanx_resource_delete(info->capture_resource);
    vc_dispmanx_display_close(info->display_handle);
}

static int capture(struct capture_info *info)
{
    vc_dispmanx_snapshot(info->display_handle, info->capture_resource, DISPMANX_NO_ROTATE);
    // Don't check the result since I don't know what it means.

    // Be careful on vc_dispmanx_resource_read_data(). See the source code
    // when in doubt, since it looks like someone tried to disguise a memcpy
    // as a rectangular copy.
    VC_RECT_T rect;
    vc_dispmanx_rect_set(&rect, 0, 0, info->capture_stride, info->capture_height);
    vc_dispmanx_resource_read_data(info->capture_resource, &rect, info->buffer, info->capture_stride * sizeof(uint16_t));
    return 0;
}

static void write_stdout(void *buffer, size_t len)
{
    if (write(STDOUT_FILENO, buffer, len) != (ssize_t) len)
        err(EXIT_FAILURE, "write");
}

static uint8_t *add_packet_length(uint8_t *out, uint32_t size)
{
    out[0] = (size >> 24);
    out[1] = (size >> 16) & 0xff;
    out[2] = (size >> 8) & 0xff;
    out[3] = (size & 0xff);
    return out + 4;
}

static int emit_rgb24(const struct capture_info *info)
{
    int width = info->capture_width;
    int height = info->capture_height;
    const uint16_t *image = info->buffer;

    uint8_t *out = add_packet_length(info->work, 3 * width * height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel = image[x];
            out[0] = (pixel >> 11) << 3;
            out[1] = ((pixel >> 5) & 0x3f) << 2;
            out[2] = (pixel & 0x1f) << 3;
            out += 3;
        }
        image += info->capture_stride;
    }
    write_stdout(info->work, out - info->work);
    return 0;
}

static int emit_rgb565(const struct capture_info *info)
{
    int width = info->capture_width;
    int height = info->capture_height;
    const uint16_t *image = info->buffer;

    uint8_t *out = add_packet_length(info->work, sizeof(uint16_t) * width * height);

    int width_bytes = width * sizeof(uint16_t);
    for (int y = 0; y < height; y++) {
        memcpy(out, image, width_bytes);
        out += width_bytes;
        image += info->capture_stride;
    }
    write_stdout(info->work, out - info->work);
    return 0;
}

static inline int to_1bpp(const struct capture_info *info, uint16_t rgb565)
{
    if ((rgb565 & 0x001f) > info->mono_threshold_r5 ||
            (rgb565 & 0x07e0) > info->mono_threshold_g6 ||
            (rgb565 & 0xf800) > info->mono_threshold_b5)
        return 1;
    else
        return 0;
}

static int emit_mono(const struct capture_info *info)
{
    int width = info->capture_width;
    int height = info->capture_height;
    const uint16_t *image = info->buffer;
    size_t row_skip = info->capture_stride - info->capture_width;

    uint8_t *out = add_packet_length(info->work, width * height / 8);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            *out = to_1bpp(info, image[0])
                   | (to_1bpp(info, image[1]) << 1)
                   | (to_1bpp(info, image[2]) << 2)
                   | (to_1bpp(info, image[3]) << 3)
                   | (to_1bpp(info, image[4]) << 4)
                   | (to_1bpp(info, image[5]) << 5)
                   | (to_1bpp(info, image[6]) << 6)
                   | (to_1bpp(info, image[7]) << 7);
            image += 8;
            out++;
        }
        image += row_skip;
    }
    write_stdout(info->work, out - info->work);
    return 0;
}

static int emit_mono_rotate_flip(const struct capture_info *info)
{
    int width = info->capture_width;
    int height = info->capture_height;
    int stride = info->capture_stride;
    const uint16_t *image = info->buffer;

    uint8_t *out = add_packet_length(info->work, width * height / 8);

    for (int x = 0; x < width; x++) {
        const uint16_t *column = image;
        for (int y = 0; y < height; y += 8) {
            *out = to_1bpp(info, column[0])
                   | (to_1bpp(info, column[stride]) << 1)
                   | (to_1bpp(info, column[2 * stride]) << 2)
                   | (to_1bpp(info, column[3 * stride]) << 3)
                   | (to_1bpp(info, column[4 * stride]) << 4)
                   | (to_1bpp(info, column[5 * stride]) << 5)
                   | (to_1bpp(info, column[6 * stride]) << 6)
                   | (to_1bpp(info, column[7 * stride]) << 7);
            column += 8 * stride;
            out++;
        }
        image++;
    }
    write_stdout(info->work, out - info->work);
    return 0;
}

static int emit_capture_info(const struct capture_info *info)
{
    uint8_t *out = add_packet_length(info->work, 20);
    memcpy(out, &info->display_id, sizeof(uint32_t));
    out += sizeof(uint32_t);
    memcpy(out, &info->display_width, sizeof(uint32_t));
    out += sizeof(uint32_t);
    memcpy(out, &info->display_height, sizeof(uint32_t));
    out += sizeof(uint32_t);
    memcpy(out, &info->capture_width, sizeof(uint32_t));
    out += sizeof(uint32_t);
    memcpy(out, &info->capture_height, sizeof(uint32_t));
    out += sizeof(uint32_t);
    write_stdout(info->work, out - info->work);
    return 0;
}

static void handle_stdin(struct capture_info *info)
{
    int amount_read = read(STDIN_FILENO, &info->request_buffer[info->request_buffer_ix], MAX_REQUEST_BUFFER_SIZE - info->request_buffer_ix - 1);
    if (amount_read < 0)
        err(EXIT_FAILURE, "Error reading stdin");
    if (amount_read == 0) {
        finalize(info);
        exit(EXIT_SUCCESS);
    }
    info->request_buffer_ix += amount_read;

    // Check if there's a command.
    while (info->request_buffer_ix >= 5) {
        // The request format is:
        //
        // 00 00 00 len cmd args
        //
        // Commands:
        // 02 -> capture rgb24
        // 03 -> capture rgb565
        // 04 -> capture 1bpp
        // 05 -> capture 1bbp, but scan down the columns
        // 06 <threshold> -> set the monochrome conversion threshold (no response)

        // NOTE: The request format is what it is since we're using Erlang's built-in 4-byte length
        //       framing for simplicity.
        if (info->request_buffer[0] != 0 ||
                info->request_buffer[1] != 0 ||
                info->request_buffer[2] != 0)
            err(EXIT_FAILURE, "Unexpected command: %02x %02x %02x %02x", info->request_buffer[0], info->request_buffer[1], info->request_buffer[2], info->request_buffer[3]);

        uint8_t len = 4 + info->request_buffer[3];
        if (info->request_buffer_ix < len)
            break;

        switch (info->request_buffer[4]) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            info->send_snapshot = info->request_buffer[4];
            break;

        case 6:
            set_mono_threshold(info, info->request_buffer[5]);
            break;

        default: // ignore
            break;
        }
        info->request_buffer_ix -= len;

        if (info->request_buffer_ix > 0)
            memmove(info->request_buffer, info->request_buffer + len, info->request_buffer_ix - len);
    }
}

static int send_snapshot(struct capture_info *info)
{
    switch (info->send_snapshot) {
    case 1:
    case 2:
        return emit_rgb24(info);
    case 3:
        return emit_rgb565(info);
    case 4:
        return emit_mono(info);
    case 5:
        return emit_mono_rotate_flip(info);
    default:
        return 0;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
        errx(EXIT_FAILURE, "rpi_fb_capture <display> <w> <h>\n");

    uint32_t display_device = strtoul(argv[1], NULL, 0);
    int width = strtol(argv[2], NULL, 0);
    int height = strtol(argv[3], NULL, 0);

    bcm_host_init();

    struct capture_info info;
    if (initialize(display_device, width, height, &info) < 0)
        errx(EXIT_FAILURE, "capture initialization failed");

    emit_capture_info(&info);

    for (;;) {
        struct pollfd fdset[1];

        fdset[0].fd = STDIN_FILENO;
        fdset[0].events = POLLIN;
        fdset[0].revents = 0;

        int rc = poll(fdset, 1, -1);
        if (rc < 0)
            err(EXIT_FAILURE, "poll");

        if (fdset[0].revents & (POLLIN | POLLHUP))
            handle_stdin(&info);

        if (info.send_snapshot) {
            capture(&info);

            send_snapshot(&info);
            info.send_snapshot = 0;
        }
    }
}
