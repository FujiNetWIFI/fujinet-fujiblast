# FujiBlast stream protocol

The protocol runs over one TCP connection per client, to `fujiblastd` port 9910. On the Atari that connection is FujiNet netstream. The constants are in `server/internal/proto/proto.go`, and `fujiblastd -cheader` writes the same ones to `src/proto.h`.

## Frames

A frame is the COBS encoding of `type, payload…, s1, s2`, followed by a `0`.

- `s1` and `s2` are a Fletcher-16 checksum over the type and payload, each sum mod 255.
- A decoded frame is at most 128 bytes.
- A receiver drops any frame that is malformed or fails its checksum.

This framing is M.U.L.E.'s stream mode (fujinet-multiplayer-mule, `docs/PROTOCOL.md`).

## Client to server

| Type | Payload | Meaning |
|---|---|---|
| `H` | ver, flags, table[8], name[8] | Sit at the table. The name gets the player's old seat back, matched in any case. Otherwise the client takes a free seat in the waiting room, or watches. Flags: bit 0 (`HELLO_BOT`) marks a house bot, which is host only when no person is seated; clients send 0. A person can't take a seat a bot holds (or the reverse): they watch. |
| `T` | (1: list the dev tables too) | List the tables. |
| `M` | x, y, dir\|moving, warp | Where the client's player is. |
| `A` | seq, act, a1, a2 | An action (below). Seq is never 0. A repeated seq is answered but not applied again. |
| `R` | | Send the whole state again. |
| `C` | taken | Bytes taken from the receive ring so far, mod 256. |
| `P` | | Keepalive. The server answers it with a commit. |
| `B` | | Leave the table and hang up. |

Actions:

| Action | Arguments | Meaning |
|---|---|---|
| `DROP` | tile | Drop a bomb. |
| `THROW` | dir | Throw the bomb the player stands on (needs the glove). |
| `BUMP` | tile, dir | The player walked into a bomb or dud: it gets kicked, or a dud gets its relight roll. |
| `ROUNDS` | n | The host sets the number of rounds, 1 to 9. |
| `START` | | The host starts the game; needs at least 2 seated. |
| `LEAVE` | | Give up the seat and watch. A new `H` sits down again. |

## Server to client

| Type | Payload | Meaning |
|---|---|---|
| `Q` | runs of off, len, bytes… | Patches to the state being built. |
| `C` | ack seq | The state being built is now whole. Seq is the action it answers, or 0. |
| `T` | index, count, record[24] | One table: id[8], name[12], seated, max, phase, rounds. A count of 0 means there are no tables. |
| `E` | code | 1: no such table. 2: wrong version. |

**Flow control.** The server keeps at most 192 encoded bytes unacknowledged.

- If more than 96 bytes are outstanding, the client is behind. The server then skips building the next state and sends a newer one once the client catches up.
- An action's answer goes out at once.
- A link silent for 35 s is dropped.

## The state (247 bytes)

| Offset | Contents |
|---|---|
| 0 | Header, 16 bytes: phase, round, rounds, host, timer (s), result (seat, 4 for a draw, 255 for none), seated, alive, in-play and connected masks, epoch, watchers, sudden-death flag, round seq. |
| 16 | `players[4]`, 10 bytes each: x, y (16 units a tile), dir\|moving, flags (kick, glove), range, bombs, speed, out, wins, warp. |
| 56 | `names[4][8]`. |
| 88 | `map[13×11]`, one tile code per tile, row major. Hidden powerups are not in it; their tiles show as blocks. |
| 231 | `movers[4]`, 4 bytes each: kind (slide, fly, \|0x80 dud), x, y, 0. |

## Who owns what

**The client owns its own movement.**
- The client moves its player with `src/move.c`, which is the same code as `internal/game/move.go`, and sends `M` at most 20 times a second.
- The server checks each move against the speed allowance plus 24 units of slack, against pillars and blocks, and against the lanes.
- If a move fails, the server bumps `warp`. The client snaps to the server's position when it sees `warp` change.
- An `M` carrying an old `warp` is ignored.

**The server owns everything else**: bombs, flames, pickups, deaths and rounds.
- A player standing in flames for 6 jiffies dies. The grace covers the lag on `M`.

## The lobby

The lobby passes the game its details in appkeys, all under creator 1, app 1:

| Key | Contents |
|---|---|
| 0 | The player's name. |
| 0x10 | `tcp://host:port/?table=id` |

The server posts to the lobby only when `FUJIBLAST_LOBBY` is set to `qa`, `prod` or a URL (`internal/lobby`).
