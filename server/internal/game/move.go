package game

import "fujiblast/internal/proto"

// Movement as every client does it (src/move.c is the same code): the
// server only checks the result, the bots use it to walk.
//
// A player is a tile-sized box at x, y (units). At least one of x, y is
// always a multiple of Unit: you walk along a lane. Turning into a cross
// lane from up to Lane units off it slides you onto it.

const unit = proto.Unit

func inArena(c, r int) bool { return c >= 0 && c < proto.ArenaW && r >= 0 && r < proto.ArenaH }

// Walkable reports whether a walker may step into tile c, r.
func Walkable(m *[proto.Tiles]byte, c, r int) bool {
	if !inArena(c, r) {
		return false
	}
	switch m[r*proto.ArenaW+c] {
	case proto.TPillar, proto.TBlock, proto.TBurn, proto.TBomb, proto.TDud:
		return false
	}
	return true
}

// Walker is one player's position and its sub-unit speed remainder.
type Walker struct {
	X, Y, Dir int
	acc       int
}

// Step moves w one jiffy with the stick at d (-1: centred) at speed level
// lv. It returns whether w moved, and the bomb or dud tile it walked into
// (-1 if none).
func (w *Walker) Step(m *[proto.Tiles]byte, d, lv int) (moved bool, bump int) {
	bump = -1
	if d < 0 {
		w.acc = 0
		return false, bump
	}
	w.Dir = d
	w.acc += proto.SpeedBase + lv*proto.SpeedStep
	n := w.acc >> 4
	w.acc &= 15
	for ; n > 0; n-- {
		ok, b := w.unitStep(m, d)
		if b >= 0 {
			bump = b
		}
		if !ok {
			w.acc = 0
			break
		}
		moved = true
	}
	return moved, bump
}

// unitStep moves one unit toward d.
func (w *Walker) unitStep(m *[proto.Tiles]byte, d int) (bool, int) {
	dx, dy := 0, 0
	switch d {
	case proto.DirUp:
		dy = -1
	case proto.DirDown:
		dy = 1
	case proto.DirLeft:
		dx = -1
	case proto.DirRight:
		dx = 1
	}
	// along, across: the coordinate we move in and the one we must be on a lane in
	along, across := &w.X, &w.Y
	da := dx
	if dy != 0 {
		along, across, da = &w.Y, &w.X, dy
	}
	tile := func(a, c int) (int, int) { // along/across tile to column, row
		if dy != 0 {
			return c, a
		}
		return a, c
	}
	if off := *across % unit; off != 0 {
		a := *along/unit + da
		c0 := *across / unit
		if c, r := tile(a, c0); off <= proto.Lane && Walkable(m, c, r) {
			*across--
			return true, -1
		}
		if c, r := tile(a, c0+1); off >= unit-proto.Lane && Walkable(m, c, r) {
			*across++
			return true, -1
		}
		return false, -1
	}
	if *along%unit == 0 {
		c, r := tile(*along/unit+da, *across/unit)
		if !Walkable(m, c, r) {
			if inArena(c, r) {
				if t := m[r*proto.ArenaW+c]; t == proto.TBomb || t == proto.TDud {
					return false, r*proto.ArenaW + c
				}
			}
			return false, -1
		}
	}
	*along += da
	return true, -1
}

// CentreTile is the tile under a box's centre.
func CentreTile(x, y int) int {
	c, r := (x+unit/2)/unit, (y+unit/2)/unit
	if c >= proto.ArenaW {
		c = proto.ArenaW - 1
	}
	if r >= proto.ArenaH {
		r = proto.ArenaH - 1
	}
	return r*proto.ArenaW + c
}

// Fits reports whether a box at x, y overlaps nothing that never moves out
// of the way: pillars, blocks, burning blocks. (Bombs are the client's to
// respect; it may stand on one it was on when it appeared.)
func Fits(m *[proto.Tiles]byte, x, y int) bool {
	if x < 0 || y < 0 || x > proto.MaxX || y > proto.MaxY {
		return false
	}
	for r := y / unit; r <= (y+unit-1)/unit; r++ {
		for c := x / unit; c <= (x+unit-1)/unit; c++ {
			switch m[r*proto.ArenaW+c] {
			case proto.TPillar, proto.TBlock, proto.TBurn:
				return false
			}
		}
	}
	return true
}
