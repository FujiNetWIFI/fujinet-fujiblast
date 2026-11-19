#ifndef FB_HISTORY_STORE_H
#define FB_HISTORY_STORE_H
#include <stdint.h>
/* Called only by the network worker, which owns the library context. */
uint8_t fb_history_read(uint8_t *data, uint16_t *size);
uint8_t fb_history_write(const uint8_t *data, uint16_t size);
#endif
