#ifndef FB_AMIGA_STREAM_H
#define FB_AMIGA_STREAM_H
#include <stdint.h>
/* A call is always one FujiBus-sized request, keeping read retries replayable. */
#define FB_READ_SIZE 128
#define FB_IO_ATTEMPTS 3
uint8_t fb_stream_read(uint16_t handle, uint32_t offset, uint8_t *data,
                       uint16_t *got, uint8_t *flags);
uint8_t fb_stream_write(uint16_t handle, uint32_t offset, const uint8_t *data,
                        uint16_t size, uint16_t *written);
void fb_stream_pause(unsigned ticks);
void fb_stream_event(const char *event, uint8_t rc, unsigned wanted,
                     unsigned done, unsigned attempt);
#endif
