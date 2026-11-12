// Package game is FujiBlast's rules: a deterministic engine advanced one
// jiffy (1/60 s) at a time. Each client owns its player's position (Move);
// the engine owns everything else and judges deaths from those positions.
package game

import (
	"math/rand/v2"
	"strings"

	"fujiblast/internal/proto"
)

const (
	aw    = proto.ArenaW
	ah    = proto.ArenaH
	tiles = proto.Tiles
)

// Player is one seat.
type Player struct {
	Name   string
	Seated bool
	Bot    bool // a house bot: host only when no human is seated
	Conn   bool
	X, Y   int
	Dir    int // DIR_* | PDirMoving
	Alive  bool
	InPlay bool
	Range  int
	Bombs  int
	Speed  int
	Out    int
	Wins   int
	Kick   bool
	Glove  bool
	Warp   byte

	inFlame  int
	lastMove int64 // jiffy of the last accepted move
	lastRoll int64
	goneAt   int64 // jiffy the link went down (Conn false)
}

type bomb struct {
	tile    int
	owner   int
	rng     int
	fuse    int
	dud     bool
	counted bool // in the owner's Out
	mv      *mover
}

type mover struct {
	kind   int // MSlide, MFly
	x, y   int
	dx, dy int
	left   int // MFly: units to the landing tile
	flown  int // MFly: units so far (a bomb with nowhere to land fizzles)
	b      *bomb
}

// Game is one table's game.
type Game struct {
	J      int64
	Phase  int
	Round  int
	Rounds int
	Host   int
	Result int
	Epoch  byte
	Seq    byte
	Sudden bool
	Spect  int
	P      [proto.Seats]Player
	Map    [tiles]byte

	rng     *rand.Rand
	dudOdds int // DudOdds, RelightOdds (tests change them)
	relight int
	phaseJ  int64
	settleJ int64
	hidden  [tiles]byte
	flame   [tiles]int
	bombs   []*bomb
	movers  [proto.Movers]*mover
	queue   []*bomb // to explode this jiffy
}

// New makes an empty table; seed fixes the arenas and the dice.
func New(seed uint64) *Game {
	g := &Game{rng: rand.New(rand.NewPCG(seed, seed^0x9E3779B97F4A7C15)),
		Rounds: DefRounds, Host: proto.NoSeat, Result: proto.NoSeat, settleJ: -1,
		dudOdds: DudOdds, relight: RelightOdds}
	return g
}

// SeatOf is name's seat, or -1.
func (g *Game) SeatOf(name string) int {
	for i := range g.P {
		if g.P[i].Seated && strings.EqualFold(g.P[i].Name, name) {
			return i
		}
	}
	return -1
}

// Join seats name: its old seat if it has one, else a free one in the
// waiting room. -1: it watches.
func (g *Game) Join(name string) int { return g.JoinAs(name, false) }

// JoinAs is Join for a house bot (bot) or a person. A name already seated
// as the other kind is not theirs: they watch.
func (g *Game) JoinAs(name string, bot bool) int {
	if name == "" {
		return -1
	}
	if s := g.SeatOf(name); s >= 0 {
		if g.P[s].Bot != bot {
			return -1
		}
		g.P[s].Conn = true
		return s
	}
	if g.Phase != proto.PhWait {
		return -1
	}
	for i := range g.P {
		if !g.P[i].Seated {
			g.P[i] = Player{Name: name, Seated: true, Bot: bot, Conn: true, Warp: g.P[i].Warp}
			g.fixHost()
			return i
		}
	}
	return -1
}

// SetConn records seat's link going up or down.
func (g *Game) SetConn(seat int, up bool) {
	if seat < 0 || !g.P[seat].Seated {
		return
	}
	p := &g.P[seat]
	if p.Conn && !up {
		p.goneAt = g.J
	}
	p.Conn = up
}

// Leave frees seat (it dies if it was playing).
func (g *Game) Leave(seat int) {
	if seat < 0 || !g.P[seat].Seated {
		return
	}
	g.P[seat] = Player{Warp: g.P[seat].Warp + 1}
	g.fixHost()
}

// fixHost makes the first person seated the host, else the first bot.
func (g *Game) fixHost() {
	g.Host = proto.NoSeat
	for i := range g.P {
		if g.P[i].Seated && !g.P[i].Bot {
			g.Host = i
			return
		}
	}
	for i := range g.P {
		if g.P[i].Seated {
			g.Host = i
			return
		}
	}
}

func (g *Game) seated() (n int) {
	for i := range g.P {
		if g.P[i].Seated {
			n++
		}
	}
	return n
}

// Move is seat's report of where its player is. False if the server
// didn't believe it: the player is warped back (PWarp) instead.
func (g *Game) Move(seat, x, y, dir int, warp byte) bool {
	if seat < 0 || seat >= proto.Seats {
		return false
	}
	p := &g.P[seat]
	if !p.Seated || warp != p.Warp {
		return false // sent before the client saw the last warp
	}
	if g.Phase != proto.PhPlay || !p.Alive {
		return true // standing still is all there is to do
	}
	reach := MoveSlack + int(g.J-p.lastMove)*(proto.SpeedBase+p.Speed*proto.SpeedStep)/16
	if abs(x-p.X)+abs(y-p.Y) > reach || !Fits(&g.Map, x, y) || (x%unit != 0 && y%unit != 0) {
		p.Warp++
		return false
	}
	p.X, p.Y, p.Dir = x, y, dir&(3|proto.PDirMoving)
	p.lastMove = g.J
	return true
}

// Act is seat's action.
func (g *Game) Act(seat, act, a1, a2 int) {
	if seat < 0 || seat >= proto.Seats || !g.P[seat].Seated {
		return
	}
	p := &g.P[seat]
	switch act {
	case proto.ActRounds:
		if g.Phase == proto.PhWait && seat == g.Host && a1 >= 1 && a1 <= proto.MaxRounds {
			g.Rounds = a1
		}
	case proto.ActStart:
		if g.Phase == proto.PhWait && seat == g.Host && g.seated() >= 2 {
			g.startGame()
		}
	case proto.ActLeave:
		g.Leave(seat)
	case proto.ActDrop:
		if g.Phase == proto.PhPlay && p.Alive {
			g.drop(seat, a1)
		}
	case proto.ActThrow:
		if g.Phase == proto.PhPlay && p.Alive && p.Glove {
			g.throw(seat, a1&3)
		}
	case proto.ActBump:
		if g.Phase == proto.PhPlay && p.Alive {
			g.bump(seat, a1, a2&3)
		}
	}
}

// near reports whether tile t is the player's centre tile or next to it.
func (g *Game) near(p *Player, t int) bool {
	if t < 0 || t >= tiles {
		return false
	}
	c := CentreTile(p.X, p.Y)
	dc, dr := abs(t%aw-c%aw), abs(t/aw-c/aw)
	return dc+dr <= 1
}

func (g *Game) drop(seat, t int) {
	p := &g.P[seat]
	if p.Out >= p.Bombs || !g.near(p, t) {
		return
	}
	switch g.Map[t] {
	case proto.TFloor:
	default:
		return
	}
	g.Map[t] = proto.TBomb
	g.bombs = append(g.bombs, &bomb{tile: t, owner: seat, rng: p.Range, fuse: FuseJ, counted: true})
	p.Out++
}

func (g *Game) bombAt(t int) *bomb {
	for _, b := range g.bombs {
		if b.mv == nil && b.tile == t {
			return b
		}
	}
	return nil
}

func (g *Game) freeMover() int {
	for i, m := range g.movers {
		if m == nil {
			return i
		}
	}
	return -1
}

func step(d int) (int, int) {
	switch d {
	case proto.DirUp:
		return 0, -1
	case proto.DirDown:
		return 0, 1
	case proto.DirLeft:
		return -1, 0
	}
	return 1, 0
}

func (g *Game) throw(seat, d int) {
	p := &g.P[seat]
	t := CentreTile(p.X, p.Y)
	b := g.bombAt(t)
	if b == nil || b.dud || b.owner != seat {
		return
	}
	i := g.freeMover()
	if i < 0 {
		return
	}
	dx, dy := step(d)
	g.Map[t] = proto.TFloor
	b.mv = &mover{kind: proto.MFly, x: t % aw * unit, y: t / aw * unit, dx: dx, dy: dy, left: ThrowTiles * unit, b: b}
	g.movers[i] = b.mv
}

func (g *Game) bump(seat, t, d int) {
	p := &g.P[seat]
	if !g.near(p, t) {
		return
	}
	b := g.bombAt(t)
	if b == nil {
		return
	}
	if b.dud && g.J-p.lastRoll >= RollEveryJ {
		p.lastRoll = g.J
		if g.rng.IntN(g.relight) == 0 {
			b.dud, b.fuse = false, RelightJ
			g.Map[t] = proto.TBomb
		}
	}
	if !p.Kick {
		return
	}
	dx, dy := step(d)
	if !g.slideFree(t%aw+dx, t/aw+dy) {
		return
	}
	i := g.freeMover()
	if i < 0 {
		return
	}
	g.Map[t] = proto.TFloor
	b.mv = &mover{kind: proto.MSlide, x: t % aw * unit, y: t / aw * unit, dx: dx, dy: dy, b: b}
	g.movers[i] = b.mv
}

// slideFree reports whether a sliding bomb may go on into c, r: floor or
// flames, and no player stands there.
func (g *Game) slideFree(c, r int) bool {
	if !inArena(c, r) {
		return false
	}
	t := r*aw + c
	if !isFlame(g.Map[t]) && g.Map[t] != proto.TFloor {
		return false
	}
	for i := range g.P {
		if g.P[i].Alive && CentreTile(g.P[i].X, g.P[i].Y) == t {
			return false
		}
	}
	for _, m := range g.movers {
		if m != nil && CentreTile(m.x, m.y) == t {
			return false
		}
	}
	return true
}

func isFlame(t byte) bool { return t >= proto.TFlameC && t <= proto.TFlameD }

func isPower(t byte) bool { return t >= proto.TFire && t <= proto.TGlove }

// Advance runs n jiffies.
func (g *Game) Advance(n int) {
	for ; n > 0; n-- {
		g.tick()
	}
}

func (g *Game) tick() {
	g.J++
	el := g.J - g.phaseJ
	switch g.Phase {
	case proto.PhWait:
		for i := range g.P {
			if p := &g.P[i]; p.Seated && !p.Conn && g.J-p.goneAt >= WaitGoneJ {
				g.Leave(i)
			}
		}
	case proto.PhCountdown:
		if el >= CountdownJ {
			g.setPhase(proto.PhPlay)
		}
	case proto.PhPlay:
		g.play()
	case proto.PhRoundEnd:
		if el >= RoundEndJ {
			g.nextRound()
		}
	case proto.PhGameOver:
		if el >= GameOverJ {
			g.setPhase(proto.PhWait)
			g.Round, g.Result, g.Sudden = 0, proto.NoSeat, false
			g.clearArena()
			for i := range g.P {
				g.P[i].InPlay, g.P[i].Alive = false, false
			}
		}
	}
}

func (g *Game) setPhase(ph int) {
	g.Phase, g.phaseJ = ph, g.J
}

func (g *Game) startGame() {
	g.Epoch++
	g.Round, g.Sudden, g.Result = 0, false, proto.NoSeat
	for i := range g.P {
		g.P[i].Wins = 0
		g.P[i].InPlay = g.P[i].Seated
	}
	g.startRound()
}

// startRound deals a new arena to the players marked InPlay.
func (g *Game) startRound() {
	g.Round++
	g.Seq++
	g.Result = proto.NoSeat
	g.settleJ = -1
	g.clearArena()
	g.genArena()
	k := 0
	corners := [4][2]int{{0, 0}, {aw - 1, ah - 1}, {aw - 1, 0}, {0, ah - 1}}
	for i := range g.P {
		p := &g.P[i]
		if !p.InPlay {
			p.Alive = false
			continue
		}
		c := corners[k]
		k++
		p.X, p.Y, p.Dir = c[0]*unit, c[1]*unit, proto.DirDown
		p.Alive, p.Range, p.Bombs, p.Speed, p.Out = true, StartRange, StartBombs, 0, 0
		p.Kick, p.Glove, p.inFlame = false, false, 0
		p.lastMove = g.J
		p.Warp++
	}
	g.setPhase(proto.PhCountdown)
}

func (g *Game) clearArena() {
	g.Map, g.hidden, g.flame = [tiles]byte{}, [tiles]byte{}, [tiles]int{}
	g.bombs, g.queue = nil, nil
	g.movers = [proto.Movers]*mover{}
}

// genArena lays pillars on odd/odd tiles and blocks at random, keeping
// each corner's L clear.
func (g *Game) genArena() {
	clear := func(c, r int) bool {
		for _, k := range [4][2]int{{0, 0}, {aw - 1, 0}, {0, ah - 1}, {aw - 1, ah - 1}} {
			dc, dr := abs(c-k[0]), abs(r-k[1])
			if dc+dr <= 1 {
				return true
			}
		}
		return false
	}
	sum := 0
	for _, w := range powerWeights {
		sum += w
	}
	for r := 0; r < ah; r++ {
		for c := 0; c < aw; c++ {
			t := r*aw + c
			switch {
			case c%2 == 1 && r%2 == 1:
				g.Map[t] = proto.TPillar
			case clear(c, r):
			case g.rng.IntN(100) < BlockPct:
				g.Map[t] = proto.TBlock
				if g.rng.IntN(100) < PowerPct {
					n := g.rng.IntN(sum)
					for k, w := range powerWeights {
						if n < w {
							g.hidden[t] = byte(proto.TFire + k)
							break
						}
						n -= w
					}
				}
			}
		}
	}
}

func (g *Game) play() {
	// Fuses.
	for _, b := range g.bombs {
		if b.dud || (b.mv != nil && b.mv.kind == proto.MFly) {
			continue
		}
		if b.fuse--; b.fuse <= 0 {
			if b.mv == nil && g.rng.IntN(g.dudOdds) == 0 {
				b.dud = true
				g.Map[b.tile] = proto.TDud
				g.uncount(b)
				continue
			}
			g.queue = append(g.queue, b)
		}
	}
	g.moveMovers()
	g.explode()
	// Flames and burning blocks die down.
	for t := range g.flame {
		if g.flame[t] == 0 {
			continue
		}
		if g.flame[t]--; g.flame[t] == 0 {
			if g.Map[t] == proto.TBurn {
				g.Map[t] = g.hidden[t]
				g.hidden[t] = 0
			} else if isFlame(g.Map[t]) {
				g.Map[t] = proto.TFloor
			}
		}
	}
	// Players: flames, pickups.
	alive := 0
	for i := range g.P {
		p := &g.P[i]
		if !p.Alive {
			continue
		}
		t := CentreTile(p.X, p.Y)
		if isFlame(g.Map[t]) {
			if p.inFlame++; p.inFlame >= DeathGraceJ {
				p.Alive = false
				continue
			}
		} else {
			p.inFlame = 0
		}
		if isPower(g.Map[t]) {
			g.pickup(p, g.Map[t])
			g.Map[t] = proto.TFloor
		}
		alive++
	}
	if alive <= 1 && g.settleJ < 0 {
		g.settleJ = g.J
	}
	if g.settleJ >= 0 && g.J-g.settleJ >= SettleJ {
		g.Result = proto.Draw
		for i := range g.P {
			if g.P[i].Alive {
				g.Result = i
				g.P[i].Wins++
			}
		}
		g.setPhase(proto.PhRoundEnd)
		return
	}
	if g.J-g.phaseJ >= RoundJ {
		g.Result = proto.Draw
		g.setPhase(proto.PhRoundEnd)
	}
}

func (g *Game) uncount(b *bomb) {
	if b.counted {
		b.counted = false
		if p := &g.P[b.owner]; p.Out > 0 {
			p.Out--
		}
	}
}

func (g *Game) pickup(p *Player, t byte) {
	switch t {
	case proto.TFire:
		p.Range = min(p.Range+1, MaxRange)
	case proto.TBombUp:
		p.Bombs = min(p.Bombs+1, MaxBombs)
	case proto.TSpeed:
		p.Speed = min(p.Speed+1, proto.MaxSpeed)
	case proto.TKick:
		p.Kick = true
	case proto.TGlove:
		p.Glove = true
	}
}

func (g *Game) moveMovers() {
	for i, m := range g.movers {
		if m == nil {
			continue
		}
		switch m.kind {
		case proto.MSlide:
			for k := 0; k < SlideSpeed; k++ {
				if m.x%unit == 0 && m.y%unit == 0 {
					c, r := m.x/unit, m.y/unit
					if isFlame(g.Map[r*aw+c]) {
						g.land(i, r*aw+c)
						g.queue = append(g.queue, m.b)
						break
					}
					if !g.slideFree(c+m.dx, r+m.dy) {
						g.land(i, r*aw+c)
						break
					}
				}
				m.x += m.dx
				m.y += m.dy
			}
		case proto.MFly:
			if m.flown += FlySpeed; m.flown > 4*aw*unit {
				g.movers[i] = nil
				g.remove(m.b)
				g.uncount(m.b)
				continue
			}
			for k := 0; k < FlySpeed; k++ {
				m.x = (m.x + m.dx + aw*unit) % (aw * unit)
				m.y = (m.y + m.dy + ah*unit) % (ah * unit)
				if m.left--; m.left > 0 {
					continue
				}
				t := (m.y/unit)*aw + m.x/unit
				if g.landFree(t) {
					g.land(i, t)
					if isFlame(g.Map[t]) {
						g.queue = append(g.queue, m.b)
					} else {
						m.b.fuse = max(m.b.fuse, LandFuseJ)
					}
					break
				}
				m.left = unit // something's there: on to the next tile
			}
		}
	}
}

// landFree: a thrown bomb may come down on floor or flames with nobody on it.
func (g *Game) landFree(t int) bool {
	if g.Map[t] != proto.TFloor && !isFlame(g.Map[t]) {
		return false
	}
	for i := range g.P {
		if g.P[i].Alive && CentreTile(g.P[i].X, g.P[i].Y) == t {
			return false
		}
	}
	return true
}

// land puts mover i's bomb on tile t.
func (g *Game) land(i, t int) {
	m := g.movers[i]
	g.movers[i] = nil
	b := m.b
	b.mv, b.tile = nil, t
	if !isFlame(g.Map[t]) {
		g.Map[t] = proto.TBomb
		if b.dud {
			g.Map[t] = proto.TDud
		}
	}
}

// explode sets off every queued bomb, and those their flames reach.
func (g *Game) explode() {
	for len(g.queue) > 0 {
		b := g.queue[0]
		g.queue = g.queue[1:]
		if !g.live(b) {
			continue
		}
		if b.mv != nil {
			if b.mv.kind != proto.MSlide {
				continue // in the air: goes off when it lands
			}
			for i, m := range g.movers {
				if m == b.mv {
					g.land(i, CentreTile(m.x, m.y))
				}
			}
		}
		g.remove(b)
		g.uncount(b)
		t := b.tile
		g.setFlame(t, proto.TFlameC)
		for d := 0; d < 4; d++ {
			dx, dy := step(d)
			c, r := t%aw, t/aw
			var arm []int
			for k := 1; k <= b.rng; k++ {
				c, r = c+dx, r+dy
				if !inArena(c, r) {
					break
				}
				u := r*aw + c
				stop := false
				switch tl := g.Map[u]; {
				case tl == proto.TPillar || tl == proto.TBurn:
					stop = true
				case tl == proto.TBlock:
					g.Map[u] = proto.TBurn
					g.flame[u] = FlameJ
					stop = true
				case tl == proto.TBomb || tl == proto.TDud:
					if o := g.bombAt(u); o != nil {
						g.queue = append(g.queue, o)
					}
					stop = true
				case isPower(tl):
					arm = append(arm, u)
					stop = true
				default:
					for _, m := range g.movers {
						if m != nil && m.kind == proto.MSlide && CentreTile(m.x, m.y) == u {
							g.queue = append(g.queue, m.b)
						}
					}
					arm = append(arm, u)
				}
				if stop {
					break
				}
			}
			mid, end := byte(proto.TFlameH), [4]byte{proto.TFlameU, proto.TFlameR, proto.TFlameD, proto.TFlameL}[d]
			if dx == 0 {
				mid = proto.TFlameV
			}
			for k, u := range arm {
				if k == len(arm)-1 {
					g.setFlame(u, end)
				} else {
					g.setFlame(u, mid)
				}
			}
		}
	}
	// A sliding bomb caught in flames goes off where it is.
	for i, m := range g.movers {
		if m != nil && m.kind == proto.MSlide && isFlame(g.Map[CentreTile(m.x, m.y)]) {
			g.land(i, CentreTile(m.x, m.y))
			g.queue = append(g.queue, m.b)
		}
	}
	if len(g.queue) > 0 {
		g.explode()
	}
}

func (g *Game) live(b *bomb) bool {
	for _, o := range g.bombs {
		if o == b {
			return true
		}
	}
	return false
}

func (g *Game) remove(b *bomb) {
	for i, o := range g.bombs {
		if o == b {
			g.bombs = append(g.bombs[:i], g.bombs[i+1:]...)
			return
		}
	}
}

// setFlame puts flame f on t; a different flame already there makes a crossing.
func (g *Game) setFlame(t int, f byte) {
	if isFlame(g.Map[t]) && g.Map[t] != f {
		f = proto.TFlameC
	}
	g.Map[t] = f
	g.flame[t] = FlameJ
}

// nextRound follows the round-end screen: another round, sudden death, or
// the end of the game.
func (g *Game) nextRound() {
	for i := range g.P {
		if p := &g.P[i]; p.Seated && !p.Conn && g.J-p.goneAt >= GoneJ {
			g.Leave(i)
		}
	}
	if g.seated() < 2 {
		g.gameOver()
		return
	}
	if g.Round < g.Rounds && !g.Sudden {
		for i := range g.P {
			g.P[i].InPlay = g.P[i].Seated
		}
		g.startRound()
		return
	}
	best, tied := -1, 0
	for i := range g.P {
		if p := &g.P[i]; p.Seated {
			switch {
			case p.Wins > best:
				best, tied = p.Wins, 1
			case p.Wins == best:
				tied++
			}
		}
	}
	if tied == 1 {
		g.gameOver()
		return
	}
	g.Sudden = true
	for i := range g.P {
		p := &g.P[i]
		p.InPlay = p.Seated && p.Wins == best
	}
	g.startRound()
}

func (g *Game) gameOver() {
	g.Result = proto.NoSeat
	best, tied := -1, false
	for i := range g.P {
		if p := &g.P[i]; p.Seated {
			switch {
			case p.Wins > best:
				best, tied, g.Result = p.Wins, false, i
			case p.Wins == best:
				tied = true
			}
		}
	}
	if tied {
		g.Result = proto.NoSeat
	}
	for i := range g.P {
		g.P[i].Alive = false
	}
	g.setPhase(proto.PhGameOver)
}

// State is the binary state (proto's layout).
func (g *Game) State() []byte {
	s := make([]byte, proto.StateSize)
	s[proto.OffPhase] = byte(g.Phase)
	s[proto.OffRound] = byte(g.Round)
	s[proto.OffRounds] = byte(g.Rounds)
	s[proto.OffHost] = byte(g.Host)
	s[proto.OffTimer] = byte(g.timer())
	s[proto.OffResult] = byte(g.Result)
	s[proto.OffEpoch] = g.Epoch
	s[proto.OffSpect] = byte(min(g.Spect, 255))
	if g.Sudden {
		s[proto.OffSudden] = 1
	}
	s[proto.OffSeq] = g.Seq
	for i := range g.P {
		p := &g.P[i]
		bit := byte(1) << i
		if p.Seated {
			s[proto.OffSeated] |= bit
		}
		if p.Alive {
			s[proto.OffAlive] |= bit
		}
		if p.InPlay {
			s[proto.OffInPlay] |= bit
		}
		if p.Conn {
			s[proto.OffConn] |= bit
		}
		o := proto.OffPlayers + i*proto.PlayerSize
		s[o+proto.PX], s[o+proto.PY], s[o+proto.PDir] = byte(p.X), byte(p.Y), byte(p.Dir)
		if p.Kick {
			s[o+proto.PFlags] |= proto.PFKick
		}
		if p.Glove {
			s[o+proto.PFlags] |= proto.PFGlove
		}
		s[o+proto.PRange], s[o+proto.PBombs], s[o+proto.PSpeed] = byte(p.Range), byte(p.Bombs), byte(p.Speed)
		s[o+proto.POut], s[o+proto.PWins], s[o+proto.PWarp] = byte(p.Out), byte(p.Wins), p.Warp
		copy(s[proto.OffNames+i*proto.NameLen:proto.OffNames+(i+1)*proto.NameLen], p.Name)
	}
	copy(s[proto.OffMap:], g.Map[:])
	for i, m := range g.movers {
		if m == nil {
			continue
		}
		o := proto.OffMovers + i*proto.MoverSize
		s[o] = byte(m.kind)
		if m.b.dud {
			s[o] |= proto.MDud
		}
		s[o+1], s[o+2] = byte(m.x), byte(m.y)
	}
	return s
}

func (g *Game) timer() int {
	el := int(g.J - g.phaseJ)
	left := 0
	switch g.Phase {
	case proto.PhCountdown:
		left = CountdownJ - el
	case proto.PhPlay:
		left = RoundJ - el
	case proto.PhRoundEnd:
		left = RoundEndJ - el
	case proto.PhGameOver:
		left = GameOverJ - el
	}
	return min(max((left+59)/60, 0), 255)
}

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}
