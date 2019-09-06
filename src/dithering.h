#ifndef DITHERING_H
#define DITHERING_H

#define DITHERING_NONE              0
#define DITHERING_FLOYD_STEINBERG   1
#define DITHERING_SIERRA            2
#define DITHERING_SIERRA_2ROW       3
#define DITHERING_SIERRA_LITE       4

void dithering_apply(const struct capture_info *info);

#endif
