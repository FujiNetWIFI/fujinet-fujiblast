package game

// Tuning. Times are jiffies (1/60 s), distances position units (16 a tile).
const (
	FuseJ       = 150 // a bomb's fuse
	FlameJ      = 30  // flames, and a block burning
	DudOdds     = 80  // 1 bomb in DudOdds fizzles
	RelightOdds = 3   // a touch relights a dud 1 time in RelightOdds
	RelightJ    = 60  // a relit dud's fuse
	RollEveryJ  = 60  // one relight roll per player per this long
	SlideSpeed  = 2   // kicked bomb, units a jiffy
	FlySpeed    = 4   // thrown bomb
	ThrowTiles  = 3
	LandFuseJ   = 30 // a thrown bomb's fuse is at least this when it lands
	DeathGraceJ = 6  // in flames this long: dead (covers the move upload's lag)
	RoundJ      = 180 * 60
	SettleJ     = 90 // after one is left: a late death makes it a draw
	CountdownJ  = 180
	RoundEndJ   = 360
	GameOverJ   = 900
	BlockPct    = 70 // free tiles that get a block
	PowerPct    = 25 // blocks that hide a powerup
	StartRange  = 2
	StartBombs  = 1
	MaxRange    = 8
	MaxBombs    = 8
	MoveSlack   = 24      // units a move may differ from the speed's reach
	WaitGoneJ   = 10 * 60 // link down this long in the waiting room: seat freed
	GoneJ       = 30 * 60 // ... during a game: freed when the round ends
	DefRounds   = 3
)

// Powerup mix: weights of Fire, BombUp, Speed, Kick, Glove.
var powerWeights = [5]int{30, 30, 20, 10, 10}
