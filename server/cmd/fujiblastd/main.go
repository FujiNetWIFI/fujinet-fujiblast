// fujiblastd is the FujiBlast server.
//
//	fujiblastd [-stream :9910] [-seed n]
//	fujiblastd -cheader > ../src/proto.h
//
// Lobby listing is off unless FUJIBLAST_LOBBY is qa, prod or a lobby URL;
// then FUJIBLAST_SERVER_URL (tcp://host:9910/) and FUJIBLAST_CLIENT_ATARI
// (tnfs://host/path/fujiblst.atr), _APPLE2, _MSDOS and _INTV say what it
// lists.
package main

import (
	"flag"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"fujiblast/internal/lobby"
	"fujiblast/internal/proto"
	"fujiblast/internal/server"
)

func env(k, def string) string {
	if v := os.Getenv(k); v != "" {
		return v
	}
	return def
}

func main() {
	addr := flag.String("stream", env("FUJIBLAST_STREAM_ADDR", ":9910"), "stream listen address")
	seed := flag.Uint64("seed", uint64(time.Now().UnixNano()), "arena seed")
	cheader := flag.Bool("cheader", false, "write src/proto.h to stdout and exit")
	flag.Parse()
	if *cheader {
		proto.CHeader(os.Stdout)
		return
	}

	s := server.New(*seed)
	stop := make(chan struct{})
	done := make(chan struct{})
	if lb := os.Getenv("FUJIBLAST_LOBBY"); lb != "" {
		ep := map[string]string{"qa": lobby.QA, "prod": lobby.Prod}[lb]
		if ep == "" {
			ep = lb
		}
		cfg := lobby.Config{Endpoint: ep, Game: "FujiBlast", AppKey: 16, Server: "FujiBlast",
			Region: env("FUJIBLAST_REGION", "us"), ServerURL: env("FUJIBLAST_SERVER_URL", "tcp://localhost:9910/")}
		if u := os.Getenv("FUJIBLAST_CLIENT_ATARI"); u != "" {
			cfg.Clients = append(cfg.Clients, lobby.Client{Platform: "atari", URL: u})
		}
		if u := os.Getenv("FUJIBLAST_CLIENT_APPLE2"); u != "" {
			cfg.Clients = append(cfg.Clients, lobby.Client{Platform: "apple2", URL: u})
		}
		if u := os.Getenv("FUJIBLAST_CLIENT_MSDOS"); u != "" {
			cfg.Clients = append(cfg.Clients, lobby.Client{Platform: "msdos", URL: u})
		}
		if u := os.Getenv("FUJIBLAST_CLIENT_INTV"); u != "" {
			cfg.Clients = append(cfg.Clients, lobby.Client{Platform: "intv", URL: u})
		}
		pub := lobby.New(cfg)
		s.OnChange = func(ti server.TableInfo) { pub.Notify(lobby.Table{ID: ti.ID, Players: ti.Seated}) }
		var ts []lobby.Table
		for _, ti := range s.Tables(false) {
			ts = append(ts, lobby.Table{ID: ti.ID, Players: ti.Seated})
		}
		log.Printf("lobby: %s", cfg)
		go func() { pub.Run(ts, stop); close(done) }()
	} else {
		close(done)
	}
	if _, err := s.Listen(*addr); err != nil {
		log.Fatal(err)
	}
	go s.Run(time.Second/60, stop)

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
	<-sig
	close(stop)
	select {
	case <-done:
	case <-time.After(15 * time.Second):
	}
}
