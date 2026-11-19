#ifndef FB_WORKER_H
#define FB_WORKER_H
#include <stdint.h>
enum { FB_NET_IDLE, FB_NET_BAD_FRAME, FB_NET_FRAME_OVERFLOW, FB_NET_QUEUE_FULL };
void amiga_tcp_note(uint8_t reason);
int amiga_worker_init(void);
void amiga_worker_shutdown(void);
uint8_t amiga_tcp_open(const char *host, uint16_t port);
void amiga_tcp_close(void);
uint8_t amiga_tcp_send(const uint8_t *data, unsigned n);
unsigned amiga_tcp_read(uint8_t *data, unsigned n);
uint8_t amiga_tcp_lost(void);
uint8_t amiga_key_read(uint8_t key, uint16_t *n, uint8_t *data);
uint8_t amiga_key_write(uint8_t key, uint16_t n, uint8_t *data);
uint8_t amiga_history_read(uint8_t *data, uint16_t *size);
uint8_t amiga_history_write(uint8_t *data, uint16_t size);
#endif
