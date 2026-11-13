"""png.py -- just enough PNG reading for the converters (8-bit RGB/RGBA or
palette, not interlaced), so the build needs no Pillow (defoogi has none)."""
import struct
import zlib


class Image:
    def __init__(self, path):
        data = open(path, "rb").read()
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"{path}: not a PNG")
        pos, idat, pal, trns = 8, b"", None, None
        while pos < len(data):
            n, typ = struct.unpack(">I4s", data[pos:pos + 8])
            body = data[pos + 8:pos + 8 + n]
            pos += 12 + n
            if typ == b"IHDR":
                self.width, self.height, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", body)
                if depth != 8 or inter or ctype not in (2, 3, 6):
                    raise ValueError(f"{path}: want 8-bit RGB, RGBA or palette, not interlaced")
            elif typ == b"PLTE":
                pal = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
            elif typ == b"tRNS":
                trns = body
            elif typ == b"IDAT":
                idat += body
        bpp = {2: 3, 3: 1, 6: 4}[ctype]
        raw = zlib.decompress(idat)
        stride = self.width * bpp
        rows, prev = [], bytearray(stride)
        for y in range(self.height):
            f = raw[y * (stride + 1)]
            line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                if f == 1:
                    line[i] = (line[i] + a) & 255
                elif f == 2:
                    line[i] = (line[i] + b) & 255
                elif f == 3:
                    line[i] = (line[i] + (a + b) // 2) & 255
                elif f == 4:
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                    line[i] = (line[i] + pr) & 255
            rows.append(line)
            prev = line
        self.px = []
        self.idx = [list(line[:self.width]) for line in rows] if ctype == 3 else None
        for line in rows:
            out = []
            for x in range(self.width):
                if ctype == 3:
                    k = line[x]
                    alpha = trns[k] if trns and k < len(trns) else 255
                    out.append(pal[k] + (alpha,))
                elif ctype == 2:
                    out.append(tuple(line[x * 3:x * 3 + 3]) + (255,))
                else:
                    out.append(tuple(line[x * 4:x * 4 + 4]))
            self.px.append(out)

    def getpixel(self, xy):
        return self.px[xy[1]][xy[0]]
