#include <assert.h>
#include <stdio.h>
#include "../../src/amiga/queue.h"
int main(void)
{
    struct fb_queue q = {0};
    uint8_t a[] = {1, 2}, b[] = {3, 4}, action[] = {5, 6}, out[128];
    unsigned i;
    assert(fb_queue_put(&q, a, 2, 1));
    assert(fb_queue_put(&q, b, 2, 1));
    assert(q.count == 1); /* stale position superseded */
    assert(fb_queue_put(&q, action, 2, 0));
    assert(fb_queue_put(&q, a, 2, 1));
    assert(fb_queue_take(&q, out, 5) == 4); /* never split a frame */
    assert(!memcmp(out, b, 2) && !memcmp(out + 2, action, 2));
    assert(fb_queue_take(&q, out, 2) == 2 && !memcmp(out, a, 2));
    /* Wrap around repeatedly; no action loss or action-crossing replacement. */
    for (i = 0; i < 1000; ++i) {
        assert(fb_queue_put(&q, action, 2, 0));
        assert(fb_queue_take(&q, out, 128) == 2);
        assert(!memcmp(out, action, 2));
    }
    for (i = 0; i < FB_TX_SLOTS; ++i)
        assert(fb_queue_put(&q, a, 2, 0));
    assert(!fb_queue_put(&q, action, 2, 0));
    assert(q.count == FB_TX_SLOTS);
    assert(fb_queue_take(&q, out, 128) == FB_TX_SLOTS * 2);
    assert(!fb_queue_put(&q, out, 0, 0));
    assert(!fb_queue_put(&q, out, FB_TX_FRAME + 1, 0));
    puts("bounded queue ordering passed");
}
