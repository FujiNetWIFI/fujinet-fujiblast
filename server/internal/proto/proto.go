// Package proto is FujiBlast's wire format: frame types, the binary game
// state the server pushes as patches, and the ids both sides share. CHeader
// writes the same constants as src/proto.h for the 8-bit clients.
package proto

import (
	"fmt"
	"io"
)

const (
	Version   = 1
	MaxFrame  = 128          // decoded frame, type and sum included
	MaxPatch  = MaxFrame - 3 // a CS_PATCH payload
	Window    = 192          // encoded bytes in flight to one client
	Port      = 9910
	NameLen   = 8
	TableLen  = 8
	Seats     = 4
	Movers    = 4
	ArenaW    = 13
	ArenaH    = 11
	Tiles     = ArenaW * ArenaH
	Unit      = 16 // position units per tile, both axes
	MaxX      = (ArenaW - 1) * Unit
	MaxY      = (ArenaH - 1) * Unit
	MaxRounds = 9
	NoSeat    = 0xFF
	Draw      = 4 // OffResult: nobody won the round
)

// HELLO flags.
const HelloBot = 1 // a house bot: never the host while a human is seated

// Client -> server frame types.
const (
	SCHello  = 'H' // ver, flags (HelloBot), table[8], name[8] (NUL padded)
	SCTables = 'T' // list the tables (payload 1: the dev tables too)
	SCMove   = 'M' // x, y, dir|moving, warp: where my player is
	SCAct    = 'A' // seq, act, a1, a2
	SCResync = 'R' // send the whole state again
	SCCredit = 'C' // bytes taken from the receive ring so far, mod 256
	SCPing   = 'P' // keepalive
	SCBye    = 'B' // leave the table and hang up
)

// Server -> client frame types.
const (
	CSPatch  = 'Q' // runs of off, len, bytes...: into the state being built
	CSCommit = 'C' // ack seq: the state being built is whole
	CSTable  = 'T' // index, count, table record (count 0: no tables)
	CSError  = 'E' // ERR_*
)

// Errors (CS_ERROR).
const (
	ErrNoTable = 1 + iota
	ErrVersion
)

// Actions (SC_ACT act).
const (
	ActNone   = iota
	ActDrop   // a1: tile
	ActThrow  // a1: dir
	ActBump   // a1: tile, a2: dir: walked into a bomb or dud (kick, relight)
	ActRounds // a1: 1..MaxRounds (host, waiting room)
	ActStart  // host, waiting room
	ActLeave  // give up the seat, keep watching
)

// Phases (OffPhase).
const (
	PhWait = iota
	PhCountdown
	PhPlay
	PhRoundEnd
	PhGameOver
)

// Map tiles (OffMap). Hidden powerups are the server's; the map shows Block.
const (
	TFloor = iota
	TPillar
	TBlock
	TBurn // a block on fire: floor or a powerup when the flames go
	TBomb
	TDud
	TFlameC // flames: centre or crossing
	TFlameH
	TFlameV
	TFlameL // arm ends
	TFlameR
	TFlameU
	TFlameD
	TFire // powerups
	TBombUp
	TSpeed
	TKick
	TGlove
	NTiles
)

// Directions (P_DIR bits 0-1, SC_MOVE, ActThrow, ActBump).
const (
	DirUp = iota
	DirRight
	DirDown
	DirLeft
)

// State layout.
const (
	OffPhase  = 0  // PH_*
	OffRound  = 1  // round being played, 1-based (0: none yet)
	OffRounds = 2  // rounds in this game
	OffHost   = 3  // seat that picks the rounds and starts (NoSeat)
	OffTimer  = 4  // seconds: left in the round, the countdown, or the screen
	OffResult = 5  // round or game winner's seat, Draw, or NoSeat
	OffSeated = 6  // bit n: seat n taken
	OffAlive  = 7  // bit n: seat n alive this round
	OffInPlay = 8  // bit n: seat n plays this round (sudden death: the tied)
	OffEpoch  = 9  // +1 each game
	OffConn   = 10 // bit n: seat n's link is up
	OffSpect  = 11 // watchers
	OffSudden = 12 // 1: a sudden-death round
	OffSeq    = 13 // +1 each round: the client resets its view

	OffPlayers = 16
	PlayerSize = 10
	PX         = 0 // position, units (Unit per tile)
	PY         = 1
	PDir       = 2 // DIR_* | PDirMoving
	PFlags     = 3 // PF*
	PRange     = 4 // flame range
	PBombs     = 5 // bombs at once
	PSpeed     = 6 // speed level, 0..MaxSpeed
	POut       = 7 // bombs on the field now
	PWins      = 8 // trophies this game
	PWarp      = 9 // +1 when the server moves the player: the client snaps

	OffNames  = OffPlayers + Seats*PlayerSize // Seats x NameLen, NUL padded
	OffMap    = OffNames + Seats*NameLen      // Tiles, row major
	OffMovers = OffMap + Tiles                // Movers x MoverSize
	MoverSize = 4                             // kind|MDud, x, y, 0
	StateSize = OffMovers + Movers*MoverSize
)

const (
	PDirMoving = 0x04
	PFKick     = 0x01
	PFGlove    = 0x02

	MSlide = 1 // mover kinds
	MFly   = 2
	MDud   = 0x80 // a dud being kicked
)

// Table record (CS_TABLE): id[8], name[12], seated, max, phase, rounds.
const TableRec = 24

// Rules the client needs to move like the server expects.
const (
	SpeedBase = 16 // 1/16 units per jiffy at speed level 0
	SpeedStep = 4  // per level
	MaxSpeed  = 4
	Lane      = 6 // units off a lane that still turn the corner
)

// The constants, in the order src/proto.h has them.
var cdefs = []struct {
	name string
	val  int
}{
	{"PROTO_VERSION", Version}, {"MAX_FRAME", MaxFrame}, {"STREAM_PORT", Port},
	{"NAME_LEN", NameLen}, {"TABLE_LEN", TableLen}, {"SEATS", Seats}, {"MOVERS", Movers},
	{"ARENA_W", ArenaW}, {"ARENA_H", ArenaH}, {"TILES", Tiles}, {"UNIT", Unit},
	{"MAX_X", MaxX}, {"MAX_Y", MaxY}, {"MAX_ROUNDS", MaxRounds}, {"NO_SEAT", NoSeat}, {"DRAW", Draw},
	{"SC_HELLO", SCHello}, {"SC_TABLES", SCTables}, {"SC_MOVE", SCMove}, {"SC_ACT", SCAct},
	{"SC_RESYNC", SCResync}, {"SC_CREDIT", SCCredit}, {"SC_PING", SCPing}, {"SC_BYE", SCBye},
	{"CS_PATCH", CSPatch}, {"CS_COMMIT", CSCommit}, {"CS_TABLE", CSTable}, {"CS_ERROR", CSError},
	{"ERR_NO_TABLE", ErrNoTable}, {"ERR_VERSION", ErrVersion},
	{"ACT_DROP", ActDrop}, {"ACT_THROW", ActThrow}, {"ACT_BUMP", ActBump},
	{"ACT_ROUNDS", ActRounds}, {"ACT_START", ActStart}, {"ACT_LEAVE", ActLeave},
	{"PH_WAIT", PhWait}, {"PH_COUNTDOWN", PhCountdown}, {"PH_PLAY", PhPlay},
	{"PH_ROUNDEND", PhRoundEnd}, {"PH_GAMEOVER", PhGameOver},
	{"T_FLOOR", TFloor}, {"T_PILLAR", TPillar}, {"T_BLOCK", TBlock}, {"T_BURN", TBurn},
	{"T_BOMB", TBomb}, {"T_DUD", TDud}, {"T_FLAME_C", TFlameC}, {"T_FLAME_H", TFlameH},
	{"T_FLAME_V", TFlameV}, {"T_FLAME_L", TFlameL}, {"T_FLAME_R", TFlameR},
	{"T_FLAME_U", TFlameU}, {"T_FLAME_D", TFlameD}, {"T_FIRE", TFire}, {"T_BOMBUP", TBombUp},
	{"T_SPEED", TSpeed}, {"T_KICK", TKick}, {"T_GLOVE", TGlove}, {"N_TILES", NTiles},
	{"DIR_UP", DirUp}, {"DIR_RIGHT", DirRight}, {"DIR_DOWN", DirDown}, {"DIR_LEFT", DirLeft},
	{"OFF_PHASE", OffPhase}, {"OFF_ROUND", OffRound}, {"OFF_ROUNDS", OffRounds},
	{"OFF_HOST", OffHost}, {"OFF_TIMER", OffTimer}, {"OFF_RESULT", OffResult},
	{"OFF_SEATED", OffSeated}, {"OFF_ALIVE", OffAlive}, {"OFF_INPLAY", OffInPlay},
	{"OFF_EPOCH", OffEpoch}, {"OFF_CONN", OffConn}, {"OFF_SPECT", OffSpect},
	{"OFF_SUDDEN", OffSudden}, {"OFF_SEQ", OffSeq},
	{"OFF_PLAYERS", OffPlayers}, {"PLAYER_SIZE", PlayerSize},
	{"P_X", PX}, {"P_Y", PY}, {"P_DIR", PDir}, {"P_FLAGS", PFlags}, {"P_RANGE", PRange},
	{"P_BOMBS", PBombs}, {"P_SPEED", PSpeed}, {"P_OUT", POut}, {"P_WINS", PWins}, {"P_WARP", PWarp},
	{"OFF_NAMES", OffNames}, {"OFF_MAP", OffMap}, {"OFF_MOVERS", OffMovers},
	{"MOVER_SIZE", MoverSize}, {"STATE_SIZE", StateSize},
	{"P_DIR_MOVING", PDirMoving}, {"PF_KICK", PFKick}, {"PF_GLOVE", PFGlove},
	{"M_SLIDE", MSlide}, {"M_FLY", MFly}, {"M_DUD", MDud}, {"TABLE_REC", TableRec},
	{"SPEED_BASE", SpeedBase}, {"SPEED_STEP", SpeedStep}, {"MAX_SPEED", MaxSpeed}, {"LANE", Lane},
}

// CHeader writes src/proto.h.
func CHeader(w io.Writer) {
	fmt.Fprintln(w, "/* proto.h -- generated by `fujiblastd -cheader`; do not edit. */")
	fmt.Fprintln(w, "#ifndef PROTO_H\n#define PROTO_H")
	for _, d := range cdefs {
		fmt.Fprintf(w, "#define %-14s %d\n", d.name, d.val)
	}
	fmt.Fprintln(w, "#endif")
}
