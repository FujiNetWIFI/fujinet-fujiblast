// Package lobby lists the tables in the FujiNet lobby
// (fujinet-lobby/server: POST /server). It posts a table when its seats
// change, every Keepalive regardless, and offline at shutdown.
package lobby

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"time"
)

const (
	QA   = "https://qalobby.fujinet.online/server"
	Prod = "https://lobby.fujinet.online/server"
)

var Keepalive = 5 * time.Minute

// Client is a platform's game image (tnfs://host/path, at most 64 chars).
type Client struct {
	Platform string `json:"platform"`
	URL      string `json:"url"`
}

// Config says where and how the server is listed.
type Config struct {
	Endpoint  string // QA, Prod or another lobby's /server
	Game      string // registered name: "FujiBlast"
	AppKey    int    // lobby key: 16
	Server    string // shown as the server's name
	Region    string
	ServerURL string // base: tcp://host:9910/ (the table is added as ?table=id)
	Clients   []Client
}

// Table is one table's lobby state.
type Table struct {
	ID      string
	Players int
	Online  bool
}

type entry struct {
	Game       string   `json:"game"`
	AppKey     int      `json:"appkey"`
	Server     string   `json:"server"`
	Region     string   `json:"region"`
	ServerURL  string   `json:"serverurl"`
	Status     string   `json:"status"`
	MaxPlayers int      `json:"maxplayers"`
	CurPlayers int      `json:"curplayers"`
	Clients    []Client `json:"clients"`
}

// Publisher posts table changes.
type Publisher struct {
	cfg  Config
	ch   chan Table
	http *http.Client
}

func New(cfg Config) *Publisher {
	return &Publisher{cfg: cfg, ch: make(chan Table, 64), http: &http.Client{Timeout: 10 * time.Second}}
}

// Notify queues a table's new state (never blocks; a full queue drops it,
// the keepalive catches up).
func (p *Publisher) Notify(t Table) {
	select {
	case p.ch <- t:
	default:
	}
}

// Run posts until stop closes, then posts every table offline.
func (p *Publisher) Run(tables []Table, stop <-chan struct{}) {
	cur := map[string]Table{}
	for _, t := range tables {
		t.Online = true
		cur[t.ID] = t
		p.post(t)
	}
	tk := time.NewTicker(Keepalive)
	defer tk.Stop()
	for {
		select {
		case t := <-p.ch:
			t.Online = true
			if cur[t.ID] != t {
				cur[t.ID] = t
				p.post(t)
			}
		case <-tk.C:
			for _, t := range cur {
				p.post(t)
			}
		case <-stop:
			for _, t := range cur {
				t.Online = false
				p.post(t)
			}
			return
		}
	}
}

func (p *Publisher) post(t Table) {
	e := entry{Game: p.cfg.Game, AppKey: p.cfg.AppKey, Server: p.cfg.Server + " " + t.ID, Region: p.cfg.Region,
		ServerURL: p.cfg.ServerURL + "?table=" + t.ID, Status: "online", MaxPlayers: 4,
		CurPlayers: t.Players, Clients: p.cfg.Clients}
	if !t.Online {
		e.Status = "offline"
	}
	body, _ := json.Marshal(e)
	resp, err := p.http.Post(p.cfg.Endpoint, "application/json", bytes.NewReader(body))
	if err != nil {
		log.Printf("lobby: %v", err)
		return
	}
	resp.Body.Close()
	if resp.StatusCode/100 != 2 {
		log.Printf("lobby: %s: %s", e.ServerURL, resp.Status)
	}
}

func (c Config) String() string { return fmt.Sprintf("%s as %q key %d", c.Endpoint, c.Game, c.AppKey) }
