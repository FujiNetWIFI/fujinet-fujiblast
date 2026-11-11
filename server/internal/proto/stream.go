package proto

// Stream framing, from M.U.L.E. (fujinet-multiplayer-mule internal/proto):
// each frame is COBS-encoded and ends in a 0 byte, so a receiver that lost
// or garbled a byte finds the next frame at the next 0. A decoded frame is
//
//	type, payload..., sum1, sum2
//
// where sum1/sum2 are Fletcher-16 (mod 255) over type and payload.

// Fletcher16 is the frame checksum: sum1 = sum of bytes, sum2 = sum of
// sum1s, both mod 255.
func Fletcher16(b []byte) (uint8, uint8) {
	var s1, s2 uint32
	for _, c := range b {
		s1 = (s1 + uint32(c)) % 255
		s2 = (s2 + s1) % 255
	}
	return uint8(s1), uint8(s2)
}

// Frame builds one encoded frame: COBS of type, payload, sum; then 0.
func Frame(typ byte, payload []byte) []byte {
	raw := make([]byte, 0, len(payload)+3)
	raw = append(raw, typ)
	raw = append(raw, payload...)
	s1, s2 := Fletcher16(raw)
	raw = append(raw, s1, s2)
	return append(COBSEncode(raw), 0)
}

// COBSEncode encodes b (no trailing 0).
func COBSEncode(b []byte) []byte {
	out := make([]byte, 1, len(b)+len(b)/254+2)
	code, ci := byte(1), 0
	for _, c := range b {
		if c != 0 {
			out = append(out, c)
			code++
		}
		if c == 0 || code == 0xFF {
			out[ci] = code
			code, ci = 1, len(out)
			out = append(out, 0)
		}
	}
	out[ci] = code
	return out
}

// COBSDecode decodes one frame (without its trailing 0). ok is false if it
// is malformed.
func COBSDecode(b []byte) (out []byte, ok bool) {
	for i := 0; i < len(b); {
		code := int(b[i])
		if code == 0 || i+code > len(b) {
			return nil, false
		}
		out = append(out, b[i+1:i+code]...)
		i += code
		if code < 0xFF && i < len(b) {
			out = append(out, 0)
		}
	}
	return out, true
}

// Unframe checks and strips a decoded frame's sum: type and payload.
func Unframe(raw []byte) (typ byte, payload []byte, ok bool) {
	if len(raw) < 3 {
		return 0, nil, false
	}
	n := len(raw) - 2
	s1, s2 := Fletcher16(raw[:n])
	if s1 != raw[n] || s2 != raw[n+1] {
		return 0, nil, false
	}
	return raw[0], raw[1:n], true
}

// Patches returns the payloads of the CS_PATCH frames that turn prev into
// cur (nil prev: all of cur). Each payload is runs of off, len, bytes...;
// runs closer than 3 bytes are joined (a run costs 2 bytes of header), and
// a payload holds at most MaxPatch bytes.
func Patches(prev, cur []byte) (frames [][]byte) {
	n := len(cur)
	var p []byte
	for i := 0; i < n; {
		if prev != nil && prev[i] == cur[i] {
			i++
			continue
		}
		// One run: from i to the last changed byte before a gap of 3 or more.
		j, last := i, i
		for j < n && j-i < MaxPatch-2 {
			if prev == nil || prev[j] != cur[j] {
				last = j
			} else if j-last >= 3 {
				break
			}
			j++
		}
		run := cur[i : last+1]
		if len(p)+2+len(run) > MaxPatch {
			frames = append(frames, p)
			p = nil
		}
		p = append(p, byte(i), byte(len(run)))
		p = append(p, run...)
		i = last + 1
	}
	if len(p) > 0 {
		frames = append(frames, p)
	}
	return frames
}

// ApplyPatch writes a CS_PATCH payload into st. False if it is malformed.
func ApplyPatch(st, p []byte) bool {
	for len(p) > 0 {
		if len(p) < 2 {
			return false
		}
		off, n := int(p[0]), int(p[1])
		if len(p) < 2+n || off+n > len(st) {
			return false
		}
		copy(st[off:], p[2:2+n])
		p = p[2+n:]
	}
	return true
}
