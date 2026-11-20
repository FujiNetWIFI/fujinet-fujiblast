/* Bounded complete-frame FIFO; caller serializes producer/consumer. */
#ifndef FB_QUEUE_H
#define FB_QUEUE_H
#include <stdint.h>
#include <string.h>
#define FB_TX_SLOTS 32
#define FB_TX_FRAME 64
struct fb_frame {
    uint8_t size, move;
    uint8_t data[FB_TX_FRAME];
};
struct fb_queue {
    unsigned head, count;
    struct fb_frame frame[FB_TX_SLOTS];
};
static inline int fb_queue_put(struct fb_queue *q, const uint8_t *data, unsigned n, int move)
{
    unsigned i;
    if (!n || n > FB_TX_FRAME)
        return 0;
    /* Only replace a consecutive, unsent move. Never cross an action/credit. */
    i = (q->head + q->count - 1) % FB_TX_SLOTS;
    if (!(q->count && move && q->frame[i].move)) {
        if (q->count == FB_TX_SLOTS)
            return 0;
        i = (q->head + q->count++) % FB_TX_SLOTS;
    }
    memcpy(q->frame[i].data, data, n);
    q->frame[i].size = n;
    q->frame[i].move = move;
    return 1;
}
static inline unsigned fb_queue_take(struct fb_queue *q, uint8_t *data, unsigned cap)
{
    unsigned n = 0;
    while (q->count && n + q->frame[q->head].size <= cap) {
        unsigned size = q->frame[q->head].size;
        memcpy(data + n, q->frame[q->head].data, size);
        n += size;
        q->head = (q->head + 1) % FB_TX_SLOTS;
        --q->count;
    }
    return n;
}
#endif
