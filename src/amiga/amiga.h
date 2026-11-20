#ifndef FUJIBLAST_AMIGA_H
#define FUJIBLAST_AMIGA_H
#include <intuition/intuition.h>
extern struct Window *amiga_win;
void gfx_render(void);
void gfx_close(void);
void gfx_dump(const char *path);
void input_close(void);
void snd_close(void);
void snd_vblank(void);
void amiga_fail(const char *why);
#endif
