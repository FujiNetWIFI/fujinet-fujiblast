// Package client speaks the stream protocol from the player's side, as the
// 8-bit clients do: for the bots and the tests.
package client

import (
	"bufio"
	"errors"
	"net"
	"os"
	"strings"
	"time"

	"fujiblast/internal/proto"
)

// Client is one connection.
type Client struct {
	nc    net.Conn
	br    *bufio.Reader
	buf   []byte
	stage [proto.StateSize]byte
	taken uint8
	owed  int
	seq   byte

	State   [proto.StateSize]byte // the last whole state
	Commits int
	Acked   byte
	Tables  [][]byte // CS_TABLE records, as listed
	Err     byte     // CS_ERROR
	BytesIn int
}

// Dial connects to a server's stream port.
func Dial(addr string) (*Client, error) {
	nc, err := net.DialTimeout("tcp", addr, 5*time.Second)
	if err != nil {
		return nil, err
	}
	return &Client{nc: nc, br: bufio.NewReader(nc)}, nil
}

func (c *Client) Close() error { return c.nc.Close() }

func (c *Client) send(typ byte, p []byte) error {
	c.nc.SetWriteDeadline(time.Now().Add(5 * time.Second))
	_, err := c.nc.Write(proto.Frame(typ, p))
	return err
}

func pad(s string, n int) []byte {
	b := make([]byte, n)
	copy(b, strings.ToUpper(s))
	return b
}

// Hello sits down at table as name.
func (c *Client) Hello(table, name string) error { return c.HelloFlags(table, name, 0) }

// HelloFlags is Hello with the HELLO flags byte (proto.HelloBot).
func (c *Client) HelloFlags(table, name string, flags byte) error {
	p := append([]byte{proto.Version, flags}, pad(table, proto.TableLen)...)
	return c.send(proto.SCHello, append(p, pad(name, proto.NameLen)...))
}

func (c *Client) ListTables(dev bool) error {
	if dev {
		return c.send(proto.SCTables, []byte{1})
	}
	return c.send(proto.SCTables, nil)
}

func (c *Client) Move(x, y, dir int, warp byte) error {
	return c.send(proto.SCMove, []byte{byte(x), byte(y), byte(dir), warp})
}

// Act sends an action and returns its seq (never 0).
func (c *Client) Act(act, a1, a2 int) (byte, error) {
	if c.seq++; c.seq == 0 {
		c.seq = 1
	}
	return c.seq, c.send(proto.SCAct, []byte{c.seq, byte(act), byte(a1), byte(a2)})
}

func (c *Client) Resync() error { return c.send(proto.SCResync, nil) }
func (c *Client) Ping() error   { return c.send(proto.SCPing, nil) }
func (c *Client) Bye() error    { return c.send(proto.SCBye, nil) }

// Poll reads what arrives within wait and applies it. It returns the
// number of commits read.
func (c *Client) Poll(wait time.Duration) (int, error) {
	n0 := c.Commits
	c.nc.SetReadDeadline(time.Now().Add(wait))
	for {
		b, err := c.br.ReadByte()
		if err != nil {
			if errors.Is(err, os.ErrDeadlineExceeded) {
				err = nil
			}
			return c.Commits - n0, err
		}
		c.BytesIn++
		c.taken++
		if c.owed++; c.owed >= 48 {
			c.owed = 0
			if err := c.send(proto.SCCredit, []byte{c.taken}); err != nil {
				return c.Commits - n0, err
			}
		}
		if b != 0 {
			if len(c.buf) < 2*proto.MaxFrame {
				c.buf = append(c.buf, b)
			}
			continue
		}
		raw, ok := proto.COBSDecode(c.buf)
		c.buf = c.buf[:0]
		if !ok {
			continue
		}
		typ, p, ok := proto.Unframe(raw)
		if !ok {
			continue
		}
		switch typ {
		case proto.CSPatch:
			proto.ApplyPatch(c.stage[:], p)
		case proto.CSCommit:
			c.State = c.stage
			c.Commits++
			if len(p) > 0 && p[0] != 0 {
				c.Acked = p[0]
			}
			if c.buf2() {
				return c.Commits - n0, nil
			}
		case proto.CSTable:
			if len(p) >= 2 && p[1] > 0 && len(p) >= 2+proto.TableRec {
				if int(p[0]) == 0 {
					c.Tables = nil
				}
				c.Tables = append(c.Tables, append([]byte(nil), p[2:2+proto.TableRec]...))
			}
		case proto.CSError:
			if len(p) > 0 {
				c.Err = p[0]
			}
		}
	}
}

// buf2 reports whether nothing more is buffered (stop at a commit then).
func (c *Client) buf2() bool { return c.br.Buffered() == 0 }

// Seat is name's seat in State, or -1.
func (c *Client) Seat(name string) int {
	for i := 0; i < proto.Seats; i++ {
		if c.State[proto.OffSeated]&(1<<i) == 0 {
			continue
		}
		o := proto.OffNames + i*proto.NameLen
		n := strings.TrimRight(string(c.State[o:o+proto.NameLen]), "\x00")
		if strings.EqualFold(n, name) {
			return i
		}
	}
	return -1
}

// Player is seat's record in State.
func (c *Client) Player(seat int) []byte {
	o := proto.OffPlayers + seat*proto.PlayerSize
	return c.State[o : o+proto.PlayerSize]
}

// Map is the arena in State.
func (c *Client) Map() *[proto.Tiles]byte {
	var m [proto.Tiles]byte
	copy(m[:], c.State[proto.OffMap:])
	return &m
}
