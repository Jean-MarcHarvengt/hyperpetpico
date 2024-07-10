#ifndef GIFDEC_H
#define GIFDEC_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gd_GIF {
    int fd;
    uint8_t *input;
    uint16_t width, height;
    uint8_t palette[256 * 3]; // 256 color for compatibility
    uint16_t fx, fy, fw, fh;
    uint8_t *frame;
} gd_GIF;

int decode_gif(gd_GIF * gif, unsigned char * input, unsigned char * output);


#ifdef __cplusplus
}
#endif

#endif /* GIFDEC_H */
