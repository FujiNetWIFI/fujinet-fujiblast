package game

import (
	"testing"

	"fujiblast/internal/proto"
)

func tl(c, r int) int { return r*aw + c }

// started is a game of two (seats 0 and 1) in play on an arena of pillars
// only, no dice.
func started(t *testing.T) *Game {
	t.Helper()
	g := New(1)
	g.Join("ANN")
	g.Join("BOB")
	g.Act(0, proto.ActStart, 0, 0)
	if g.Phase != proto.PhCountdown {
		t.Fatalf("phase %d after start", g.Phase)
	}
	g.Advance(CountdownJ)
	if g.Phase != proto.PhPlay {
		t.Fatalf("phase %d after countdown", g.Phase)
	}
	g.dudOdds = 1 << 30
	for i := range g.Map {
		if g.Map[i] != proto.TPillar {
			g.Map[i] = proto.TFloor
		}
		g.hidden[i] = 0
	}
	return g
}

func place(g *Game, s, c, r int) {
	g.P[s].X, g.P[s].Y = c*unit, r*unit
}

func TestArena(t *testing.T) {
	g := New(7)
	g.Join("A")
	g.Join("B")
	g.Act(0, proto.ActStart, 0, 0)
	blocks := 0
	for r := 0; r < ah; r++ {
		for c := 0; c < aw; c++ {
			m := g.Map[tl(c, r)]
			if (c%2 == 1 && r%2 == 1) != (m == proto.TPillar) {
				t.Fatalf("tile %d,%d = %d", c, r, m)
			}
			if m == proto.TBlock {
				blocks++
			}
		}
	}
	for _, k := range [][2]int{{0, 0}, {1, 0}, {0, 1}, {12, 10}, {11, 10}, {12, 9}, {12, 0}, {0, 10}} {
		if g.Map[tl(k[0], k[1])] != proto.TFloor {
			t.Errorf("corner tile %v not clear", k)
		}
	}
	if blocks < 40 || blocks > 110 {
		t.Errorf("%d blocks", blocks)
	}
	h := New(7)
	h.Join("A")
	h.Join("B")
	h.Act(0, proto.ActStart, 0, 0)
	if h.Map != g.Map {
		t.Error("same seed, different arena")
	}
	if g.P[0].X != 0 || g.P[0].Y != 0 || g.P[1].X != proto.MaxX || g.P[1].Y != proto.MaxY {
		t.Errorf("spawns %d,%d %d,%d", g.P[0].X, g.P[0].Y, g.P[1].X, g.P[1].Y)
	}
}

func TestStartNeedsTwoAndHost(t *testing.T) {
	g := New(1)
	g.Join("A")
	g.Act(0, proto.ActStart, 0, 0)
	if g.Phase != proto.PhWait {
		t.Fatal("started alone")
	}
	g.Join("B")
	g.Act(1, proto.ActRounds, 5, 0)
	g.Act(1, proto.ActStart, 0, 0)
	if g.Phase != proto.PhWait || g.Rounds != DefRounds {
		t.Fatal("non-host started or set rounds")
	}
	g.Act(0, proto.ActRounds, 5, 0)
	g.Act(0, proto.ActStart, 0, 0)
	if g.Phase != proto.PhCountdown || g.Rounds != 5 {
		t.Fatalf("phase %d rounds %d", g.Phase, g.Rounds)
	}
	if g.Join("C") != -1 {
		t.Error("joined a running game")
	}
	if g.Join("a") != 0 {
		t.Error("rejoin by name (any case) lost the seat")
	}
}

func TestWalker(t *testing.T) {
	var m [tiles]byte
	for r := 1; r < ah; r += 2 {
		for c := 1; c < aw; c += 2 {
			m[tl(c, r)] = proto.TPillar
		}
	}
	w := Walker{}
	for i := 0; i < 40; i++ {
		w.Step(&m, proto.DirRight, 0)
	}
	if w.X != 40 || w.Y != 0 {
		t.Fatalf("walked to %d,%d", w.X, w.Y)
	}
	// Down from x=40 (8 past column 2): not within Lane, column 3 has pillars
	// at odd rows, column 2 is a lane... 40 is col 2.5: off 8 > Lane: stuck.
	w.Step(&m, proto.DirDown, 0)
	if w.Y != 0 {
		t.Fatalf("turned from mid-tile: %d,%d", w.X, w.Y)
	}
	// 4 units past column 2: slides back onto it, then goes down.
	w = Walker{X: 36}
	for i := 0; i < 10; i++ {
		w.Step(&m, proto.DirDown, 0)
	}
	if w.X != 32 || w.Y != 6 {
		t.Fatalf("corner slide: %d,%d", w.X, w.Y)
	}
	// A pillar stops you; a bomb stops you and reports the bump.
	w = Walker{X: 16}
	w.Step(&m, proto.DirDown, 0)
	if w.Y != 0 {
		t.Fatal("walked into a pillar")
	}
	m[tl(3, 0)] = proto.TBomb
	w = Walker{X: 32}
	_, bump := w.Step(&m, proto.DirRight, 0)
	if w.X != 32 || bump != tl(3, 0) {
		t.Fatalf("bomb: x %d bump %d", w.X, bump)
	}
	// Speed level 4 is twice as fast.
	w = Walker{}
	for i := 0; i < 8; i++ {
		w.Step(&m, proto.DirRight, proto.MaxSpeed)
	}
	if w.X != 16 {
		t.Fatalf("speed 4: %d", w.X)
	}
}

func TestBlast(t *testing.T) {
	g := started(t)
	place(g, 0, 4, 4)
	place(g, 1, 12, 10)
	g.Map[tl(6, 4)] = proto.TBlock
	g.hidden[tl(6, 4)] = proto.TKick
	g.Map[tl(4, 2)] = proto.TFire // powerup: burnt, stops the arm
	g.Act(0, proto.ActDrop, tl(4, 4), 0)
	if g.Map[tl(4, 4)] != proto.TBomb || g.P[0].Out != 1 {
		t.Fatal("no bomb")
	}
	g.Act(0, proto.ActDrop, tl(4, 4), 0)
	if len(g.bombs) != 1 {
		t.Fatal("dropped past the limit")
	}
	place(g, 0, 0, 0) // run
	g.Advance(FuseJ)
	want := map[int]byte{
		tl(4, 4): proto.TFlameC,
		tl(5, 4): proto.TFlameR, tl(6, 4): proto.TBurn, // the block stops the arm
		tl(3, 4): proto.TFlameH, tl(2, 4): proto.TFlameL,
		tl(4, 5): proto.TFlameV, tl(4, 6): proto.TFlameD,
		tl(4, 3): proto.TFlameV, tl(4, 2): proto.TFlameU,
	}
	for k, v := range want {
		if g.Map[k] != v {
			t.Errorf("tile %d,%d = %d want %d", k%aw, k/aw, g.Map[k], v)
		}
	}
	if g.P[0].Out != 0 {
		t.Error("bomb still counted")
	}
	g.Advance(FlameJ)
	if g.Map[tl(4, 4)] != proto.TFloor || g.Map[tl(4, 2)] != proto.TFloor {
		t.Error("flames stayed, or the powerup survived")
	}
	if g.Map[tl(6, 4)] != proto.TKick {
		t.Errorf("block left %d, want the hidden kick", g.Map[tl(6, 4)])
	}
	// Pick it up.
	place(g, 0, 6, 4)
	g.Advance(1)
	if !g.P[0].Kick || g.Map[tl(6, 4)] != proto.TFloor {
		t.Error("no pickup")
	}
}

func TestChainAndDeath(t *testing.T) {
	g := started(t)
	g.Rounds = 1
	place(g, 0, 0, 0)
	place(g, 1, 2, 2)
	g.Act(0, proto.ActDrop, tl(0, 0), 0)
	g.Advance(100)
	place(g, 0, 2, 0)
	g.Act(0, proto.ActDrop, tl(2, 0), 0) // limit 1: refused
	g.P[0].Bombs = 2
	g.Act(0, proto.ActDrop, tl(2, 0), 0)
	place(g, 0, 12, 0)
	g.Advance(FuseJ - 100)
	// The first bomb's flame (range 2) reaches the second: both went.
	if len(g.bombs) != 0 {
		t.Fatalf("%d bombs left", len(g.bombs))
	}
	if g.Map[tl(2, 2)] != proto.TFlameD {
		t.Fatalf("chain blast didn't reach 2,2: %d", g.Map[tl(2, 2)])
	}
	// Seat 1 stands in it: dead after the grace.
	g.Advance(DeathGraceJ)
	if g.P[1].Alive {
		t.Fatal("survived the flames")
	}
	g.Advance(SettleJ)
	if g.Phase != proto.PhRoundEnd || g.Result != 0 || g.P[0].Wins != 1 {
		t.Fatalf("phase %d result %d wins %d", g.Phase, g.Result, g.P[0].Wins)
	}
	g.Advance(RoundEndJ)
	if g.Phase != proto.PhGameOver || g.Result != 0 {
		t.Fatalf("game over: phase %d result %d", g.Phase, g.Result)
	}
	g.Advance(GameOverJ)
	if g.Phase != proto.PhWait || g.Join("CAT") != 2 {
		t.Fatal("back to the waiting room")
	}
}

func TestGrace(t *testing.T) {
	g := started(t)
	place(g, 0, 0, 0)
	place(g, 1, 12, 10)
	g.Act(0, proto.ActDrop, tl(0, 0), 0)
	g.Advance(FuseJ)
	g.Advance(DeathGraceJ - 2)
	place(g, 0, 0, 4) // out just in time
	g.Advance(FlameJ)
	if !g.P[0].Alive {
		t.Fatal("died inside the grace")
	}
}

func TestKick(t *testing.T) {
	g := started(t)
	place(g, 1, 12, 10)
	place(g, 0, 0, 0)
	g.Act(0, proto.ActDrop, tl(0, 0), 0)
	place(g, 0, 0, 2)
	g.Map[tl(0, 8)] = proto.TBlock
	// No kick: nothing.
	g.Act(0, proto.ActBump, tl(0, 0), proto.DirUp)
	g.P[0].Kick = true
	place(g, 0, 0, 1)
	// Kicked up from below? (it's at row 0: can't go up)
	g.Act(0, proto.ActBump, tl(0, 0), proto.DirUp)
	if g.Map[tl(0, 0)] != proto.TBomb {
		t.Fatal("kicked into the wall")
	}
	place(g, 0, 1, 0)
	g.Act(0, proto.ActBump, tl(0, 0), proto.DirDown)
	if g.Map[tl(0, 0)] != proto.TFloor || g.movers[0] == nil {
		t.Fatal("no slide")
	}
	g.Advance(7*unit/SlideSpeed + 1)
	if g.Map[tl(0, 7)] != proto.TBomb || g.movers[0] != nil {
		t.Fatalf("stopped at %v", g.State()[proto.OffMovers:])
	}
}

func TestThrow(t *testing.T) {
	g := started(t)
	place(g, 1, 12, 10)
	place(g, 0, 2, 0)
	g.P[0].Glove = true
	g.Act(0, proto.ActDrop, tl(2, 0), 0)
	g.Act(0, proto.ActThrow, proto.DirDown, 0)
	if g.movers[0] == nil {
		t.Fatal("no throw")
	}
	g.Advance(3 * unit / FlySpeed)
	if g.Map[tl(2, 3)] != proto.TBomb {
		t.Fatal("not at 2,3")
	}
	// Thrown over the edge onto a pillar: carries on, and wraps.
	place(g, 0, 2, 3)
	g.Act(0, proto.ActThrow, proto.DirLeft, 0) // 2,3 -> -1,3 = 12,3: floor
	g.Advance(3 * unit / FlySpeed)
	if g.Map[tl(12, 3)] != proto.TBomb {
		t.Fatalf("wrap: %v", g.Map[tl(12, 3)])
	}
	place(g, 0, 12, 3)
	g.Act(0, proto.ActThrow, proto.DirLeft, 0) // 9,3 is a pillar: on to 8,3
	g.Advance(4 * unit / FlySpeed)
	if g.Map[tl(8, 3)] != proto.TBomb {
		t.Fatal("didn't carry past the pillar")
	}
	// Only your own.
	place(g, 1, 8, 3)
	g.P[1].Glove = true
	g.Act(1, proto.ActThrow, proto.DirUp, 0)
	if g.Map[tl(8, 3)] != proto.TBomb {
		t.Fatal("threw someone else's bomb")
	}
}

func TestDud(t *testing.T) {
	g := started(t)
	place(g, 1, 12, 10)
	place(g, 0, 4, 0)
	g.dudOdds = 1
	g.Act(0, proto.ActDrop, tl(4, 0), 0)
	place(g, 0, 5, 0)
	g.Advance(FuseJ)
	if g.Map[tl(4, 0)] != proto.TDud || g.P[0].Out != 0 {
		t.Fatal("no dud, or still counted")
	}
	g.relight = 1 << 30
	g.Act(0, proto.ActBump, tl(4, 0), proto.DirLeft)
	if g.Map[tl(4, 0)] != proto.TDud {
		t.Fatal("relit against the odds")
	}
	g.relight = 1
	g.Act(0, proto.ActBump, tl(4, 0), proto.DirLeft) // too soon: no roll
	if g.Map[tl(4, 0)] != proto.TDud {
		t.Fatal("rolled twice in a second")
	}
	g.Advance(RollEveryJ)
	g.Act(0, proto.ActBump, tl(4, 0), proto.DirLeft)
	if g.Map[tl(4, 0)] != proto.TBomb {
		t.Fatal("not relit")
	}
	g.dudOdds = 1 << 30
	place(g, 0, 8, 0)
	g.Advance(RelightJ)
	if g.Map[tl(4, 0)] != proto.TFlameC {
		t.Fatalf("relit dud didn't go off: %d", g.Map[tl(4, 0)])
	}
	// A dud in a blast goes off.
	g.Advance(FlameJ)
	g.dudOdds = 1
	place(g, 0, 0, 0)
	g.Act(0, proto.ActDrop, tl(0, 0), 0)
	place(g, 0, 0, 4)
	g.Advance(FuseJ)
	g.dudOdds = 1 << 30
	place(g, 0, 2, 0)
	g.Act(0, proto.ActDrop, tl(2, 0), 0)
	place(g, 0, 2, 4)
	g.Advance(FuseJ)
	if g.Map[tl(0, 0)] != proto.TFlameC {
		t.Fatalf("dud in the blast: %d", g.Map[tl(0, 0)])
	}
}

func TestSuddenDeath(t *testing.T) {
	g := started(t)
	g.Rounds = 1
	g.Join("CAT") // too late: watches
	// Both die together: a draw, then sudden death between the tied.
	place(g, 0, 0, 0)
	place(g, 1, 1, 0)
	g.Act(0, proto.ActDrop, tl(0, 0), 0)
	g.Advance(FuseJ + DeathGraceJ + SettleJ)
	if g.Phase != proto.PhRoundEnd || g.Result != proto.Draw {
		t.Fatalf("phase %d result %d", g.Phase, g.Result)
	}
	g.Advance(RoundEndJ)
	if g.Phase != proto.PhCountdown || !g.Sudden || g.Round != 2 {
		t.Fatalf("no sudden death: phase %d sudden %v round %d", g.Phase, g.Sudden, g.Round)
	}
}

func TestMoveCheck(t *testing.T) {
	g := started(t)
	w := g.P[0].Warp
	if !g.Move(0, 4, 0, proto.DirRight|proto.PDirMoving, w) {
		t.Fatal("honest move refused")
	}
	if g.Move(0, 100, 0, proto.DirRight, w) || g.P[0].Warp == w {
		t.Fatal("teleport accepted, or no warp")
	}
	if g.Move(0, 8, 0, proto.DirRight, w) {
		t.Fatal("move with a stale warp accepted")
	}
	w = g.P[0].Warp
	if g.Move(0, 16, 16, proto.DirDown, w) {
		t.Fatal("move into a pillar accepted")
	}
	if g.Move(0, 6, 6, proto.DirDown, g.P[0].Warp) {
		t.Fatal("move off both lanes accepted")
	}
}

func TestLeaveMidRound(t *testing.T) {
	g := started(t)
	g.Join("CAT")
	g.Act(1, proto.ActLeave, 0, 0)
	g.Advance(SettleJ + 1)
	if g.Phase != proto.PhRoundEnd || g.Result != 0 {
		t.Fatalf("phase %d result %d", g.Phase, g.Result)
	}
	g.Advance(RoundEndJ)
	if g.Phase != proto.PhGameOver || g.Result != 0 {
		t.Fatalf("one left: phase %d result %d", g.Phase, g.Result)
	}
}

func TestStateSize(t *testing.T) {
	g := started(t)
	if n := len(g.State()); n != proto.StateSize || n > 256 {
		t.Fatalf("state %d bytes", n)
	}
}
