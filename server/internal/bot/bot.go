// Package bot plays FujiBlast over the stream protocol, moving the way the
// 8-bit clients do (game.Walker). It is for soak tests and filling seats in
// test rigs, not a strong player: it runs from blasts, bombs blocks and
// rivals when it can get away, and picks things up.
package bot

import (
	"math/rand/v2"
	"time"

	"fujiblast/internal/client"
	"fujiblast/internal/game"
	"fujiblast/internal/proto"
)

const (
	aw = proto.ArenaW
	ah = proto.ArenaH
	u  = proto.Unit
)

// Bot is one player.
type Bot struct {
	C      *client.Client
	Name   string
	Table  string
	Start  int // host: start once this many are seated (0: never)
	Rounds int

	rng      *rand.Rand
	w        game.Walker
	warp     byte
	seq      byte // OffSeq of the round the walker belongs to
	alive    bool
	j        int
	lastSent [4]int
	sentJ    int
	bumpJ    int
	dropJ    int
	stuck    int
	wander   int
	started  byte // epoch we last asked to start
}

// New connects a bot and sits it down.
func New(addr, table, name string, seed uint64) (*Bot, error) {
	c, err := client.Dial(addr)
	if err != nil {
		return nil, err
	}
	b := &Bot{C: c, Name: name, Table: table, Rounds: 3, rng: rand.New(rand.NewPCG(seed, 7)), wander: -1}
	return b, c.Hello(table, name)
}

// Run plays until stop closes or the link fails.
func (b *Bot) Run(stop <-chan struct{}) error {
	tk := time.NewTicker(time.Second / 60)
	defer tk.Stop()
	for {
		select {
		case <-stop:
			b.C.Bye()
			return nil
		case <-tk.C:
		}
		if _, err := b.C.Poll(time.Millisecond); err != nil {
			return err
		}
		if err := b.Tick(); err != nil {
			return err
		}
	}
}

// Tick is one jiffy of play on the last state.
func (b *Bot) Tick() error {
	b.j++
	if b.j%300 == 0 { // a keepalive: the server drops a link silent for 35 s
		if err := b.C.Ping(); err != nil {
			return err
		}
	}
	st := &b.C.State
	seat := b.C.Seat(b.Name)
	if seat < 0 {
		return nil
	}
	ph := st[proto.OffPhase]
	if ph == proto.PhWait {
		b.alive = false
		if b.Start > 0 && int(st[proto.OffHost]) == seat && st[proto.OffEpoch] == b.started && b.j%60 == 0 {
			n := 0
			for i := 0; i < proto.Seats; i++ {
				if st[proto.OffSeated]&(1<<i) != 0 {
					n++
				}
			}
			if n >= b.Start {
				if _, err := b.C.Act(proto.ActRounds, b.Rounds, 0); err != nil {
					return err
				}
				if _, err := b.C.Act(proto.ActStart, 0, 0); err != nil {
					return err
				}
				b.started = st[proto.OffEpoch] + 1
			}
		}
		return nil
	}
	p := b.C.Player(seat)
	if p[proto.PWarp] != b.warp || st[proto.OffSeq] != b.seq {
		b.warp, b.seq = p[proto.PWarp], st[proto.OffSeq]
		b.w = game.Walker{X: int(p[proto.PX]), Y: int(p[proto.PY]), Dir: int(p[proto.PDir] & 3)}
	}
	b.alive = st[proto.OffAlive]&(1<<seat) != 0
	if ph != proto.PhPlay || !b.alive {
		return nil
	}
	m := b.C.Map()
	danger := b.danger(m)
	here := game.CentreTile(b.w.X, b.w.Y)
	d := -1
	switch {
	case danger[here]:
		d = b.toward(m, danger, here, func(t int) bool { return !danger[t] })
	default:
		if b.j-b.dropJ > 30 && int(p[proto.POut]) < int(p[proto.PBombs]) && m[here] == proto.TFloor && b.worthBombing(m, here, seat) && b.canEscape(m, danger, here, int(p[proto.PRange])) {
			if _, err := b.C.Act(proto.ActDrop, here, 0); err != nil {
				return err
			}
			b.dropJ = b.j
			m[here] = proto.TBomb // until the server says so
			if p[proto.PFlags]&proto.PFGlove != 0 && b.rng.IntN(3) == 0 {
				if _, err := b.C.Act(proto.ActThrow, b.w.Dir, 0); err != nil {
					return err
				}
			}
			return nil
		}
		d = b.toward(m, danger, here, func(t int) bool {
			if t == here {
				return false
			}
			if m[t] >= proto.TFire && m[t] <= proto.TGlove {
				return true
			}
			return b.worthBombing(m, t, seat) && t != b.wander
		})
		if d < 0 || b.stuck > 90 {
			d = b.rng.IntN(4)
			b.stuck = 0
		}
	}
	moved, bump := b.w.Step(m, d, int(p[proto.PSpeed]))
	if moved {
		b.stuck = 0
	} else if d >= 0 {
		b.stuck++
	}
	if bump >= 0 && b.j-b.bumpJ > 30 {
		b.bumpJ = b.j
		if _, err := b.C.Act(proto.ActBump, bump, d); err != nil {
			return err
		}
	}
	dir := b.w.Dir
	if moved {
		dir |= proto.PDirMoving
	}
	cur := [4]int{b.w.X, b.w.Y, dir, int(b.warp)}
	if cur != b.lastSent && b.j-b.sentJ >= 3 {
		b.lastSent, b.sentJ = cur, b.j
		return b.C.Move(b.w.X, b.w.Y, dir, b.warp)
	}
	return nil
}

func isFlame(t byte) bool { return t >= proto.TFlameC && t <= proto.TFlameD }

// danger marks flames, and every tile a bomb on the map may reach (at the
// largest range anyone has).
func (b *Bot) danger(m *[proto.Tiles]byte) (dz [proto.Tiles]bool) {
	rng := 2
	for i := 0; i < proto.Seats; i++ {
		rng = max(rng, int(b.C.Player(i)[proto.PRange]))
	}
	for t, v := range m {
		if isFlame(v) {
			dz[t] = true
		}
		if v == proto.TBomb {
			blast(m, t, rng, &dz)
		}
	}
	return dz
}

func blast(m *[proto.Tiles]byte, t, rng int, dz *[proto.Tiles]bool) {
	dz[t] = true
	for _, d := range [4][2]int{{0, -1}, {1, 0}, {0, 1}, {-1, 0}} {
		c, r := t%aw, t/aw
		for k := 0; k < rng; k++ {
			c, r = c+d[0], r+d[1]
			if c < 0 || c >= aw || r < 0 || r >= ah {
				break
			}
			v := m[r*aw+c]
			if v == proto.TPillar || v == proto.TBlock || v == proto.TBurn {
				break
			}
			dz[r*aw+c] = true
		}
	}
}

// worthBombing: a block next to t, or a rival within 2 tiles in line.
func (b *Bot) worthBombing(m *[proto.Tiles]byte, t, seat int) bool {
	c, r := t%aw, t/aw
	for _, d := range [4][2]int{{0, -1}, {1, 0}, {0, 1}, {-1, 0}} {
		cc, rr := c+d[0], r+d[1]
		if cc >= 0 && cc < aw && rr >= 0 && rr < ah && m[rr*aw+cc] == proto.TBlock {
			return true
		}
	}
	st := &b.C.State
	for i := 0; i < proto.Seats; i++ {
		if i == seat || st[proto.OffAlive]&(1<<i) == 0 {
			continue
		}
		p := b.C.Player(i)
		o := game.CentreTile(int(p[proto.PX]), int(p[proto.PY]))
		oc, or := o%aw, o/aw
		if (oc == c && abs(or-r) <= 2) || (or == r && abs(oc-c) <= 2) {
			return true
		}
	}
	return false
}

// canEscape: with a bomb at t, a safe tile is near enough.
func (b *Bot) canEscape(m *[proto.Tiles]byte, danger [proto.Tiles]bool, t, rng int) bool {
	dz := danger
	blast(m, t, rng, &dz)
	dist := bfs(m, t, nil)
	for v, d := range dist {
		if d >= 0 && d <= 5 && !dz[v] {
			return true
		}
	}
	return false
}

// bfs is each tile's distance from t on foot (-1: can't get there),
// avoiding tiles in avoid.
func bfs(m *[proto.Tiles]byte, t int, avoid *[proto.Tiles]bool) (dist [proto.Tiles]int) {
	for i := range dist {
		dist[i] = -1
	}
	dist[t] = 0
	q := []int{t}
	for len(q) > 0 {
		v := q[0]
		q = q[1:]
		c, r := v%aw, v/aw
		for _, d := range [4][2]int{{0, -1}, {1, 0}, {0, 1}, {-1, 0}} {
			cc, rr := c+d[0], r+d[1]
			if !game.Walkable(m, cc, rr) {
				continue
			}
			n := rr*aw + cc
			if dist[n] >= 0 || (avoid != nil && avoid[n] && isFlame(m[n])) {
				continue
			}
			dist[n] = dist[v] + 1
			q = append(q, n)
		}
	}
	return dist
}

// toward is the stick direction that heads for the nearest tile goal
// accepts (-1: none reachable).
func (b *Bot) toward(m *[proto.Tiles]byte, danger [proto.Tiles]bool, here int, goal func(int) bool) int {
	dist := bfs(m, here, &danger)
	best := -1
	for t, d := range dist {
		if d > 0 && goal(t) && (best < 0 || d < dist[best]) {
			best = t
		}
	}
	if best < 0 {
		return -1
	}
	// Walk back from best to the first step.
	step := best
	for dist[step] > 1 {
		c, r := step%aw, step/aw
		for _, d := range [4][2]int{{0, -1}, {1, 0}, {0, 1}, {-1, 0}} {
			cc, rr := c+d[0], r+d[1]
			if cc >= 0 && cc < aw && rr >= 0 && rr < ah && dist[rr*aw+cc] == dist[step]-1 {
				step = rr*aw + cc
				break
			}
		}
	}
	return b.dirTo(step)
}

// dirTo steers onto t, the tile next to the one under us on a path:
// lined up on the lane first, then along it.
func (b *Bot) dirTo(t int) int {
	tx, ty := t%aw*u, t/aw*u
	x, y := b.w.X, b.w.Y
	here := game.CentreTile(x, y)
	if t%aw != here%aw { // a step sideways: get onto the row
		switch {
		case y < ty:
			return proto.DirDown
		case y > ty:
			return proto.DirUp
		case tx > x:
			return proto.DirRight
		}
		return proto.DirLeft
	}
	switch {
	case x < tx:
		return proto.DirRight
	case x > tx:
		return proto.DirLeft
	case ty > y:
		return proto.DirDown
	case ty < y:
		return proto.DirUp
	}
	return -1
}

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}
