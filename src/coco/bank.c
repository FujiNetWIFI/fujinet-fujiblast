/*
 * bank.c -- a 64K CoCo 1/2 into all-RAM mode, and the high core read from
 * the disk (from M.U.L.E.'s bank.c, without its overlays).
 *
 * BASIC LOADMs fujiblst.bin (the low core, $2600-$7BFF) and runs it.
 * bank_init then:
 *   - copies the Disk ROM ($C000-$DFFF: DSKCON and the DriveWire routines
 *     fujinet-lib calls through $D93F/$D941) and the CPU vectors (which the
 *     SAM fetches from $BFF2-$BFFF) into the RAM underneath, and switches
 *     the SAM to all-RAM (map type 1). Color and Extended BASIC are gone
 *     from then on: their 16K holds the high core.
 *   - reads fujiblhi.bin (the high core, $8000-$BFEF, and the art at
 *     $E000) with DSKCON. A file is DECB: blocks of 00 len addr data, then
 *     FF 0000 exec.
 *
 * The CoCo 3 (COCO3) is already all-RAM, so nothing is copied, and $FFDE is
 * left alone (ROM mode would fetch from a cartridge FujiNet). Its high core
 * is FUJIBL3H.BIN.
 *
 * Everything bank_init calls lives in the low core: the high core isn't
 * there yet.
 */
#include <string.h>
#include "fbcoco.h"

/* DSKCON's parameters, in BASIC's direct page. */
#define DCOPC  (*(uint8_t *)0x00EA)
#define DCDRV  (*(uint8_t *)0x00EB)
#define DCTRK  (*(uint8_t *)0x00EC)
#define DCSEC  (*(uint8_t *)0x00ED)
#define DCBPT  (*(uint8_t **)0x00EE)
#define DCSTA  (*(uint8_t *)0x00F0)
#define DEFDRV (*(uint8_t *)0x095A)     /* the drive BASIC last used */

#ifdef COCO3
#define MMU7 (*(volatile uint8_t *)0xFFA7)
/* DECB's disk buffers: nothing reads the disk once the game runs */
#define sec ((uint8_t *)0x0600)
#define fat ((uint8_t *)0x0700)
#else
static uint8_t sec[256];
static uint8_t fat[68];
#endif
static uint8_t drive;

#ifndef COCO3
/* Copy [a, a + n) from ROM to the RAM under it, a byte at a time. */
static void rom_to_ram(uint16_t a, uint16_t n)
{
    asm {
        ldx     :a
        ldd     :n
        pshs    u
        tfr     d,u
@loop
        sta     $FFDE           ; ROM
        lda     ,x
        sta     $FFDF           ; RAM
        sta     ,x+
        leau    -1,u
        cmpu    #0
        bne     @loop
        sta     $FFDE
        puls    u
    }
}
#endif

static uint8_t read_sector(uint8_t trk, uint8_t s)
{
    DCOPC = 2;
    DCDRV = drive;
    DCTRK = trk;
    DCSEC = s;
    DCBPT = sec;
    asm {
        pshs    u,y
        jsr     [$C004]
        puls    u,y
    }
    return DCSTA;
}

/* ---- a DECB .bin, streamed from the disk ------------------------------ */
static uint8_t hdr[5], hn;
static uint16_t left;
static uint8_t *at;
static uint8_t done;

static void feed(const uint8_t *p, uint16_t n)
{
    uint16_t k;
    while (n && !done) {
        if (left) {
            k = left < n ? left : n;
            memcpy(at, p, k);
            at += k;
            left -= k;
            p += k;
            n -= k;
            continue;
        }
        hdr[hn++] = *p++;
        --n;
        if (hn == 5) {
            hn = 0;
            if (hdr[0] == 0xFF) {
                done = 1;
                break;
            }
            left = ((uint16_t)hdr[1] << 8) | hdr[2];
            at = (uint8_t *)(((uint16_t)hdr[3] << 8) | hdr[4]);
        }
    }
}

/* Load the DECB file NAME (8 chars, space padded) with extension BIN.
 * Returns 0 if all went well. */
static uint8_t load(const char *name)
{
    uint8_t s, i, g, n, trk, last;
    uint8_t *e;
    uint16_t lastbytes = 256;

    if (read_sector(17, 2))
        return 1;
    memcpy(fat, sec, 68);
    g = 0xFF;
    for (s = 3; s <= 11 && g == 0xFF; ++s) {
        if (read_sector(17, s))
            return 1;
        for (i = 0; i < 8; ++i) {
            e = sec + i * 32;
            if (e[0] == 0xFF)
                break;
            if (memcmp(e, name, 8) == 0 && memcmp(e + 8, "BIN", 3) == 0) {
                g = e[13];
                lastbytes = ((uint16_t)e[14] << 8) | e[15];
                if (lastbytes == 0 || lastbytes > 256)
                    lastbytes = 256;
                break;
            }
        }
        if (i < 8 && e[0] == 0xFF)
            break;
    }
    if (g == 0xFF)
        return 2;
    hn = 0;
    left = 0;
    done = 0;
    while (!done) {
        if (g >= 68)
            return 3;
        n = fat[g];
        last = n >= 0xC0;
        n = last ? (n & 0x3F) : 9;
        trk = g >> 1;
        if (trk >= 17)
            ++trk;
        for (s = 0; s < n && !done; ++s) {
            if (read_sector(trk, (g & 1) * 9 + 1 + s))
                return 4;
            feed(sec, last && s == n - 1 ? lastbytes : 256);
        }
        if (last)
            break;
        g = fat[g];
    }
    return done ? 0 : 5;
}

/* A disk read can fail now and then (it is retried): 0 if the file came. */
static uint8_t load_try(const char *name)
{
    uint8_t tries, e = 1;
    for (tries = 0; tries < 8 && e; ++tries)
        e = load(name);
    return e;
}

/* ---- messages before the graphics are up: the text screen ------------- */
void gfx_boot_text(const char *s)
{
    uint8_t *p = (uint8_t *)0x0400 + 32 * 7;
    uint8_t c;
    memset((uint8_t *)0x0400, 0x60, 512);
    while ((c = (uint8_t)*s++) != 0)
        *p++ = c < 0x40 ? c + 0x40 : c;     /* VDG: normal video */
}

const char *bank_init(void)
{

    /* the stack's unused part ($7C00 up to here) gets a mark, so a dump
     * shows how deep it has been (tools/cocosim: dump 7C00 7FFF) */
    asm {
        leax    -16,s
@fill
        lda     #$A5
        sta     ,-x
        cmpx    #$7C00
        bhi     @fill
    }
    gfx_boot_text("FUJIBLAST LOADING...");
    drive = DEFDRV;
    plat_irq_init();                /* our IRQ: BASIC's is about to go */
#ifdef COCO3
    if (load_try("FUJIBL3H"))
        return "CAN'T LOAD FUJIBL3H.BIN";
    MMU7 = 6;                       /* the art: blocks 6 and 7, at $E000 */
    if (load_try("FUJIBL3A"))
        return "CAN'T LOAD FUJIBL3A.BIN";
    MMU7 = 7;
    if (load_try("FUJIBL3B"))
        return "CAN'T LOAD FUJIBL3B.BIN";
    MMU7 = 6;
#else
    asm { orcc #$50 }
    rom_to_ram(0xC000, 0x2000);
    rom_to_ram(0xBFF0, 0x10);
    asm {
        sta     $FFDF               ; all RAM from here on
        andcc   #$EF
    }
    {   /* a 32K machine has nothing at $8000 */
        volatile uint8_t *t = (volatile uint8_t *)0x9000;
        *t = 0xA5;
        if (*t != 0xA5) {
            asm { sta $FFDE }
            return "FUJIBLAST NEEDS 64K";
        }
    }
    if (load_try("FUJIBLHI"))
        return "CAN'T LOAD FUJIBLHI.BIN";
#endif
    return 0;
}
