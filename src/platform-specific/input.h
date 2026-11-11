/* input.h -- the joystick and keyboard. input_poll() runs once a frame. */
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* inp_dir bits */
#define IN_UP    1
#define IN_DOWN  2
#define IN_LEFT  4
#define IN_RIGHT 8

#define KEY_NONE   0
#define KEY_RETURN 13
#define KEY_ESC    27
#define KEY_BACK   8

extern uint8_t inp_dir;     /* held */
extern uint8_t inp_btn;     /* held */
extern uint8_t ev_btn;      /* pressed since the last input_take */
extern uint8_t ev_key;      /* a key (ASCII, upper case), KEY_NONE if none */

void input_init(void);
void input_poll(void);
void input_take(void);      /* the latches into ev_btn / ev_key */

/* Test hook: while inp_hook is set, inp_hook_dir/btn/key replace the hardware. */
extern uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

#endif
