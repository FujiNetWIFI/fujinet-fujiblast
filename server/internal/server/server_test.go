package server

import (
	"bytes"
	"math/rand/v2"
	"net"
	"sync"
	"testing"
	"time"

	"fujiblast/internal/bot"
	"fujiblast/internal/client"
	"fujiblast/internal/proto"
)

type clock struct {
	mu sync.Mutex
	t  time.Time
}

func (c *clock) now() time.Time { c.mu.Lock(); defer c.mu.Unlock(); return c.t }
func (c *clock) add(d time.Duration) {
	c.mu.Lock()
	c.t = c.t.Add(d)
	c.mu.Unlock()
}

func serve(t *testing.T) (*Server, *clock, string) {
	t.Helper()
	PushEvery = time.Millisecond
	ck := &clock{t: time.Unix(1e9, 0)}
	s := New(1)
	s.Now = ck.now
	for _, tb := range s.tables {
		tb.t0 = ck.now()
	}
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ln.Close() })
	go s.Serve(ln)
	return s, ck, ln.Addr().String()
}

func waitFor(t *testing.T, c *client.Client, what string, ok func() bool) {
	t.Helper()
	for i := 0; i < 200; i++ {
		if _, err := c.Poll(5 * time.Millisecond); err != nil {
			t.Fatal(err)
		}
		if ok() {
			return
		}
	}
	t.Fatalf("no %s", what)
}

func TestPatches(t *testing.T) {
	r := rand.New(rand.NewPCG(1, 2))
	for k := 0; k < 500; k++ {
		prev := make([]byte, proto.StateSize)
		for i := range prev {
			prev[i] = byte(r.IntN(4))
		}
		cur := append([]byte(nil), prev...)
		for n := r.IntN(80); n > 0; n-- {
			cur[r.IntN(len(cur))] = byte(r.IntN(256))
		}
		if k%50 == 0 {
			prev = nil
		}
		got := make([]byte, proto.StateSize)
		if prev != nil {
			copy(got, prev)
		}
		for _, p := range proto.Patches(prev, cur) {
			if len(p) > proto.MaxPatch {
				t.Fatalf("payload %d bytes", len(p))
			}
			raw, ok := proto.COBSDecode(bytes.TrimSuffix(proto.Frame(proto.CSPatch, p), []byte{0}))
			typ, q, ok2 := proto.Unframe(raw)
			if !ok || !ok2 || typ != proto.CSPatch || len(raw) > proto.MaxFrame {
				t.Fatal("frame doesn't round-trip")
			}
			if !proto.ApplyPatch(got, q) {
				t.Fatal("bad patch")
			}
		}
		if !bytes.Equal(got, cur) {
			t.Fatalf("patched state differs (case %d)", k)
		}
	}
}

func TestHello(t *testing.T) {
	_, _, addr := serve(t)
	c, err := client.Dial(addr)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	c.Hello("dev1", "ann")
	waitFor(t, c, "seat", func() bool { return c.Seat("ANN") == 0 })
	if c.State[proto.OffHost] != 0 || c.State[proto.OffConn] != 1 {
		t.Fatalf("host %d conn %d", c.State[proto.OffHost], c.State[proto.OffConn])
	}
	seq, _ := c.Act(proto.ActRounds, 5, 0)
	waitFor(t, c, "ack", func() bool { return c.Acked == seq && c.State[proto.OffRounds] == 5 })

	d, _ := client.Dial(addr)
	defer d.Close()
	d.Hello("nope", "bob")
	waitFor(t, d, "error", func() bool { return d.Err == proto.ErrNoTable })
	d.ListTables(false)
	waitFor(t, d, "tables", func() bool { return len(d.Tables) == 4 })
	if string(bytes.TrimRight(d.Tables[0][:8], "\x00")) != "blast1" {
		t.Fatalf("table 0 %q", d.Tables[0][:8])
	}
	d.ListTables(true)
	waitFor(t, d, "dev tables", func() bool { return len(d.Tables) == 7 })
}

// A client that never gives credit gets at most a window.
func TestWindow(t *testing.T) {
	_, ck, addr := serve(t)
	nc, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	defer nc.Close()
	p := append([]byte{proto.Version, 0}, []byte("dev2\x00\x00\x00\x00ANN\x00\x00\x00\x00\x00")...)
	nc.Write(proto.Frame(proto.SCHello, p))
	nc.Write(proto.Frame(proto.SCTables, []byte{1}))
	total := 0
	buf := make([]byte, 4096)
	for i := 0; i < 50; i++ {
		ck.add(time.Second / 10)
		nc.SetReadDeadline(time.Now().Add(10 * time.Millisecond))
		n, _ := nc.Read(buf)
		total += n
	}
	if total == 0 || total > proto.Window {
		t.Fatalf("%d bytes without credit", total)
	}
}

// Four bots play a two-round game to the end on a fast clock.
func TestBotsPlayAGame(t *testing.T) {
	s, ck, addr := serve(t)
	var bots []*bot.Bot
	for i, n := range []string{"B1", "B2", "B3", "B4"} {
		b, err := bot.New(addr, "dev3", n, uint64(i+1))
		if err != nil {
			t.Fatal(err)
		}
		defer b.C.Close()
		if i == 0 {
			b.Start, b.Rounds = 4, 2
		}
		bots = append(bots, b)
		waitFor(t, b.C, "seat", func() bool { return b.C.Seat(n) == i })
	}
	tb := s.Table("dev3")
	phases := map[int]bool{}
	for j := 0; j < 60*60*10; j++ {
		ck.add(time.Second / 60)
		s.Update()
		var wg sync.WaitGroup
		for _, b := range bots {
			wg.Add(1)
			go func() {
				defer wg.Done()
				if _, err := b.C.Poll(200 * time.Microsecond); err != nil {
					t.Error(err)
				}
				if err := b.Tick(); err != nil {
					t.Error(err)
				}
			}()
		}
		wg.Wait()
		tb.mu.Lock()
		ph, res, ep := tb.g.Phase, tb.g.Result, tb.g.Epoch
		tb.mu.Unlock()
		phases[ph] = true
		if ph == proto.PhGameOver {
			t.Logf("game over after %d s of game time: result %d", j/60, res)
			for _, b := range bots {
				t.Logf("%s: %d bytes in", b.Name, b.C.BytesIn)
			}
			return
		}
		if ep == 0 && j > 600 {
			t.Fatal("never started")
		}
	}
	t.Fatalf("no game over; phases seen %v", phases)
}

// House bots seat 1/2/3/0 at blast1..4; a person is host; they never start.
func TestHouseBots(t *testing.T) {
	s, ck, addr := serve(t)
	plan, err := ParseBots(DefaultBots)
	if err != nil {
		t.Fatal(err)
	}
	stop := make(chan struct{})
	done := make(chan struct{})
	go func() { HouseBots(addr, plan, stop); close(done) }()
	defer func() { close(stop); <-done }()

	seated := func() (out [4]int) {
		for _, ti := range s.Tables(false) {
			out[ti.ID[5]-'1'] = ti.Seated
		}
		return
	}
	for i := 0; i < 200 && seated() != [4]int{1, 2, 3, 0}; i++ {
		ck.add(time.Second / 60)
		s.Update()
		time.Sleep(10 * time.Millisecond)
	}
	if got := seated(); got != [4]int{1, 2, 3, 0} {
		t.Fatalf("seated %v", got)
	}
	if n := s.Tables(false)[3].Name; n != "HUMANS ONLY" {
		t.Fatalf("room 4 is %q", n)
	}

	// A person joining blast2 is host, in the seat after the bots.
	h, _ := client.Dial(addr)
	defer h.Close()
	h.Hello("blast2", "ann")
	waitFor(t, h, "seat", func() bool { return h.Seat("ANN") == 2 })
	if h.State[proto.OffHost] != 2 {
		t.Fatalf("host %d, want the person in seat 2", h.State[proto.OffHost])
	}

	// A person named BOT1 doesn't get the bot's seat or its powers.
	x, _ := client.Dial(addr)
	defer x.Close()
	x.Hello("blast2", "bot1")
	for i := 0; i < 20; i++ {
		x.Poll(5 * time.Millisecond)
	}
	x.Act(proto.ActLeave, 0, 0)
	for i := 0; i < 20; i++ {
		ck.add(time.Second / 60)
		s.Update()
		x.Poll(5 * time.Millisecond)
	}
	tb := s.Table("blast2")
	tb.mu.Lock()
	p := tb.g.P[0]
	tb.mu.Unlock()
	if !p.Bot || !p.Conn || p.Name != "BOT1" {
		t.Fatalf("bot seat taken over: %+v", p)
	}

	// The person leaves: host goes back to a bot, and nothing starts by itself.
	h.Act(proto.ActLeave, 0, 0)
	for i := 0; i < 600; i++ {
		ck.add(time.Second / 60)
		s.Update()
		h.Poll(time.Millisecond)
	}
	tb.mu.Lock()
	host, ph := tb.g.Host, tb.g.Phase
	tb.mu.Unlock()
	if host != 0 || ph != proto.PhWait {
		t.Fatalf("host %d phase %d after the person left", host, ph)
	}
}
