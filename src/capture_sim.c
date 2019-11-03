#include "capture.h"

#include <math.h>

#include <stdio.h>
#include <string.h>

// Simulated display properties
#define DISPLAY_WIDTH 1280
#define DISPLAY_HEIGHT 720

#define MANDELBROT_MAX_ITERATIONS 200

static uint16_t iterations_to_rgb565(int iterations)
{
    // See the Javascript example at https://rosettacode.org/wiki/Mandelbrot_set
    // for this coloring.

    if (iterations >= MANDELBROT_MAX_ITERATIONS)
        return 0;

    double c = 3. * log(iterations) / log(MANDELBROT_MAX_ITERATIONS - 1);

    uint8_t r, g, b;

    if (c < 1) {
        r = (uint8_t) round(255 * c);
        g = 0;
        b = 0;
    }
    else if ( c < 2 ) {
        r = 255;
        g = (uint8_t) round(255 * (c - 1));
        b = 0;
    } else {
        r = 255;
        g = 255;
        b = (uint8_t) round(255 * (c - 2));
    }

    return (uint16_t) (((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | (b >> 3));


}

static int calc_mandelbrot(double cx, double cy)
{
    double x = 0.0;
    double y = 0.0;
    double xx = 0;
    double yy = 0;

    int i;
    for (i = 0; i < MANDELBROT_MAX_ITERATIONS && xx + yy <= 4; i++) {
        double xy = x * y;
        xx = x * x;
        yy = y * y;
        x = xx - yy + cx;
        y = xy + xy + cy;
    }
    return   i;
}

static void mandelbrot565(int width, int height, int stride, uint16_t *output)
{
    double scale = (height > width) ? 2. / width : 2. / height;
    for (int i = 0; i < height; i++) {
        double y = (i - 0.5*height) * scale;
        for (int j = 0; j  < width; j++) {
            double x = (j - 0.5*width) * scale - 0.6;

            int iterations = calc_mandelbrot(x, y);
            output[j] = iterations_to_rgb565(iterations);
        }
        output += stride;
    }
}

int capture_initialize(uint32_t device, int width, int height, struct capture_info *info)
{
    strcpy(info->backend_name, "sim");

    info->request_buffer_ix = 0;
    info->display_id = device;

    info->display_width = DISPLAY_WIDTH;
    info->display_height = DISPLAY_HEIGHT;

    // If capture width or height are out of bounds, set them to reasonable sizes.
    // This lets users capture the entire display without knowing how big it is.
    info->capture_width =
        (width <= 0 || width > info->display_width) ? info->display_width : width;
    info->capture_height =
        (height <= 0 || height > info->display_height) ? info->display_height : height;

    info->capture_stride = info->display_width;

    return 0;
}

void capture_finalize()
{
}

int capture(struct capture_info *info)
{
    mandelbrot565(info->capture_width, info->capture_height, info->capture_stride, info->buffer);
    return 0;
}
