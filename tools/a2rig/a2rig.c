/*
 * a2rig.c -- the Apple II build live: AppleWin's core (from
 * fujinet-go-apple2-desktop's libapple2session) with an in-process FujiNet
 * (its libfujinet.so) on SmartPort over SLIP, driven by a script. The
 * counterpart of tools/atarisim/asim's -netsio mode; tools/a2rig/rig.sh
 * runs it beside a private server and bots.
 *
 *   a2rig [-fast] -fn LIBFUJINET FNROOT SCRIPT OUTDIR
 *
 * FNROOT holds fnconfig.ini, data/ and SD/. The machine is an enhanced //e,
 * a Mockingboard in slot 4, FujiNet in slot 7; it cold-boots once FujiNet
 * has joined, so the boot scan finds it. Frames run at 60 a second (the
 * server keeps real time); -fast runs them flat out.
 *
 * Script, one command a line (# comments; numbers decimal or 0x hex;
 * ADDR a main-RAM address, rig.sh writes @symbol as one):
 *   wait N            N frames
 *   until ADDR V [N]  until the byte at ADDR is V (at most N frames, 3600)
 *   untilne ADDR V [N]
 *   peek ADDR [N]     print N bytes (as the CPU sees them: main RAM)
 *   poke ADDR V
 *   key K             a key, down then up (a character, or a number:
 *                     13 RETURN, 27 ESC, 8 left, 21 right, 11 up, 10 down)
 *   keydown K / keyup K
 *   joy X Y           the stick, -32768..32767 each
 *   btn N 0|1         button 0 (Open Apple) or 1 (Solid Apple)
 *   snap NAME         the screen to OUTDIR/NAME.png (560x384)
 *   film N NAME       N frames, every 4th to OUTDIR/NAME-NNN.png
 *   prof N FILE       N frames, the PC at each frame's end to OUTDIR/FILE
 *                     (tools/a2rig/prof.py maps them to functions)
 *   xprof N FILE      N frames single-stepped: every instruction's PC, counted
 *                     ("PC COUNT" lines; prof.py takes them too; FILE.cyc has
 *                     the cycles a PC took instead). SmartPort
 *                     calls take no emulated time here, as in a2rig always.
 *   xtrace N FILE     N frames single-stepped, every PC to OUTDIR/FILE as
 *                     "PC: " lines (tools/a2sim/gaps.py reads them)
 *   wav N FILE        N frames, the sound (44.1 kHz stereo) to OUTDIR/FILE
 *   print TEXT
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "apple2_host.h"

#define SLIP_PORT 1985

static uint32_t fb[APPLE2_HOST_MAX_WIDTH * APPLE2_HOST_MAX_HEIGHT];
static int fb_w, fb_h;
static long frames;
static int fast;
static const char *outdir;
static FILE *res;                       /* OUTDIR/a2rig.out: our lines alone */

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    if (res) {
        va_start(ap, fmt);
        vfprintf(res, fmt, ap);
        va_end(ap);
        fflush(res);
    }
}

static void on_frame(const uint32_t *px, int w, int h, void *user)
{
    (void)user;
    fb_w = w;
    fb_h = h;
    memcpy(fb, px, (size_t)w * h * 4);
}

static void on_log(int level, const char *msg, void *user)
{
    (void)user;
    if (level >= 2)
        fprintf(stderr, "core: %s\n", msg);
}

/* ---- PNG ----------------------------------------------------------------- */

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t h[8];
    uint32_t crc = crc32(0, (const uint8_t *)type, 4);
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(data, 1, n, f);
    crc = crc32(crc, data, n);
    be32(h, crc);
    fwrite(h, 1, 4, f);
}

static void snap(const char *name)
{
    char path[512];
    uint8_t ihdr[13];
    size_t raw_n = (size_t)fb_h * (fb_w * 3 + 1), i;
    uint8_t *raw = malloc(raw_n), *z;
    uLongf zn = compressBound(raw_n);
    FILE *f;
    int x, y;
    for (y = 0, i = 0; y < fb_h; ++y) {
        raw[i++] = 0;
        for (x = 0; x < fb_w; ++x) {
            uint32_t p = fb[y * fb_w + x];
            raw[i++] = p >> 16;
            raw[i++] = p >> 8;
            raw[i++] = p;
        }
    }
    z = malloc(zn);
    compress2(z, &zn, raw, raw_n, 6);
    snprintf(path, sizeof(path), "%s/%s.png", outdir, name);
    f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, fb_w);
    be32(ihdr + 4, fb_h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zn);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    say("snap %s (frame %ld)\n", path, frames);
}

/* ---- frames, in real time ------------------------------------------------- */

static struct timespec next;

static void frame(void)
{
    apple2host_core_run_frame();
    ++frames;
    if (fast)
        return;
    next.tv_nsec += 16683333;           /* 59.94 a second */
    while (next.tv_nsec >= 1000000000) {
        next.tv_nsec -= 1000000000;
        next.tv_sec++;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
}

/* Memory as the CPU sees it (the built core has no bank seam yet). */
static uint8_t peek(unsigned a)
{
    uint8_t b = 0;
    apple2host_regs r;
    int n = 0;
    /* (the game's code in main RAM is running: then RAMRD is off, or its
     * own next instruction would come from aux; the blitter runs in the
     * language card with aux switched in) */
    for (;;) {
        apple2host_get_regs(&r);
        if ((r.pc >= 0x0800 && r.pc < 0xC000) || ++n > 200000)
            break;
        apple2host_step_instruction();
    }
    apple2host_read_mem((uint16_t)a, &b, 1);
    return b;
}

/* ---- keys ----------------------------------------------------------------- */

static void keyev(int down, long k)
{
    unsigned code;
    uint32_t ch = 0;
    switch (k) {
    case 8:  code = 276; break;         /* RETROK_LEFT */
    case 21: code = 275; break;         /* RIGHT */
    case 11: code = 273; break;         /* UP */
    case 10: code = 274; break;         /* DOWN */
    case 13: code = 13; ch = 13; break;
    case 27: code = 27; ch = 27; break;
    default:
        code = (k >= 'A' && k <= 'Z') ? (unsigned)(k + 32) : (unsigned)k;
        ch = (uint32_t)k;
    }
    apple2host_inject_key(down, code, down ? ch : 0, 0);
}

static long num(const char *s)
{
    if (!s)
        return 0;
    if (s[0] && !s[1] && !(s[0] >= '0' && s[0] <= '9'))
        return (unsigned char)s[0];     /* a single character */
    return strtol(s, NULL, 0);
}

/* ---- FujiNet -------------------------------------------------------------- */

typedef int (*start_fn)(const char *, const char *, const char *, const char *, int);
static start_fn fn_start;
static char fn_root[512], fn_cfg[600], fn_sd[600], fn_data[600];
static volatile int fn_up;

static void *fn_thread(void *arg)
{
    (void)arg;
    fn_up = fn_start(fn_root, fn_cfg, fn_sd, fn_data, SLIP_PORT) ? 1 : -1;
    return NULL;
}

int main(int argc, char **argv)
{
    const char *lib = NULL;
    FILE *sc;
    char line[512];
    void *h;
    pthread_t th;
    int i;

    for (i = 1; i < argc && argv[i][0] == '-'; ++i) {
        if (!strcmp(argv[i], "-fast"))
            fast = 1;
        else if (!strcmp(argv[i], "-fn") && i + 1 < argc)
            lib = argv[++i];
    }
    if (argc - i != 3 || !lib) {
        fprintf(stderr, "usage: a2rig [-fast] -fn LIBFUJINET FNROOT SCRIPT OUTDIR\n");
        return 2;
    }
    snprintf(fn_root, sizeof(fn_root), "%s", argv[i]);
    snprintf(fn_cfg, sizeof(fn_cfg), "%s/fnconfig.ini", fn_root);
    snprintf(fn_sd, sizeof(fn_sd), "%s/SD", fn_root);
    snprintf(fn_data, sizeof(fn_data), "%s/data", fn_root);
    sc = fopen(argv[i + 1], "r");
    if (!sc) {
        perror(argv[i + 1]);
        return 1;
    }
    outdir = argv[i + 2];
    setvbuf(stdout, NULL, _IOLBF, 0);
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/a2rig.out", outdir);
        res = fopen(path, "w");
    }

    apple2host_set_log_sink(on_log, NULL);
    apple2host_set_frame_sink(on_frame, NULL);
    apple2host_set_variable("applewin_machine", "Enhanced Apple //e");
    apple2host_set_variable("applewin_slot4", "Mockingboard");
    apple2host_set_variable("applewin_slot6", "Disk II");
    apple2host_set_variable("applewin_slot7", "FujiNet");
    apple2host_set_slip_listener("127.0.0.1", SLIP_PORT);
    if (!apple2host_core_start()) {
        fprintf(stderr, "a2rig: the core did not start\n");
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &next);

    h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!h || !(fn_start = (start_fn)dlsym(h, "fujinet_desktop_start_runtime"))) {
        fprintf(stderr, "a2rig: %s: %s\n", lib, dlerror());
        return 1;
    }
    pthread_create(&th, NULL, fn_thread, NULL);
    for (i = 0; i < 60 * 30 && !(fn_up && apple2host_fujinet_devices() > 0); ++i)
        frame();
    if (apple2host_fujinet_devices() <= 0) {
        fprintf(stderr, "a2rig: FujiNet never joined (start %d)\n", fn_up);
        return 1;
    }
    say("fujinet up after %ld frames, %d devices; cold boot\n", frames, apple2host_fujinet_devices());
    apple2host_power_cycle();

    while (fgets(line, sizeof(line), sc)) {
        char *w[6] = { 0 }, *p = strchr(line, '#');
        int n = 0;
        if (p)
            *p = 0;
        for (p = strtok(line, " \t\r\n"); p && n < 6; p = strtok(NULL, " \t\r\n"))
            w[n++] = p;
        if (!n)
            continue;
        if (!strcmp(w[0], "wait")) {
            long k = num(w[1]);
            while (k-- > 0)
                frame();
        } else if (!strcmp(w[0], "until") || !strcmp(w[0], "untilne")) {
            unsigned a = (unsigned)num(w[1]);
            uint8_t v = (uint8_t)num(w[2]);
            long k = w[3] ? num(w[3]) : 3600;
            int ne = w[0][5] == 'n';
            while (k-- > 0 && ((peek(a) == v) == ne))
                frame();
            say("%s %04X %02X: %s (frame %ld)\n", w[0], a, v, k >= 0 ? "ok" : "TIMEOUT", frames);
        } else if (!strcmp(w[0], "peek")) {
            unsigned a = (unsigned)num(w[1]);
            long k = w[2] ? num(w[2]) : 1;
            {
                char line[400];
                int o = snprintf(line, sizeof(line), "%s %04X:", w[0], a);
                while (k-- > 0 && o < 380)
                    o += snprintf(line + o, sizeof(line) - o, " %02X", peek(a++));
                say("%s  (frame %ld)\n", line, frames);
            }
        } else if (!strcmp(w[0], "poke")) {
            uint8_t v = (uint8_t)num(w[2]);
            apple2host_write_mem((uint16_t)num(w[1]), &v, 1);
        } else if (!strcmp(w[0], "key")) {
            keyev(1, num(w[1]));
            frame();
            frame();
            keyev(0, num(w[1]));
            frame();
        } else if (!strcmp(w[0], "keydown")) {
            keyev(1, num(w[1]));
        } else if (!strcmp(w[0], "keyup")) {
            keyev(0, num(w[1]));
        } else if (!strcmp(w[0], "joy")) {
            apple2host_set_joystick_axis(0, 0, (int16_t)num(w[1]));
            apple2host_set_joystick_axis(0, 1, (int16_t)num(w[2]));
        } else if (!strcmp(w[0], "btn")) {
            apple2host_set_joystick_button(0, num(w[1]) ? 0 : 8, (int)num(w[2]));
        } else if (!strcmp(w[0], "snap")) {
            snap(w[1]);
        } else if (!strcmp(w[0], "film")) {
            long k = num(w[1]), j;
            char nm[256];
            for (j = 0; j < k; ++j) {
                frame();
                if (j % 4 == 0) {
                    snprintf(nm, sizeof(nm), "%s-%03ld", w[2], j / 4);
                    snap(nm);
                }
            }
        } else if (!strcmp(w[0], "prof")) {
            long k = num(w[1]);
            char path[512];
            FILE *pf;
            apple2host_regs r;
            snprintf(path, sizeof(path), "%s/%s", outdir, w[2]);
            pf = fopen(path, "w");
            while (k-- > 0) {
                frame();
                apple2host_get_regs(&r);
                fprintf(pf, "%04X\n", r.pc);
            }
            fclose(pf);
            say("prof %s (frame %ld)\n", path, frames);
        } else if (!strcmp(w[0], "xprof")) {
            static uint32_t hist[65536], chist[65536];
            long k = num(w[1]);
            char path[512];
            FILE *pf;
            apple2host_regs r;
            unsigned long cyc = 0, goal = (unsigned long)k * 17030, c;
            unsigned a;
            memset(hist, 0, sizeof(hist));
            memset(chist, 0, sizeof(chist));
            while (cyc < goal) {
                apple2host_get_regs(&r);
                hist[r.pc]++;
                c = apple2host_step_instruction();
                chist[r.pc] += c;
                cyc += c;
            }
            frames += k;
            snprintf(path, sizeof(path), "%s/%s", outdir, w[2]);
            pf = fopen(path, "w");
            for (a = 0; a < 65536; ++a)
                if (hist[a])
                    fprintf(pf, "%04X %u\n", a, hist[a]);
            fclose(pf);
            strcat(path, ".cyc");           /* the same, in cycles */
            pf = fopen(path, "w");
            for (a = 0; a < 65536; ++a)
                if (chist[a])
                    fprintf(pf, "%04X %u\n", a, chist[a]);
            fclose(pf);
            clock_gettime(CLOCK_MONOTONIC, &next);
            say("xprof %s (frame %ld)\n", path, frames);
        } else if (!strcmp(w[0], "xtrace")) {
            long k = num(w[1]);
            char path[512];
            FILE *pf;
            apple2host_regs r;
            unsigned long cyc = 0, goal = (unsigned long)k * 17030;
            snprintf(path, sizeof(path), "%s/%s", outdir, w[2]);
            pf = fopen(path, "w");
            while (cyc < goal) {
                apple2host_get_regs(&r);
                fprintf(pf, "%04X: \n", r.pc);
                cyc += apple2host_step_instruction();
            }
            fclose(pf);
            frames += k;
            clock_gettime(CLOCK_MONOTONIC, &next);
            say("xtrace %s (frame %ld)\n", path, frames);
        } else if (!strcmp(w[0], "wav")) {
            long k = num(w[1]);
            char path[512];
            FILE *pf;
            static int16_t buf[1470 * 2];
            uint32_t bytes = 0;
            uint8_t hdr[44] = "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xAC\0\0\x10\xB1\x02\0\x04\0\x10\0data";
            snprintf(path, sizeof(path), "%s/%s", outdir, w[2]);
            pf = fopen(path, "wb");
            fwrite(hdr, 1, 44, pf);
            apple2host_audio_set_active(1);
            while (k-- > 0) {
                int n;
                frame();
                n = apple2host_fill_audio(buf, 1470);      /* a frame's: 735 stereo */
                fwrite(buf, 2, n, pf);
                bytes += n * 2;
            }
            apple2host_audio_set_active(0);
            fseek(pf, 4, SEEK_SET);
            { uint32_t v = bytes + 36; fwrite(&v, 4, 1, pf); }
            fseek(pf, 40, SEEK_SET);
            fwrite(&bytes, 4, 1, pf);
            fclose(pf);
            say("wav %s (frame %ld)\n", path, frames);
        } else if (!strcmp(w[0], "print")) {
            say("%s\n", w[1] ? w[1] : "");
        } else {
            fprintf(stderr, "a2rig: unknown command %s\n", w[0]);
            return 1;
        }
    }
    say("done (frame %ld)\n", frames);
    fflush(stdout);
    _exit(0);                           /* (FujiNet's threads live in the library) */
}
