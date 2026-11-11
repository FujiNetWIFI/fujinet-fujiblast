package server

import (
	"bufio"
	"bytes"
	"log"
	"net"
	"os"
	"strings"
	"sync"
	"time"

	"fujiblast/internal/proto"
)

// Stream timing (package vars so tests can change them).
var (
	PushEvery = 50 * time.Millisecond // how often to look for changes
	Idle      = 35 * time.Second      // no frame from the client: drop it
)

var debug = os.Getenv("FUJIBLAST_STREAM_DEBUG") != ""

// Serve accepts stream clients on ln until it fails.
func (s *Server) Serve(ln net.Listener) error {
	for {
		nc, err := ln.Accept()
		if err != nil {
			return err
		}
		go s.handle(nc)
	}
}

// Listen serves on addr (":9910") in the background.
func (s *Server) Listen(addr string) (net.Addr, error) {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return nil, err
	}
	log.Printf("stream on %s", ln.Addr())
	go func() { log.Println("stream:", s.Serve(ln)) }()
	return ln.Addr(), nil
}

type conn struct {
	s  *Server
	nc net.Conn

	mu      sync.Mutex
	table   *Table
	name    string // "" watches
	last    []byte // the state the client has once q is out
	q       [][]byte
	sent    uint8 // encoded bytes sent, mod 256
	taken   uint8 // bytes the client took from its ring (SC_CREDIT), mod 256
	lastSeq byte
	acked   bool // lastSeq was applied (a repeat is answered, not applied)
	ack     bool // a commit owed for ackSeq
	ackSeq  byte
	total   int
	t0      time.Time
	kick    chan struct{}
	done    chan struct{}
}

func (s *Server) handle(nc net.Conn) {
	if tc, ok := nc.(*net.TCPConn); ok {
		tc.SetNoDelay(true)
	}
	c := &conn{s: s, nc: nc, kick: make(chan struct{}, 1), done: make(chan struct{}), t0: time.Now()}
	defer nc.Close()
	go c.writer()
	c.reader()
	close(c.done)
	c.mu.Lock()
	t := c.table
	c.mu.Unlock()
	if t != nil {
		t.part(c)
	}
	if secs := time.Since(c.t0).Seconds(); secs > 0 {
		log.Printf("stream %s: closed, %d bytes out (%.0f B/s)", nc.RemoteAddr(), c.total, float64(c.total)/secs)
	}
}

func (c *conn) poke() {
	select {
	case c.kick <- struct{}{}:
	default:
	}
}

func (c *conn) reader() {
	br := bufio.NewReader(c.nc)
	var buf []byte
	for {
		c.nc.SetReadDeadline(time.Now().Add(Idle))
		b, err := br.ReadByte()
		if err != nil {
			return
		}
		if b != 0 {
			if len(buf) < 2*proto.MaxFrame {
				buf = append(buf, b)
			}
			continue
		}
		raw, ok := proto.COBSDecode(buf)
		if debug {
			log.Printf("stream %s: in % x", c.nc.RemoteAddr(), buf)
		}
		buf = buf[:0]
		if !ok {
			continue // a REGISTER preamble, or noise
		}
		typ, p, ok := proto.Unframe(raw)
		if !ok {
			continue
		}
		if !c.frame(typ, p) {
			return
		}
	}
}

func cstr(b []byte) string {
	if i := bytes.IndexByte(b, 0); i >= 0 {
		b = b[:i]
	}
	return strings.TrimSpace(string(b))
}

// frame handles one frame; false hangs up.
func (c *conn) frame(typ byte, p []byte) bool {
	s := c.s
	switch typ {
	case proto.SCHello:
		if len(p) < 2+proto.TableLen+proto.NameLen {
			return false
		}
		c.mu.Lock()
		old := c.table
		c.mu.Unlock()
		if old != nil {
			old.part(c)
		}
		if p[0] != proto.Version {
			c.queue(proto.Frame(proto.CSError, []byte{proto.ErrVersion}))
			break
		}
		t := s.Table(cstr(p[2 : 2+proto.TableLen]))
		name := strings.ToUpper(cstr(p[2+proto.TableLen : 2+proto.TableLen+proto.NameLen]))
		c.mu.Lock()
		c.table, c.name, c.last, c.q, c.acked, c.ack = t, name, nil, nil, false, false
		c.mu.Unlock()
		if t == nil {
			c.queue(proto.Frame(proto.CSError, []byte{proto.ErrNoTable}))
			break
		}
		t.join(c)
	case proto.SCTables:
		list := s.Tables(len(p) > 0 && p[0] == 1)
		if len(list) == 0 {
			c.queue(proto.Frame(proto.CSTable, []byte{0, 0}))
		}
		for i, ti := range list {
			c.queue(proto.Frame(proto.CSTable, append([]byte{byte(i), byte(len(list))}, record(ti)...)))
		}
	case proto.SCMove:
		if len(p) < 4 {
			return false
		}
		if t, name := c.who(); t != nil && name != "" {
			t.move(name, int(p[0]), int(p[1]), int(p[2]), p[3])
		}
		return true // the next push carries it; no hurry
	case proto.SCAct:
		if len(p) < 4 {
			return false
		}
		t, name := c.who()
		if t == nil {
			return true
		}
		c.mu.Lock()
		again := c.acked && p[0] == c.lastSeq
		c.lastSeq, c.acked, c.ack, c.ackSeq = p[0], true, true, p[0]
		c.mu.Unlock()
		if again {
			break // sent again because the answer was lost: not twice
		}
		if p[1] == proto.ActLeave {
			t.act(name, proto.ActLeave, 0, 0)
			c.mu.Lock()
			c.name = "" // watch from now on; HELLO sits down again
			c.mu.Unlock()
		} else if name != "" {
			t.act(name, int(p[1]), int(p[2]), int(p[3]))
		}
	case proto.SCResync:
		c.mu.Lock()
		c.last, c.q = nil, nil
		c.mu.Unlock()
	case proto.SCCredit:
		if len(p) > 0 {
			c.mu.Lock()
			c.taken = p[0]
			if int(c.sent-c.taken) > proto.Window {
				c.taken = c.sent // credit for more than was sent: don't wedge
			}
			c.mu.Unlock()
		}
	case proto.SCBye:
		if t, name := c.who(); t != nil && name != "" {
			t.act(name, proto.ActLeave, 0, 0)
		}
		return false
	case proto.SCPing:
		c.mu.Lock()
		c.ack, c.ackSeq = true, 0 // answered with a commit: the client hears the link is up
		c.mu.Unlock()
	default:
		return true
	}
	c.poke()
	return true
}

func (c *conn) who() (*Table, string) {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.table, c.name
}

func (c *conn) queue(f []byte) {
	c.mu.Lock()
	c.q = append(c.q, f)
	c.mu.Unlock()
}

func (c *conn) writer() {
	tk := time.NewTicker(PushEvery)
	defer tk.Stop()
	for {
		select {
		case <-c.done:
			return
		case <-tk.C:
		case <-c.kick:
		}
		if err := c.push(); err != nil {
			c.nc.Close()
			return
		}
	}
}

// push queues the state if nothing is waiting and the client isn't behind,
// then sends as much of the queue as its credit allows.
func (c *conn) push() error {
	c.mu.Lock()
	t, ack := c.table, c.ack
	// A client still holding half a window unread is behind: a state built
	// now would be stale when it got there. An answer goes at once.
	behind := int(c.sent-c.taken) > proto.Window/2
	idle := len(c.q) == 0
	c.mu.Unlock()
	if t != nil && idle && (ack || !behind) {
		st := t.state()
		c.mu.Lock()
		if c.ack || !bytes.Equal(st, c.last) {
			for _, p := range proto.Patches(c.last, st) {
				c.q = append(c.q, proto.Frame(proto.CSPatch, p))
			}
			var seq byte
			if c.ack {
				seq = c.ackSeq
			}
			c.q = append(c.q, proto.Frame(proto.CSCommit, []byte{seq}))
			c.last, c.ack = st, false
		}
		c.mu.Unlock()
	}
	c.mu.Lock()
	inflight := int(c.sent - c.taken)
	var out []byte
	for len(c.q) > 0 && inflight+len(out)+len(c.q[0]) <= proto.Window {
		out = append(out, c.q[0]...)
		c.q = c.q[1:]
	}
	c.sent += uint8(len(out))
	c.total += len(out)
	c.mu.Unlock()
	if len(out) == 0 {
		return nil
	}
	if debug {
		log.Printf("stream %s: out %d bytes, in flight %d", c.nc.RemoteAddr(), len(out), inflight+len(out))
	}
	c.nc.SetWriteDeadline(time.Now().Add(Idle))
	_, err := c.nc.Write(out)
	return err
}
