// blastbot plays FujiBlast: n bots at one table, for soak tests and rigs.
//
//	blastbot [-addr localhost:9910] [-table dev1] [-n 3] [-start 4] [-rounds 3] [-games 0]
//
// The first bot is the host if it sits first: it starts once -start
// players are seated. -games stops after that many games (0: never).
package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"sync"
	"time"

	"fujiblast/internal/bot"
	"fujiblast/internal/proto"
)

func main() {
	addr := flag.String("addr", "localhost:9910", "server stream address")
	table := flag.String("table", "dev1", "table id")
	n := flag.Int("n", 3, "bots")
	start := flag.Int("start", 0, "host starts when this many are seated (0: never)")
	rounds := flag.Int("rounds", 3, "rounds the host asks for")
	games := flag.Int("games", 0, "stop after this many games (0: never)")
	prefix := flag.String("name", "BOT", "name prefix")
	flag.Parse()

	stop := make(chan struct{})
	var once sync.Once
	quit := func() { once.Do(func() { close(stop) }) }
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)
	go func() { <-sig; quit() }()

	var wg sync.WaitGroup
	for i := 0; i < *n; i++ {
		b, err := bot.New(*addr, *table, fmt.Sprintf("%s%d", *prefix, i+1), uint64(time.Now().UnixNano())+uint64(i))
		if err != nil {
			log.Fatal(err)
		}
		if i == 0 {
			b.Start, b.Rounds = *start, *rounds
		}
		wg.Add(1)
		go func(i int, b *bot.Bot) {
			defer wg.Done()
			if err := b.Run(stop); err != nil {
				log.Printf("%s: %v", b.Name, err)
			}
			log.Printf("%s: %d bytes in", b.Name, b.C.BytesIn)
		}(i, b)
		time.Sleep(200 * time.Millisecond) // sit down in order
	}
	if *games > 0 {
		go watch(stop, quit, *games, *n, *addr, *table)
	}
	wg.Wait()
}

// watch counts game overs on a watching connection and quits after games.
func watch(stop <-chan struct{}, quit func(), games, _ int, addr, table string) {
	b, err := bot.New(addr, table, "", 1)
	if err != nil {
		log.Print(err)
		return
	}
	last, done := byte(0), 0
	for {
		select {
		case <-stop:
			return
		default:
		}
		if _, err := b.C.Poll(50 * time.Millisecond); err != nil {
			log.Print(err)
			quit()
			return
		}
		st := &b.C.State
		ph := st[proto.OffPhase]
		if ph != last {
			log.Printf("phase %d round %d/%d result %d wins %d %d %d %d", ph, st[proto.OffRound], st[proto.OffRounds],
				st[proto.OffResult], st[proto.OffPlayers+proto.PWins], st[proto.OffPlayers+proto.PlayerSize+proto.PWins],
				st[proto.OffPlayers+2*proto.PlayerSize+proto.PWins], st[proto.OffPlayers+3*proto.PlayerSize+proto.PWins])
			if ph == proto.PhGameOver {
				if done++; done >= games {
					time.Sleep(time.Second)
					quit()
				}
			}
			last = ph
		}
	}
}
