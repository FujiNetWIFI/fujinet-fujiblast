"""hgr.py -- Apple II high-res colour, for tools/drawart_hgr.py and
tools/mkgfx_hgr.py: colour pixels into screen bytes, sprites as data and
keep bytes, and a rough NTSC picture of a screen.

HGR: 280 x 192, 40 bytes a line, 7 dots a byte (bit 0 leftmost), bit 7 the
byte's palette. A colour pixel is two dots, 140 across: violet or blue light
the first, green or orange the second, white both, black neither. Bit 7
clear gives violet and green, set blue and orange (and moves the byte's dots
half a dot right). So within one byte only one pair of colours can show:
where art asks for both, the byte goes with the majority (as M.U.L.E.'s
tools/mkgfx_hgr.py does), and the rest change colour -- the fringes.
"""

BLACK, WHITE, VIOLET, GREEN, BLUE, ORANGE = range(6)
NAMES = ["black", "white", "violet", "green", "blue", "orange"]
RGB = [(0, 0, 0), (255, 255, 255), (255, 68, 253), (20, 245, 60), (20, 207, 253), (255, 106, 60)]
FROM_RGB = {c: i for i, c in enumerate(RGB)}
HIGH = {BLUE, ORANGE}           # the palette with bit 7 set
LOW = {VIOLET, GREEN}


def lit_dots(c, x):
    """The dots colour pixel x lights in colour c."""
    return {BLACK: (), WHITE: (2 * x, 2 * x + 1), VIOLET: (2 * x,), BLUE: (2 * x,),
            GREEN: (2 * x + 1,), ORANGE: (2 * x + 1,)}[c]


def encode(pixels, x0=0, under=None):
    """Colour pixels (None: not drawn) from colour pixel x0 on.

    Returns {byte: (data, keep)}: for each byte the pixels touch, the bits
    they light (bit 7: the palette, when they care), and the bits of the
    screen to keep (bit 7 kept when no coloured pixel lands in the byte).

    under: the colour taken to be beneath (the floor, for bombers): what
    shows of it in a byte the pixels only partly cover votes too, so a
    byte that is mostly floor keeps the floor's palette and the sprite's
    edge changes colour instead of the floor."""
    covered, on, votes = {}, {}, {}
    if under in HIGH or under in LOW:
        lo = min(i for i, c in enumerate(pixels) if c is not None) if any(c is not None for c in pixels) else 0
        hi = max(i for i, c in enumerate(pixels) if c is not None) if any(c is not None for c in pixels) else -1
        for x in range(x0 + lo - 4, x0 + hi + 5):
            i = x - x0
            if 0 <= i < len(pixels) and pixels[i] is not None:
                continue
            for d in lit_dots(under, x):
                v = votes.setdefault(d // 7, [0, 0])
                v[1 if under in HIGH else 0] += 1
    for i, c in enumerate(pixels):
        if c is None:
            continue
        x = x0 + i
        for d in (2 * x, 2 * x + 1):
            covered.setdefault(d // 7, 0)
            covered[d // 7] |= 1 << (d % 7)
        for d in lit_dots(c, x):
            on.setdefault(d // 7, 0)
            on[d // 7] |= 1 << (d % 7)
            if c in HIGH or c in LOW:
                v = votes.setdefault(d // 7, [0, 0])
                v[1 if c in HIGH else 0] += 1
    out = {}
    for b, cov in covered.items():
        data = on.get(b, 0)
        keep = 0x7F & ~cov
        v = votes.get(b)
        if v and under is not None:
            if v[1] > v[0]:
                data |= 0x80
        elif v:
            if v[1] > v[0]:
                data |= 0x80
        else:
            keep |= 0x80        # black and white only: the screen's palette stays
        out[b] = (data, keep)
    return out


def solid(pixels, x0=0):
    """Colour pixels with no gaps -> {byte: value} (the palette by majority;
    a byte with no colour in it gets bit 7 clear)."""
    return {b: d for b, (d, k) in encode(pixels, x0).items()}


class Screen:
    """40 x 192 bytes, drawn on as the game would."""

    def __init__(self, fill=0):
        self.b = [[fill] * 40 for _ in range(192)]

    def put(self, y, x0, pixels, under=None):
        """Colour pixels at colour pixel x0, line y, over what is there."""
        if not 0 <= y < 192:
            return
        for b, (d, k) in encode(pixels, x0, under).items():
            if 0 <= b < 40:
                self.b[y][b] = (self.b[y][b] & k) | d   # as the blitter does

    def render(self, path, scale=2):
        """A rough NTSC picture: 560 wide (half dots, so the palette's shift
        shows), each line twice."""
        from PIL import Image
        im = Image.new("RGB", (560, 192))
        px = im.load()
        for y in range(192):
            lit = [0] * 280
            hi = [0] * 280
            for b in range(40):
                v = self.b[y][b]
                for k in range(7):
                    lit[b * 7 + k] = (v >> k) & 1
                    hi[b * 7 + k] = v >> 7
            for d in range(280):
                if not lit[d]:
                    continue
                if (d > 0 and lit[d - 1]) or (d < 279 and lit[d + 1]):
                    c = WHITE
                else:
                    c = [[VIOLET, GREEN], [BLUE, ORANGE]][hi[d]][d & 1]
                h = 2 * d + hi[d]
                span = 2 if c == WHITE else 4       # a lone dot's colour fills its pixel
                for i in range(span):
                    if h + i < 560:
                        px[h + i, y] = RGB[c]
        im = im.resize((560, 384), Image.NEAREST)
        if scale != 2:
            im = im.resize((280 * scale, 192 * scale), Image.NEAREST)
        im.save(path)
