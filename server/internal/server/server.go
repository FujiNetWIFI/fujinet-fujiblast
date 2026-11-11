// Package server runs FujiBlast tables and speaks the stream protocol
// (docs/PROTOCOL.md) to clients over TCP: FujiNet netstream on the 8-bits.
package server

import (
	"log"
	"strings"
	"sync"
	"time"

	"fujiblast/internal/game"
	"fujiblast/internal/proto"
)

// TableInfo is what the lobby and the table list show.
type TableInfo struct {
	ID, Name string
	Seated   int
	Phase    int
	Rounds   int
}

// Server holds the tables.
type Server struct {
	Now      func() time.Time
	OnChange func(TableInfo) // a table's seats or phase changed (the lobby)

	mu     sync.Mutex
	tables []*Table
}

// Table is one room: a game and the connections watching or playing it.
type Table struct {
	ID, Name string
	Dev      bool

	s     *Server
	mu    sync.Mutex
	g     *game.Game
	t0    time.Time
	conns map[*conn]struct{}
	info  TableInfo
}

// New makes a server with the public tables blast1..4 and the dev tables
// dev1..3 (listed only when asked for).
func New(seed uint64) *Server {
	s := &Server{Now: time.Now}
	for i := 1; i <= 4; i++ {
		s.add("blast"+string(rune('0'+i)), "ROOM "+string(rune('0'+i)), false, seed+uint64(i))
	}
	for i := 1; i <= 3; i++ {
		s.add("dev"+string(rune('0'+i)), "DEV "+string(rune('0'+i)), true, seed+100+uint64(i))
	}
	return s
}

func (s *Server) add(id, name string, dev bool, seed uint64) {
	t := &Table{ID: id, Name: name, Dev: dev, s: s, g: game.New(seed), t0: s.Now(), conns: map[*conn]struct{}{}}
	t.info = t.infoLocked()
	s.tables = append(s.tables, t)
}

// Table is the table with id (any case), or nil.
func (s *Server) Table(id string) *Table {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, t := range s.tables {
		if strings.EqualFold(t.ID, id) {
			return t
		}
	}
	return nil
}

// Tables lists the tables (dev: the dev ones too).
func (s *Server) Tables(dev bool) []TableInfo {
	s.mu.Lock()
	ts := append([]*Table(nil), s.tables...)
	s.mu.Unlock()
	var out []TableInfo
	for _, t := range ts {
		if t.Dev && !dev {
			continue
		}
		t.mu.Lock()
		out = append(out, t.infoLocked())
		t.mu.Unlock()
	}
	return out
}

// Run advances every table to the wall clock, every tick, until stop closes.
func (s *Server) Run(tick time.Duration, stop <-chan struct{}) {
	tk := time.NewTicker(tick)
	defer tk.Stop()
	for {
		select {
		case <-stop:
			return
		case <-tk.C:
		}
		s.Update()
	}
}

// Update advances every table to now.
func (s *Server) Update() {
	s.mu.Lock()
	ts := append([]*Table(nil), s.tables...)
	s.mu.Unlock()
	for _, t := range ts {
		t.mu.Lock()
		t.syncLocked()
		changed := t.noteChange()
		t.mu.Unlock()
		if changed && s.OnChange != nil {
			s.OnChange(t.info)
		}
	}
}

// syncLocked runs the game up to now and seats watchers in the waiting room.
func (t *Table) syncLocked() {
	want := int64(t.s.Now().Sub(t.t0) * 60 / time.Second)
	if n := want - t.g.J; n > 0 {
		if n > 600 {
			n = 600 // a stalled server doesn't replay minutes of game in one go
			t.t0 = t.s.Now().Add(-time.Duration(t.g.J+n) * time.Second / 60)
		}
		t.g.Advance(int(n))
	}
	spect := 0
	for c := range t.conns {
		if c.name == "" {
			spect++
			continue
		}
		if t.g.SeatOf(c.name) < 0 && t.g.Join(c.name) < 0 {
			spect++
		}
	}
	t.g.Spect = spect
}

func (t *Table) infoLocked() TableInfo {
	n := 0
	for i := range t.g.P {
		if t.g.P[i].Seated {
			n++
		}
	}
	return TableInfo{ID: t.ID, Name: t.Name, Seated: n, Phase: t.g.Phase, Rounds: t.g.Rounds}
}

// noteChange reports (once) that the lobby's view of t changed.
func (t *Table) noteChange() bool {
	i := t.infoLocked()
	if i.Seated == t.info.Seated && (i.Phase == proto.PhWait) == (t.info.Phase == proto.PhWait) {
		return false
	}
	t.info = i
	return true
}

func (t *Table) join(c *conn) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.syncLocked()
	t.conns[c] = struct{}{}
	if s := t.g.Join(c.name); s >= 0 {
		log.Printf("table %s: %s in seat %d", t.ID, c.name, s)
	}
	t.syncLocked()
}

func (t *Table) part(c *conn) {
	t.mu.Lock()
	defer t.mu.Unlock()
	delete(t.conns, c)
	if c.name == "" {
		return
	}
	for o := range t.conns {
		if strings.EqualFold(o.name, c.name) {
			return // another link for the same player
		}
	}
	t.g.SetConn(t.g.SeatOf(c.name), false)
}

// state is the game as of now.
func (t *Table) state() []byte {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.syncLocked()
	return t.g.State()
}

func (t *Table) move(name string, x, y, dir int, warp byte) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.syncLocked()
	t.g.Move(t.g.SeatOf(name), x, y, dir, warp)
}

func (t *Table) act(name string, act, a1, a2 int) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.syncLocked()
	seat := t.g.SeatOf(name)
	t.g.Act(seat, act, a1, a2)
}

// record is a CS_TABLE record.
func record(ti TableInfo) []byte {
	r := make([]byte, proto.TableRec)
	copy(r[0:8], ti.ID)
	copy(r[8:20], ti.Name)
	r[20], r[21], r[22], r[23] = byte(ti.Seated), proto.Seats, byte(ti.Phase), byte(ti.Rounds)
	return r
}
