package server

import (
	"fmt"
	"log"
	"strconv"
	"strings"
	"sync"
	"time"

	"fujiblast/internal/bot"
)

// DefaultBots is the house bots per public table.
const DefaultBots = "blast1=1,blast2=2,blast3=3"

// ParseBots reads "blast1=1,blast2=2": bots per table.
func ParseBots(spec string) (map[string]int, error) {
	plan := map[string]int{}
	for _, f := range strings.Split(spec, ",") {
		if f = strings.TrimSpace(f); f == "" {
			continue
		}
		id, n, ok := strings.Cut(f, "=")
		v, err := strconv.Atoi(n)
		if !ok || err != nil || v < 0 || v > 3 {
			return nil, fmt.Errorf("bots: %q: want table=0..3", f)
		}
		plan[id] = v
	}
	return plan, nil
}

// HouseBots seats plan[table] flagged bots (BOT1..) at each table over the
// stream at addr, and sits them down again if their link drops. They never
// start a game: a person at the table is the host. It returns when stop
// closes and the bots have left.
func HouseBots(addr string, plan map[string]int, stop <-chan struct{}) {
	var wg sync.WaitGroup
	for table, n := range plan {
		for i := 0; i < n; i++ {
			wg.Add(1)
			go func(table, name string, seed uint64) {
				defer wg.Done()
				for {
					b, err := bot.NewHouse(addr, table, name, seed)
					if err == nil {
						err = b.Run(stop)
						b.C.Close()
					}
					select {
					case <-stop:
						return
					case <-time.After(5 * time.Second):
					}
					if err != nil {
						log.Printf("house bot %s/%s: %v", table, name, err)
					}
				}
			}(table, fmt.Sprintf("BOT%d", i+1), uint64(time.Now().UnixNano())+uint64(i))
			time.Sleep(100 * time.Millisecond) // sit down in order
		}
	}
	wg.Wait()
}
