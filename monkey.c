#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define BOOL int
#define TRUE 1
#define FALSE 0
#define S64 signed __int64
#define U64 unsigned __int64
#define INF 32001
#define MATE 32000
#define MAX_PLY 64
#define NAME "Monkey"
#define VERSION "2026-01-26"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
#define get_move_source(move) (move & 0x3f)
#define get_move_target(move) ((move & 0xfc0) >> 6)
#define get_move_piece(move) ((move & 0xf000) >> 12)
#define get_move_promoted(move) ((move & 0xf0000) >> 16)
#define get_move_capture(move) (move & 0x100000)
#define get_move_double(move) (move & 0x200000)
#define get_move_enpassant(move) (move & 0x400000)
#define get_move_castling(move) (move & 0x800000)
#define no_hash_entry 100000
#define encode_move(source, target, piece, promoted, capture, doublem, enpassant, castling) \
    (source) |          \
    (target << 6) |     \
    (piece << 12) |     \
    (promoted << 16) |  \
    (capture << 20) |   \
    (doublem << 21) |   \
    (enpassant << 22) | \
    (castling << 23)    

enum {
	a8, b8, c8, d8, e8, f8, g8, h8,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a1, b1, c1, d1, e1, f1, g1, h1,
	no_sq
};
enum { WP, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK, PIECE_NB };
enum { white, black, both };
enum { all_moves, only_captures };
enum Bound { LOWER, UPPER, EXACT };
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum Castle { CWK = 1, CWQ = 2, CBK = 4, CBQ = 8 };
enum { oGame, eGame };

typedef struct {
	U64 hash;
	int depth;
	int flag;
	int score;
} TTEntry;

typedef struct {
	U64 bitboards[12];
	U64 occupancies[3];
	int side;
	int enpassant;
	int castle;
	U64 hash;
} Position;

typedef struct {
	int moves[256];
	int count;
} MoveList;

int killer_moves[2][MAX_PLY];
int history_moves[12][64];
int pv_length[MAX_PLY];
int pv_table[MAX_PLY][MAX_PLY];
int follow_pv, score_pv;
int hash_entries = 0;
U64 piece_keys[12][64];
U64 enpassant_keys[64];
U64 castle_keys[16];
U64 side_key;

TTEntry* hash_table = NULL;
const int castling_rights[64] = {
	 7, 15, 15, 15,  3, 15, 15, 11,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	13, 15, 15, 15, 12, 15, 15, 14
};
const int materialValue[2][12] =
{
	82, 337, 365, 477, 1025, 12000, -82, -337, -365, -477, -1025, -12000,
	94, 281, 297, 512,  936, 12000, -94, -281, -297, -512,  -936, -12000
};

const int phaseValue[12] = { 0, 1, 1, 2, 4, 0, 0, 1, 1, 2, 4, 0 };
int mg_pawn_table[64] = {
	  0,   0,   0,   0,   0,   0,  0,   0,
	 98, 134,  61,  95,  68, 126, 34, -11,
	 -6,   7,  26,  31,  65,  56, 25, -20,
	-14,  13,   6,  21,  23,  12, 17, -23,
	-27,  -2,  -5,  12,  17,   6, 10, -25,
	-26,  -4,  -4, -10,   3,   3, 33, -12,
	-35,  -1, -20, -23, -15,  24, 38, -22,
	  0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
	  0,   0,   0,   0,   0,   0,   0,   0,
	178, 173, 158, 134, 147, 132, 165, 187,
	 94, 100,  85,  67,  56,  53,  82,  84,
	 32,  24,  13,   5,  -2,   4,  17,  17,
	 13,   9,  -3,  -7,  -7,  -8,   3,  -1,
	  4,   7,  -6,   1,   0,  -5,  -1,  -8,
	 13,   8,   8,  10,  13,   0,   2,  -7,
	  0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
	-167, -89, -34, -49,  61, -97, -15, -107,
	 -73, -41,  72,  36,  23,  62,   7,  -17,
	 -47,  60,  37,  65,  84, 129,  73,   44,
	  -9,  17,  19,  53,  37,  69,  18,   22,
	 -13,   4,  16,  13,  28,  19,  21,   -8,
	 -23,  -9,  12,  10,  19,  17,  25,  -16,
	 -29, -53, -12,  -3,  -1,  18, -14,  -19,
	-105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
	-58, -38, -13, -28, -31, -27, -63, -99,
	-25,  -8, -25,  -2,  -9, -25, -24, -52,
	-24, -20,  10,   9,  -1,  -9, -19, -41,
	-17,   3,  22,  22,  22,  11,   8, -18,
	-18,  -6,  16,  25,  16,  17,   4, -18,
	-23,  -3,  -1,  15,  10,  -3, -20, -22,
	-42, -20, -10,  -5,  -2, -20, -23, -44,
	-29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
	-29,   4, -82, -37, -25, -42,   7,  -8,
	-26,  16, -18, -13,  30,  59,  18, -47,
	-16,  37,  43,  40,  35,  50,  37,  -2,
	 -4,   5,  19,  50,  37,  37,   7,  -2,
	 -6,  13,  13,  26,  34,  12,  10,   4,
	  0,  15,  15,  15,  14,  27,  18,  10,
	  4,  15,  16,   0,   7,  21,  33,   1,
	-33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
	-14, -21, -11,  -8, -7,  -9, -17, -24,
	 -8,  -4,   7, -12, -3, -13,  -4, -14,
	  2,  -8,   0,  -1, -2,   6,   0,   4,
	 -3,   9,  12,   9, 14,  10,   3,   2,
	 -6,   3,  13,  19,  7,  10,  -3,  -9,
	-12,  -3,   8,  10, 13,   3,  -7, -15,
	-14, -18,  -7,  -1,  4,  -9, -15, -27,
	-23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
	 32,  42,  32,  51, 63,  9,  31,  43,
	 27,  32,  58,  62, 80, 67,  26,  44,
	 -5,  19,  26,  36, 17, 45,  61,  16,
	-24, -11,   7,  26, 24, 35,  -8, -20,
	-36, -26, -12,  -1,  9, -7,   6, -23,
	-45, -25, -16, -17,  3,  0,  -5, -33,
	-44, -16, -20,  -9, -1, 11,  -6, -71,
	-19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
	13, 10, 18, 15, 12,  12,   8,   5,
	11, 13, 13, 11, -3,   3,   8,   3,
	 7,  7,  7,  5,  4,  -3,  -5,  -3,
	 4,  3, 13,  1,  2,   1,  -1,   2,
	 3,  5,  8,  4, -5,  -6,  -8, -11,
	-4,  0, -5, -1, -7, -12,  -8, -16,
	-6, -6,  0,  2, -9,  -9, -11,  -3,
	-9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
	-28,   0,  29,  12,  59,  44,  43,  45,
	-24, -39,  -5,   1, -16,  57,  28,  54,
	-13, -17,   7,   8,  29,  56,  47,  57,
	-27, -27, -16, -16,  -1,  17,  -2,   1,
	 -9, -26,  -9, -10,  -2,  -4,   3,  -3,
	-14,   2, -11,  -2,  -5,   2,  14,   5,
	-35,  -8,  11,   2,   8,  15,  -3,   1,
	 -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
	 -9,  22,  22,  27,  27,  19,  10,  20,
	-17,  20,  32,  41,  58,  25,  30,   0,
	-20,   6,   9,  49,  47,  35,  19,   9,
	  3,  22,  24,  45,  57,  40,  57,  36,
	-18,  28,  19,  47,  31,  34,  39,  23,
	-16, -27,  15,   6,   9,  17,  10,   5,
	-22, -23, -30, -16, -16, -23, -36, -32,
	-33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
	-65,  23,  16, -15, -56, -34,   2,  13,
	 29,  -1, -20,  -7,  -8,  -4, -38, -29,
	 -9,  24,   2, -16, -20,   6,  22, -22,
	-17, -20, -12, -27, -30, -25, -14, -36,
	-49,  -1, -27, -39, -46, -44, -33, -51,
	-14, -14, -22, -46, -44, -30, -15, -27,
	  1,   7,  -8, -64, -43, -16,   9,   8,
	-15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
	-74, -35, -18, -18, -11,  15,   4, -17,
	-12,  17,  14,  17,  17,  38,  23,  11,
	 10,  17,  23,  15,  20,  45,  44,  13,
	 -8,  22,  24,  27,  26,  33,  26,   3,
	-18,  -4,  21,  24,  27,  23,   9, -11,
	-19,  -3,  11,  21,  23,  16,   7,  -9,
	-27, -11,   4,  13,  14,   4,  -5, -17,
	-53, -34, -21, -11, -28, -14, -24, -43
};

int* mg_table[6] = {
	mg_pawn_table,
	mg_knight_table,
	mg_bishop_table,
	mg_rook_table,
	mg_queen_table,
	mg_king_table
};

int* eg_table[6] = {
	eg_pawn_table,
	eg_knight_table,
	eg_bishop_table,
	eg_rook_table,
	eg_queen_table,
	eg_king_table
};

int** positional_score[2] = {
	mg_table,
	eg_table
};

const int mirror_score[64] = {
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8
};
U64 file_masks[64];
U64 rank_masks[64];
U64 isolated_masks[64];
U64 white_passed_masks[64];
U64 black_passed_masks[64];
const int get_rank[64] = {
	7, 7, 7, 7, 7, 7, 7, 7,
	6, 6, 6, 6, 6, 6, 6, 6,
	5, 5, 5, 5, 5, 5, 5, 5,
	4, 4, 4, 4, 4, 4, 4, 4,
	3, 3, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0
};
const int double_pawn_penalty_opening = -5;
const int double_pawn_penalty_endgame = -10;
const int isolated_pawn_penalty_opening = -5;
const int isolated_pawn_penalty_endgame = -10;
char ascii_pieces[13] = "ANBRQKanbrqk ";
int char_pieces[] = {
	['P'] = WP,
	['N'] = WN,
	['B'] = WB,
	['R'] = WR,
	['Q'] = WQ,
	['K'] = WK,
	['p'] = BP,
	['n'] = BN,
	['b'] = BB,
	['r'] = BR,
	['q'] = BQ,
	['k'] = BK
};
char promoted_pieces[] = {
	[WQ] = 'q',
	[WR] = 'r',
	[WB] = 'b',
	[WN] = 'n',
	[BQ] = 'q',
	[BR] = 'r',
	[BB] = 'b',
	[BN] = 'n'
};

U64 repetition_table[1000];
int repetition_index;
typedef struct {
	BOOL post;
	BOOL stop;
	int depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodesLimit;
	U64 nodes;
} SearchInfo;

SearchInfo info;
int hash_min = 1;
int hash_def = 64;
int hash_max = 1000;
const int piece_values[13] = { 0, 100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500,600 };
const U64 not_a_file = 18374403900871474942ULL;
const U64 not_h_file = 9187201950435737471ULL;
const U64 not_hg_file = 4557430888798830399ULL;
const U64 not_ab_file = 18229723555195321596ULL;
const int bishop_relevant_bits[64] = {
	6, 5, 5, 5, 5, 5, 5, 6,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 5, 5, 5, 5, 5, 5, 6
};
const int rook_relevant_bits[64] = {
	12, 11, 11, 11, 11, 11, 11, 12,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	12, 11, 11, 11, 11, 11, 11, 12
};
U64 rook_magic_numbers[64] = {
	0x8a80104000800020ULL, 0x0140002000100040ULL, 0x02801880a0017001ULL, 0x0100081001000420ULL,
	0x0200020010080420ULL, 0x03001c0002010008ULL, 0x8480008002000100ULL, 0x2080088004402900ULL,
	0x0000800098204000ULL, 0x2024401000200040ULL, 0x0100802000801000ULL, 0x0120800800801000ULL,
	0x0208808088000400ULL, 0x0002802200800400ULL, 0x2200800100020080ULL, 0x0801000060821100ULL,
	0x0080044006422000ULL, 0x0100808020004000ULL, 0x12108a0010204200ULL, 0x0140848010000802ULL,
	0x0481828014002800ULL, 0x8094004002004100ULL, 0x4010040010010802ULL, 0x0000020008806104ULL,
	0x0100400080208000ULL, 0x2040002120081000ULL, 0x0021200680100081ULL, 0x0020100080080080ULL,
	0x0002000a00200410ULL, 0x0000020080800400ULL, 0x0080088400100102ULL, 0x0080004600042881ULL,
	0x4040008040800020ULL, 0x0440003000200801ULL, 0x0004200011004500ULL, 0x0188020010100100ULL,
	0x0014800401802800ULL, 0x2080040080800200ULL, 0x0124080204001001ULL, 0x0200046502000484ULL,
	0x0480400080088020ULL, 0x1000422010034000ULL, 0x0030200100110040ULL, 0x0000100021010009ULL,
	0x2002080100110004ULL, 0x0202008004008002ULL, 0x0020020004010100ULL, 0x2048440040820001ULL,
	0x0101002200408200ULL, 0x0040802000401080ULL, 0x4008142004410100ULL, 0x02060820c0120200ULL,
	0x0001001004080100ULL, 0x020c020080040080ULL, 0x2935610830022400ULL, 0x0044440041009200ULL,
	0x0280001040802101ULL, 0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
	0x00020030a0244872ULL, 0x0012001008414402ULL, 0x02006104900a0804ULL, 0x0001004081002402ULL
};
U64 bishop_magic_numbers[64] = {
	0x0040040844404084ULL, 0x002004208a004208ULL, 0x0010190041080202ULL, 0x0108060845042010ULL,
	0x0581104180800210ULL, 0x2112080446200010ULL, 0x1080820820060210ULL, 0x03c0808410220200ULL,
	0x0004050404440404ULL, 0x0000021001420088ULL, 0x24d0080801082102ULL, 0x0001020a0a020400ULL,
	0x0000040308200402ULL, 0x0004011002100800ULL, 0x0401484104104005ULL, 0x0801010402020200ULL,
	0x00400210c3880100ULL, 0x0404022024108200ULL, 0x0810018200204102ULL, 0x0004002801a02003ULL,
	0x0085040820080400ULL, 0x810102c808880400ULL, 0x000e900410884800ULL, 0x8002020480840102ULL,
	0x0220200865090201ULL, 0x2010100a02021202ULL, 0x0152048408022401ULL, 0x0020080002081110ULL,
	0x4001001021004000ULL, 0x800040400a011002ULL, 0x00e4004081011002ULL, 0x001c004001012080ULL,
	0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
	0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
	0x0209188240001000ULL, 0x400408a884001800ULL, 0x00110400a6080400ULL, 0x1840060a44020800ULL,
	0x0090080104000041ULL, 0x0201011000808101ULL, 0x1a2208080504f080ULL, 0x8012020600211212ULL,
	0x0500861011240000ULL, 0x0180806108200800ULL, 0x4000020e01040044ULL, 0x300000261044000aULL,
	0x0802241102020002ULL, 0x0020906061210001ULL, 0x5a84841004010310ULL, 0x0004010801011c04ULL,
	0x000a010109502200ULL, 0x0000004a02012000ULL, 0x500201010098b028ULL, 0x8040002811040900ULL,
	0x0028000010020204ULL, 0x06000020202d0240ULL, 0x8918844842082200ULL, 0x4010011029020020ULL
};
U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];
U64 bishop_masks[64];
U64 rook_masks[64];
U64 bishop_attacks[64][512];
U64 rook_attacks[64][4096];

void UciCommand(Position* pos, char* input);

inline static int MVV_LVA(int attacker, int victim) { return piece_values[victim] - piece_values[attacker] / 100; }
static inline U64 GetTimeMs() { return GetTickCount64(); }
static inline U64 GetRandom64() { return (U64)rand() ^ ((U64)rand() << 15) ^ ((U64)rand() << 30) ^ ((U64)rand() << 45) ^ ((U64)rand() << 60); }
static inline U64 LSB(const U64 bb) { return _tzcnt_u64(bb); }
static inline int CountBits(const U64 bb) { return _mm_popcnt_u64(bb); }

static int InputAvailable(void) {
	static int init = 0, pipe;
	static HANDLE inh;
	DWORD dw;
	if (!init) {
		init = 1;
		inh = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(inh, &dw);
		if (!pipe) {
			SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(inh);
		}
	}
	if (pipe) {
		if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
			return 1;
		return dw > 0;
	}
	else {
		GetNumberOfConsoleInputEvents(inh, &dw);
		return dw > 1;
	}
}

static int InputWaiting() {
#if defined(_WIN32) || defined(_WIN64)
	static int init = 0, pipe;
	static HANDLE inh;
	DWORD dw;
	if (!init) {
		init = 1;
		inh = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(inh, &dw);
		if (!pipe) {
			SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(inh);
		}
	}
	if (pipe) {
		if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
			return 1;
		return dw > 0;
	}
	else {
		GetNumberOfConsoleInputEvents(inh, &dw);
		return dw > 1;
	}
#else
	fd_set readfds;
	struct timeval tv;
	FD_ZERO(&readfds);
	FD_SET(fileno(stdin), &readfds);
	tv.tv_sec = 0; tv.tv_usec = 0;
	select(16, &readfds, 0, 0, &tv);
	return (FD_ISSET(fileno(stdin), &readfds));
#endif
}

static int CheckUp(Position* pos) {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = TRUE;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = TRUE;
		if (InputAvailable()) {
			char line[4000];
			fgets(line, sizeof(line), stdin);
			UciCommand(pos, line);
		}
	}
	return info.stop;
}

static void ResetInfo() {
	info.post = TRUE;
	info.stop = FALSE;
	info.nodes = 0;
	info.depthLimit = MAX_PLY;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
}

static void InitRandomKeys() {
	for (int piece = WP; piece <= BK; piece++) {
		for (int square = 0; square < 64; square++)
			piece_keys[piece][square] = GetRandom64();
	}
	for (int square = 0; square < 64; square++)
		enpassant_keys[square] = GetRandom64();
	for (int index = 0; index < 16; index++)
		castle_keys[index] = GetRandom64();
	side_key = GetRandom64();
}

static U64 GenerateHashKey(Position* pos) {
	U64 final_key = 0ULL;
	U64 bitboard;
	for (int piece = WP; piece <= BK; piece++)
	{
		bitboard = pos->bitboards[piece];
		while (bitboard)
		{
			int square = LSB(bitboard);
			final_key ^= piece_keys[piece][square];
			pop_bit(bitboard, square);
		}
	}
	if (pos->enpassant != no_sq)
		final_key ^= enpassant_keys[pos->enpassant];
	final_key ^= castle_keys[pos->castle];
	if (pos->side == black) final_key ^= side_key;
	return final_key;
}

static int PieceOn(Position* pos, int sq) {
	for (int piece = WP; piece <= BK; piece++)
		if (get_bit(pos->bitboards[piece], sq))
			return piece;
	return PIECE_NB;
}

static char* SquareToUci(int square) {
	static char str[3] = { 0 };
	str[0] = 'a' + (square % 8);
	str[1] = '1' + (7 - square / 8);
	return str;
}

static char* MoveToUci(int move) {
	static char str[6] = { 0 };
	int from = get_move_source(move);
	int to = get_move_target(move);
	int promo = get_move_promoted(move);
	str[0] = 'a' + (from % 8);
	str[1] = '1' + (7 - from / 8);
	str[2] = 'a' + (to % 8);
	str[3] = '1' + (7 - to / 8);
	str[4] = promoted_pieces[promo];
	return str;
}

static void PrintBitboard(U64 bb) {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	printf(t);
	for (int BR = 7; BR >= 0; BR--) {
		printf(s);
		printf(" %d |", BR + 1);
		for (int f = 0; f < 8; f++) {
			int sq = (7 - BR) * 8 + f;
			printf(" %c |", bb & 1ull << sq ? 'x' : ' ');
		}
		printf(" %d \n", BR + 1);
	}
	printf(s);
	printf(t);
}

static void PrintBoard(Position* pos) {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	printf(t);
	for (int BR = 7; BR >= 0; BR--) {
		printf(s);
		printf(" %d |", BR + 1);
		for (int f = 0; f < 8; f++) {
			int sq = (7 - BR) * 8 + f;
			int piece = PieceOn(pos, sq);
			printf(" %c |", ascii_pieces[piece]);
		}
		printf(" %d \n", BR + 1);
	}
	printf(s);
	printf(t);
	printf("     Side: %s\n", pos->side ? "black" : "white");
	printf("     Enpassant: %s\n", (pos->enpassant != no_sq) ? SquareToUci(pos->enpassant) : "no");
	printf("     Castling: %c%c%c%c\n",
		(pos->castle & CWK) ? 'K' : '-',
		(pos->castle & CWQ) ? 'Q' : '-',
		(pos->castle & CBK) ? 'k' : '-',
		(pos->castle & CBQ) ? 'q' : '-');
	printf("     Hash key: %llx\n", pos->hash);
	printf("     Score: %d\n", Evaluate(pos));
}

static void ResetPosition(Position* pos) {
	memset(pos->bitboards, 0ULL, sizeof(pos->bitboards));
	memset(pos->occupancies, 0ULL, sizeof(pos->occupancies));
	pos->side = 0;
	pos->enpassant = no_sq;
	pos->castle = 0;
	repetition_index = 0;
	memset(repetition_table, 0ULL, sizeof(repetition_table));
}

static void SetFen(Position* pos, char* fen) {
	ResetPosition(pos);
	for (int rank = 0; rank < 8; rank++) {
		for (int file = 0; file < 8; file++) {
			int square = rank * 8 + file;
			if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
				int piece = char_pieces[*fen];
				set_bit(pos->bitboards[piece], square);
				fen++;
			}
			if (*fen >= '0' && *fen <= '9')
			{
				int offset = *fen - '0';
				int piece = -1;
				for (int bb_piece = WP; bb_piece <= BK; bb_piece++)
				{
					if (get_bit(pos->bitboards[bb_piece], square))
						piece = bb_piece;
				}
				if (piece == -1)
					file--;
				file += offset;
				fen++;
			}
			if (*fen == '/')
				fen++;
		}
	}
	fen++;
	(*fen == 'w') ? (pos->side = white) : (pos->side = black);
	fen += 2;
	while (*fen != ' ')
	{
		switch (*fen)
		{
		case 'K': pos->castle |= CWK; break;
		case 'Q': pos->castle |= CWQ; break;
		case 'k': pos->castle |= CBK; break;
		case 'q': pos->castle |= CBQ; break;
		case '-': break;
		}
		fen++;
	}
	fen++;
	if (*fen != '-')
	{
		int file = fen[0] - 'a';
		int rank = 8 - (fen[1] - '0');
		pos->enpassant = rank * 8 + file;
	}
	else
		pos->enpassant = no_sq;
	for (int piece = WP; piece <= WK; piece++)
		pos->occupancies[white] |= pos->bitboards[piece];
	for (int piece = BP; piece <= BK; piece++)
		pos->occupancies[black] |= pos->bitboards[piece];
	pos->occupancies[both] |= pos->occupancies[white];
	pos->occupancies[both] |= pos->occupancies[black];
	pos->hash = GenerateHashKey(pos);
}

U64 mask_pawn_attacks(int side, int square)
{
	U64 attacks = 0ULL;
	U64 bitboard = 0ULL;
	set_bit(bitboard, square);
	if (!side)
	{
		if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
		if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
	}
	else
	{
		if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
		if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
	}
	return attacks;
}

U64 mask_knight_attacks(int square)
{
	U64 attacks = 0ULL;
	U64 bitboard = 0ULL;
	set_bit(bitboard, square);
	if ((bitboard >> 17) & not_h_file) attacks |= (bitboard >> 17);
	if ((bitboard >> 15) & not_a_file) attacks |= (bitboard >> 15);
	if ((bitboard >> 10) & not_hg_file) attacks |= (bitboard >> 10);
	if ((bitboard >> 6) & not_ab_file) attacks |= (bitboard >> 6);
	if ((bitboard << 17) & not_a_file) attacks |= (bitboard << 17);
	if ((bitboard << 15) & not_h_file) attacks |= (bitboard << 15);
	if ((bitboard << 10) & not_ab_file) attacks |= (bitboard << 10);
	if ((bitboard << 6) & not_hg_file) attacks |= (bitboard << 6);
	return attacks;
}

U64 mask_king_attacks(int square)
{
	U64 attacks = 0ULL;
	U64 bitboard = 0ULL;
	set_bit(bitboard, square);
	if (bitboard >> 8) attacks |= (bitboard >> 8);
	if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
	if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
	if ((bitboard >> 1) & not_h_file) attacks |= (bitboard >> 1);
	if (bitboard << 8) attacks |= (bitboard << 8);
	if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
	if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
	if ((bitboard << 1) & not_a_file) attacks |= (bitboard << 1);
	return attacks;
}

U64 mask_bishop_attacks(int square) {
	U64 attacks = 0ULL;
	int BR, f;
	int tr = square / 8;
	int tf = square % 8;
	for (BR = tr + 1, f = tf + 1; BR <= 6 && f <= 6; BR++, f++) attacks |= (1ULL << (BR * 8 + f));
	for (BR = tr - 1, f = tf + 1; BR >= 1 && f <= 6; BR--, f++) attacks |= (1ULL << (BR * 8 + f));
	for (BR = tr + 1, f = tf - 1; BR <= 6 && f >= 1; BR++, f--) attacks |= (1ULL << (BR * 8 + f));
	for (BR = tr - 1, f = tf - 1; BR >= 1 && f >= 1; BR--, f--) attacks |= (1ULL << (BR * 8 + f));
	return attacks;
}

U64 mask_rook_attacks(int square) {
	U64 attacks = 0ULL;
	int BR, f;
	int tr = square / 8;
	int tf = square % 8;
	for (BR = tr + 1; BR <= 6; BR++) attacks |= (1ULL << (BR * 8 + tf));
	for (BR = tr - 1; BR >= 1; BR--) attacks |= (1ULL << (BR * 8 + tf));
	for (f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
	for (f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));
	return attacks;
}

U64 bishop_attacks_on_the_fly(int square, U64 block) {
	U64 attacks = 0ULL;
	int BR, f;
	int tr = square / 8;
	int tf = square % 8;
	for (BR = tr + 1, f = tf + 1; BR <= 7 && f <= 7; BR++, f++)
	{
		attacks |= (1ULL << (BR * 8 + f));
		if ((1ULL << (BR * 8 + f)) & block) break;
	}
	for (BR = tr - 1, f = tf + 1; BR >= 0 && f <= 7; BR--, f++)
	{
		attacks |= (1ULL << (BR * 8 + f));
		if ((1ULL << (BR * 8 + f)) & block) break;
	}
	for (BR = tr + 1, f = tf - 1; BR <= 7 && f >= 0; BR++, f--)
	{
		attacks |= (1ULL << (BR * 8 + f));
		if ((1ULL << (BR * 8 + f)) & block) break;
	}
	for (BR = tr - 1, f = tf - 1; BR >= 0 && f >= 0; BR--, f--)
	{
		attacks |= (1ULL << (BR * 8 + f));
		if ((1ULL << (BR * 8 + f)) & block) break;
	}
	return attacks;
}

U64 rook_attacks_on_the_fly(int square, U64 block)
{
	U64 attacks = 0ULL;
	int BR, f;
	int tr = square / 8;
	int tf = square % 8;
	for (BR = tr + 1; BR <= 7; BR++)
	{
		attacks |= (1ULL << (BR * 8 + tf));
		if ((1ULL << (BR * 8 + tf)) & block) break;
	}
	for (BR = tr - 1; BR >= 0; BR--)
	{
		attacks |= (1ULL << (BR * 8 + tf));
		if ((1ULL << (BR * 8 + tf)) & block) break;
	}
	for (f = tf + 1; f <= 7; f++)
	{
		attacks |= (1ULL << (tr * 8 + f));
		if ((1ULL << (tr * 8 + f)) & block) break;
	}
	for (f = tf - 1; f >= 0; f--)
	{
		attacks |= (1ULL << (tr * 8 + f));
		if ((1ULL << (tr * 8 + f)) & block) break;
	}
	return attacks;
}

void init_leapers_attacks() {
	for (int square = 0; square < 64; square++) {
		pawn_attacks[white][square] = mask_pawn_attacks(white, square);
		pawn_attacks[black][square] = mask_pawn_attacks(black, square);
		knight_attacks[square] = mask_knight_attacks(square);
		king_attacks[square] = mask_king_attacks(square);
	}
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
	U64 occupancy = 0ULL;
	for (int count = 0; count < bits_in_mask; count++)
	{
		int square = LSB(attack_mask);
		pop_bit(attack_mask, square);
		if (index & (1 << count))
			occupancy |= (1ULL << square);
	}
	return occupancy;
}

void init_sliders_attacks(int pt) {
	for (int square = 0; square < 64; square++)
	{
		bishop_masks[square] = mask_bishop_attacks(square);
		rook_masks[square] = mask_rook_attacks(square);
		U64 attack_mask = pt == BISHOP ? bishop_masks[square] : rook_masks[square];
		int relevant_bits_count = CountBits(attack_mask);
		int occupancy_indicies = (1 << relevant_bits_count);
		for (int index = 0; index < occupancy_indicies; index++) {
			if (pt == BISHOP) {
				U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
				int magic_index = (occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits[square]);
				bishop_attacks[square][magic_index] = bishop_attacks_on_the_fly(square, occupancy);
			}
			else {
				U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
				int magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits[square]);
				rook_attacks[square][magic_index] = rook_attacks_on_the_fly(square, occupancy);
			}
		}
	}
}

static inline U64 get_bishop_attacks(int square, U64 occupancy)
{
	occupancy &= bishop_masks[square];
	occupancy *= bishop_magic_numbers[square];
	occupancy >>= 64 - bishop_relevant_bits[square];
	return bishop_attacks[square][occupancy];
}
static inline U64 get_rook_attacks(int square, U64 occupancy)
{
	occupancy &= rook_masks[square];
	occupancy *= rook_magic_numbers[square];
	occupancy >>= 64 - rook_relevant_bits[square];
	return rook_attacks[square][occupancy];
}
static inline U64 get_queen_attacks(int square, U64 occupancy)
{
	U64 queen_attacks = 0ULL;
	U64 bishop_occupancy = occupancy;
	U64 rook_occupancy = occupancy;
	bishop_occupancy &= bishop_masks[square];
	bishop_occupancy *= bishop_magic_numbers[square];
	bishop_occupancy >>= 64 - bishop_relevant_bits[square];
	queen_attacks = bishop_attacks[square][bishop_occupancy];
	rook_occupancy &= rook_masks[square];
	rook_occupancy *= rook_magic_numbers[square];
	rook_occupancy >>= 64 - rook_relevant_bits[square];
	queen_attacks |= rook_attacks[square][rook_occupancy];
	return queen_attacks;
}
static inline int is_square_attacked(Position* pos, int square, int bySide) {
	if ((bySide == white) && (pawn_attacks[black][square] & pos->bitboards[WP])) return 1;
	if ((bySide == black) && (pawn_attacks[white][square] & pos->bitboards[BP])) return 1;
	if (knight_attacks[square] & ((bySide == white) ? pos->bitboards[WN] : pos->bitboards[BN])) return 1;
	if (get_bishop_attacks(square, pos->occupancies[both]) & ((bySide == white) ? pos->bitboards[WB] : pos->bitboards[BB])) return 1;
	if (get_rook_attacks(square, pos->occupancies[both]) & ((bySide == white) ? pos->bitboards[WR] : pos->bitboards[BR])) return 1;
	if (get_queen_attacks(square, pos->occupancies[both]) & ((bySide == white) ? pos->bitboards[WQ] : pos->bitboards[BQ])) return 1;
	if (king_attacks[square] & ((bySide == white) ? pos->bitboards[WK] : pos->bitboards[BK])) return 1;
	return 0;
}

static inline void add_move(MoveList* move_list, int move) {
	move_list->moves[move_list->count] = move;
	move_list->count++;
}

static inline int MakeMove(Position* pos, int move, int move_flag) {
	if (move_flag == all_moves) {
		int source_square = get_move_source(move);
		int target_square = get_move_target(move);
		int piece = get_move_piece(move);
		int promoted_piece = get_move_promoted(move);
		int capture = get_move_capture(move);
		int double_push = get_move_double(move);
		int enpass = get_move_enpassant(move);
		int castling = get_move_castling(move);
		pop_bit(pos->bitboards[piece], source_square);
		set_bit(pos->bitboards[piece], target_square);
		pos->hash ^= piece_keys[piece][source_square];
		pos->hash ^= piece_keys[piece][target_square];
		if (capture)
		{
			int start_piece, end_piece;
			if (pos->side == white)
			{
				start_piece = BP;
				end_piece = BK;
			}
			else
			{
				start_piece = WP;
				end_piece = WK;
			}
			for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
			{
				if (get_bit(pos->bitboards[bb_piece], target_square))
				{
					pop_bit(pos->bitboards[bb_piece], target_square);
					pos->hash ^= piece_keys[bb_piece][target_square];
					break;
				}
			}
		}
		if (promoted_piece)
		{
			if (pos->side == white)
			{
				pop_bit(pos->bitboards[WP], target_square);
				pos->hash ^= piece_keys[WP][target_square];
			}
			else
			{
				pop_bit(pos->bitboards[BP], target_square);
				pos->hash ^= piece_keys[BP][target_square];
			}
			set_bit(pos->bitboards[promoted_piece], target_square);
			pos->hash ^= piece_keys[promoted_piece][target_square];
		}
		if (enpass)
		{
			(pos->side == white) ? pop_bit(pos->bitboards[BP], target_square + 8) :
				pop_bit(pos->bitboards[WP], target_square - 8);
			if (pos->side == white)
			{
				pop_bit(pos->bitboards[BP], target_square + 8);
				pos->hash ^= piece_keys[BP][target_square + 8];
			}
			else
			{
				pop_bit(pos->bitboards[WP], target_square - 8);
				pos->hash ^= piece_keys[WP][target_square - 8];
			}
		}
		if (pos->enpassant != no_sq) pos->hash ^= enpassant_keys[pos->enpassant];
		pos->enpassant = no_sq;
		if (double_push)
		{
			if (pos->side == white)
			{
				pos->enpassant = target_square + 8;
				pos->hash ^= enpassant_keys[target_square + 8];
			}
			else
			{
				pos->enpassant = target_square - 8;
				pos->hash ^= enpassant_keys[target_square - 8];
			}
		}
		if (castling)
		{
			switch (target_square)
			{
			case (g1):
				pop_bit(pos->bitboards[WR], h1);
				set_bit(pos->bitboards[WR], f1);
				pos->hash ^= piece_keys[WR][h1];
				pos->hash ^= piece_keys[WR][f1];
				break;
			case (c1):
				pop_bit(pos->bitboards[WR], a1);
				set_bit(pos->bitboards[WR], d1);
				pos->hash ^= piece_keys[WR][a1];
				pos->hash ^= piece_keys[WR][d1];
				break;
			case (g8):
				pop_bit(pos->bitboards[BR], h8);
				set_bit(pos->bitboards[BR], f8);
				pos->hash ^= piece_keys[BR][h8];
				pos->hash ^= piece_keys[BR][f8];
				break;
			case (c8):
				pop_bit(pos->bitboards[BR], a8);
				set_bit(pos->bitboards[BR], d8);
				pos->hash ^= piece_keys[BR][a8];
				pos->hash ^= piece_keys[BR][d8];
				break;
			}
		}
		pos->hash ^= castle_keys[pos->castle];
		pos->castle &= castling_rights[source_square];
		pos->castle &= castling_rights[target_square];
		pos->hash ^= castle_keys[pos->castle];
		memset(pos->occupancies, 0ULL, 24);
		for (int bb_piece = WP; bb_piece <= WK; bb_piece++)
			pos->occupancies[white] |= pos->bitboards[bb_piece];
		for (int bb_piece = BP; bb_piece <= BK; bb_piece++)
			pos->occupancies[black] |= pos->bitboards[bb_piece];
		pos->occupancies[both] |= pos->occupancies[white];
		pos->occupancies[both] |= pos->occupancies[black];
		pos->side ^= 1;
		pos->hash ^= side_key;
		return !is_square_attacked(pos, pos->side == white ? LSB(pos->bitboards[BK]) : LSB(pos->bitboards[WK]), pos->side);
	}
	else if (get_move_capture(move))
		return MakeMove(pos, move, all_moves);
	else
		return 0;
}

static inline void GenerateMoves(Position* pos, MoveList* move_list) {
	move_list->count = 0;
	int source_square, target_square;
	U64 bitboard, attacks;
	for (int piece = WP; piece <= BK; piece++)
	{
		bitboard = pos->bitboards[piece];
		if (pos->side == white)
		{
			if (piece == WP)
			{
				while (bitboard)
				{
					source_square = LSB(bitboard);
					target_square = source_square - 8;
					if (!(target_square < a8) && !get_bit(pos->occupancies[both], target_square))
					{
						if (source_square >= a7 && source_square <= h7)
						{
							add_move(move_list, encode_move(source_square, target_square, piece, WQ, 0, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, WR, 0, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, WB, 0, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, WN, 0, 0, 0, 0));
						}
						else
						{
							add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
							if ((source_square >= a2 && source_square <= h2) && !get_bit(pos->occupancies[both], target_square - 8))
								add_move(move_list, encode_move(source_square, target_square - 8, piece, 0, 0, 1, 0, 0));
						}
					}
					attacks = pawn_attacks[pos->side][source_square] & pos->occupancies[black];
					while (attacks)
					{
						target_square = LSB(attacks);
						if (source_square >= a7 && source_square <= h7)
						{
							add_move(move_list, encode_move(source_square, target_square, piece, WQ, 1, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, WR, 1, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, WB, 1, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, WN, 1, 0, 0, 0));
						}
						else
							add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
						pop_bit(attacks, target_square);
					}
					if (pos->enpassant != no_sq)
					{
						U64 enpassant_attacks = pawn_attacks[pos->side][source_square] & (1ULL << pos->enpassant);
						if (enpassant_attacks)
						{
							int target_enpassant = LSB(enpassant_attacks);
							add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
						}
					}
					pop_bit(bitboard, source_square);
				}
			}
			if (piece == WK)
			{
				if (pos->castle & CWK)
				{
					if (!get_bit(pos->occupancies[both], f1) && !get_bit(pos->occupancies[both], g1))
					{
						if (!is_square_attacked(pos, e1, black) && !is_square_attacked(pos, f1, black))
							add_move(move_list, encode_move(e1, g1, piece, 0, 0, 0, 0, 1));
					}
				}
				if (pos->castle & CWQ)
				{
					if (!get_bit(pos->occupancies[both], d1) && !get_bit(pos->occupancies[both], c1) && !get_bit(pos->occupancies[both], b1))
					{
						if (!is_square_attacked(pos, e1, black) && !is_square_attacked(pos, d1, black))
							add_move(move_list, encode_move(e1, c1, piece, 0, 0, 0, 0, 1));
					}
				}
			}
		}
		else
		{
			if (piece == BP)
			{
				while (bitboard)
				{
					source_square = LSB(bitboard);
					target_square = source_square + 8;
					if (!(target_square > h1) && !get_bit(pos->occupancies[both], target_square))
					{
						if (source_square >= a2 && source_square <= h2)
						{
							add_move(move_list, encode_move(source_square, target_square, piece, BQ, 0, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, BR, 0, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, BB, 0, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, BN, 0, 0, 0, 0));
						}
						else
						{
							add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
							if ((source_square >= a7 && source_square <= h7) && !get_bit(pos->occupancies[both], target_square + 8))
								add_move(move_list, encode_move(source_square, target_square + 8, piece, 0, 0, 1, 0, 0));
						}
					}
					attacks = pawn_attacks[pos->side][source_square] & pos->occupancies[white];
					while (attacks)
					{
						target_square = LSB(attacks);
						if (source_square >= a2 && source_square <= h2)
						{
							add_move(move_list, encode_move(source_square, target_square, piece, BQ, 1, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, BR, 1, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, BB, 1, 0, 0, 0));
							add_move(move_list, encode_move(source_square, target_square, piece, BN, 1, 0, 0, 0));
						}
						else
							add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
						pop_bit(attacks, target_square);
					}
					if (pos->enpassant != no_sq)
					{
						U64 enpassant_attacks = pawn_attacks[pos->side][source_square] & (1ULL << pos->enpassant);
						if (enpassant_attacks)
						{
							int target_enpassant = LSB(enpassant_attacks);
							add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
						}
					}
					pop_bit(bitboard, source_square);
				}
			}
			if (piece == BK)
			{
				if (pos->castle & CBK)
				{
					if (!get_bit(pos->occupancies[both], f8) && !get_bit(pos->occupancies[both], g8))
					{
						if (!is_square_attacked(pos, e8, white) && !is_square_attacked(pos, f8, white))
							add_move(move_list, encode_move(e8, g8, piece, 0, 0, 0, 0, 1));
					}
				}
				if (pos->castle & CBQ)
				{
					if (!get_bit(pos->occupancies[both], d8) && !get_bit(pos->occupancies[both], c8) && !get_bit(pos->occupancies[both], b8))
					{
						if (!is_square_attacked(pos, e8, white) && !is_square_attacked(pos, d8, white))
							add_move(move_list, encode_move(e8, c8, piece, 0, 0, 0, 0, 1));
					}
				}
			}
		}
		if ((pos->side == white) ? piece == WN : piece == BN)
		{
			while (bitboard)
			{
				source_square = LSB(bitboard);
				attacks = knight_attacks[source_square] & ((pos->side == white) ? ~pos->occupancies[white] : ~pos->occupancies[black]);
				while (attacks)
				{
					target_square = LSB(attacks);
					if (!get_bit(((pos->side == white) ? pos->occupancies[black] : pos->occupancies[white]), target_square))
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
					else
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
					pop_bit(attacks, target_square);
				}
				pop_bit(bitboard, source_square);
			}
		}
		if ((pos->side == white) ? piece == WB : piece == BB)
		{
			while (bitboard)
			{
				source_square = LSB(bitboard);
				attacks = get_bishop_attacks(source_square, pos->occupancies[both]) & ((pos->side == white) ? ~pos->occupancies[white] : ~pos->occupancies[black]);
				while (attacks)
				{
					target_square = LSB(attacks);
					if (!get_bit(((pos->side == white) ? pos->occupancies[black] : pos->occupancies[white]), target_square))
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
					else
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
					pop_bit(attacks, target_square);
				}
				pop_bit(bitboard, source_square);
			}
		}
		if ((pos->side == white) ? piece == WR : piece == BR)
		{
			while (bitboard)
			{
				source_square = LSB(bitboard);
				attacks = get_rook_attacks(source_square, pos->occupancies[both]) & ((pos->side == white) ? ~pos->occupancies[white] : ~pos->occupancies[black]);
				while (attacks)
				{
					target_square = LSB(attacks);
					if (!get_bit(((pos->side == white) ? pos->occupancies[black] : pos->occupancies[white]), target_square))
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
					else
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
					pop_bit(attacks, target_square);
				}
				pop_bit(bitboard, source_square);
			}
		}
		if ((pos->side == white) ? piece == WQ : piece == BQ)
		{
			while (bitboard)
			{
				source_square = LSB(bitboard);
				attacks = get_queen_attacks(source_square, pos->occupancies[both]) & ((pos->side == white) ? ~pos->occupancies[white] : ~pos->occupancies[black]);
				while (attacks)
				{
					target_square = LSB(attacks);
					if (!get_bit(((pos->side == white) ? pos->occupancies[black] : pos->occupancies[white]), target_square))
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
					else
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
					pop_bit(attacks, target_square);
				}
				pop_bit(bitboard, source_square);
			}
		}
		if ((pos->side == white) ? piece == WK : piece == BK)
		{
			while (bitboard)
			{
				source_square = LSB(bitboard);
				attacks = king_attacks[source_square] & ((pos->side == white) ? ~pos->occupancies[white] : ~pos->occupancies[black]);
				while (attacks)
				{
					target_square = LSB(attacks);
					if (!get_bit(((pos->side == white) ? pos->occupancies[black] : pos->occupancies[white]), target_square))
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
					else
						add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
					pop_bit(attacks, target_square);
				}
				pop_bit(bitboard, source_square);
			}
		}
	}
}
static inline void PerftDriver(Position* pos, int depth) {
	MoveList move_list[1];
	GenerateMoves(pos, move_list);
	for (int move_count = 0; move_count < move_list->count; move_count++) {
		Position npos = *pos;
		if (!MakeMove(&npos, move_list->moves[move_count], all_moves))
			continue;
		if (depth)
			PerftDriver(&npos, depth - 1);
		else
			info.nodes++;
	}
}
static int ShrinkNumber(U64 BN) {
	if (BN < 10000)
		return 0;
	if (BN < 10000000)
		return 1;
	if (BN < 10000000000)
		return 2;
	return 3;
}
void PrintPerformanceHeader()
{
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}
static void PrintSummary(U64 time, U64 nodes) {
	if (time < 1)
		time = 1;
	U64 nps = (nodes * 1000) / time;
	const char* units[] = { "", "k", "m", "g" };
	int sn = ShrinkNumber(nps);
	U64 BP = pow(10, sn * 3);
	printf("-----------------------------\n");
	printf("Time        : %llu\n", time);
	printf("Nodes       : %llu\n", nodes);
	printf("Nps         : %llu (%llu%s/s)\n", nps, nps / BP, units[sn]);
	printf("-----------------------------\n");
}
const int passed_pawn_bonus[8] = { 0, 10, 30, 50, 75, 100, 150, 200 };
const int semi_open_file_score = 10;
const int open_file_score = 15;
static const int bishop_unit = 4;
static const int queen_unit = 9;
static const int bishop_mobility_opening = 5;
static const int bishop_mobility_endgame = 5;
static const int queen_mobility_opening = 1;
static const int queen_mobility_endgame = 2;
const int king_shield_bonus = 5;
U64 set_file_rank_mask(int file_number, int rank_number)
{
	U64 mask = 0ULL;
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			if (file_number != -1)
			{
				if (file == file_number)
					mask |= set_bit(mask, square);
			}
			else if (rank_number != -1)
			{
				if (rank == rank_number)
					mask |= set_bit(mask, square);
			}
		}
	}
	return mask;
}
void InitEvaluationMasks()
{
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			file_masks[square] |= set_file_rank_mask(file, -1);
		}
	}
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			rank_masks[square] |= set_file_rank_mask(-1, rank);
		}
	}
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			isolated_masks[square] |= set_file_rank_mask(file - 1, -1);
			isolated_masks[square] |= set_file_rank_mask(file + 1, -1);
		}
	}
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			white_passed_masks[square] |= set_file_rank_mask(file - 1, -1);
			white_passed_masks[square] |= set_file_rank_mask(file, -1);
			white_passed_masks[square] |= set_file_rank_mask(file + 1, -1);
			for (int i = 0; i < (8 - rank); i++)
				white_passed_masks[square] &= ~rank_masks[(7 - i) * 8 + file];
		}
	}
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			black_passed_masks[square] |= set_file_rank_mask(file - 1, -1);
			black_passed_masks[square] |= set_file_rank_mask(file, -1);
			black_passed_masks[square] |= set_file_rank_mask(file + 1, -1);
			for (int i = 0; i < rank + 1; i++)
				black_passed_masks[square] &= ~rank_masks[i * 8 + file];
		}
	}
}

static inline int Evaluate(Position* pos) {
	int phase = 0, score = 0, score_opening = 0, score_endgame = 0;
	U64 bitboard;
	int double_pawns = 0;
	for (int piece = WP; piece <= BK; piece++) {
		bitboard = pos->bitboards[piece];
		while (bitboard)
		{
			int square = LSB(bitboard);
			phase += phaseValue[piece];
			score_opening += materialValue[oGame][piece];
			score_endgame += materialValue[eGame][piece];
			switch (piece)
			{
			case WP:
				score_opening += positional_score[oGame][PAWN][square];
				score_endgame += positional_score[eGame][PAWN][square];
				double_pawns = CountBits(pos->bitboards[WP] & file_masks[square]);
				if (double_pawns > 1)
				{
					score_opening += (double_pawns - 1) * double_pawn_penalty_opening;
					score_endgame += (double_pawns - 1) * double_pawn_penalty_endgame;
				}
				if ((pos->bitboards[WP] & isolated_masks[square]) == 0)
				{
					score_opening += isolated_pawn_penalty_opening;
					score_endgame += isolated_pawn_penalty_endgame;
				}
				if ((white_passed_masks[square] & pos->bitboards[BP]) == 0)
				{
					score_opening += passed_pawn_bonus[get_rank[square]];
					score_endgame += passed_pawn_bonus[get_rank[square]];
				}
				break;
			case WN:
				score_opening += positional_score[oGame][KNIGHT][square];
				score_endgame += positional_score[eGame][KNIGHT][square];
				break;
			case WB:
				score_opening += positional_score[oGame][BISHOP][square];
				score_endgame += positional_score[eGame][BISHOP][square];
				score_opening += (CountBits(get_bishop_attacks(square, pos->occupancies[both])) - bishop_unit) * bishop_mobility_opening;
				score_endgame += (CountBits(get_bishop_attacks(square, pos->occupancies[both])) - bishop_unit) * bishop_mobility_endgame;
				break;
			case WR:
				score_opening += positional_score[oGame][ROOK][square];
				score_endgame += positional_score[eGame][ROOK][square];
				if ((pos->bitboards[WP] & file_masks[square]) == 0)
				{
					score_opening += semi_open_file_score;
					score_endgame += semi_open_file_score;
				}
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & file_masks[square]) == 0)
				{
					score_opening += open_file_score;
					score_endgame += open_file_score;
				}
				break;
			case WQ:
				score_opening += positional_score[oGame][QUEEN][square];
				score_endgame += positional_score[eGame][QUEEN][square];
				score_opening += (CountBits(get_queen_attacks(square, pos->occupancies[both])) - queen_unit) * queen_mobility_opening;
				score_endgame += (CountBits(get_queen_attacks(square, pos->occupancies[both])) - queen_unit) * queen_mobility_endgame;
				break;
			case WK:
				score_opening += positional_score[oGame][KING][square];
				score_endgame += positional_score[eGame][KING][square];
				if ((pos->bitboards[WP] & file_masks[square]) == 0)
				{
					score_opening -= semi_open_file_score;
					score_endgame -= semi_open_file_score;
				}
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & file_masks[square]) == 0)
				{
					score_opening -= open_file_score;
					score_endgame -= open_file_score;
				}
				score_opening += CountBits(king_attacks[square] & pos->occupancies[white]) * king_shield_bonus;
				score_endgame += CountBits(king_attacks[square] & pos->occupancies[white]) * king_shield_bonus;
				break;
			case BP:
				score_opening -= positional_score[oGame][PAWN][mirror_score[square]];
				score_endgame -= positional_score[eGame][PAWN][mirror_score[square]];
				double_pawns = CountBits(pos->bitboards[BP] & file_masks[square]);
				if (double_pawns > 1)
				{
					score_opening -= (double_pawns - 1) * double_pawn_penalty_opening;
					score_endgame -= (double_pawns - 1) * double_pawn_penalty_endgame;
				}
				if ((pos->bitboards[BP] & isolated_masks[square]) == 0)
				{
					score_opening -= isolated_pawn_penalty_opening;
					score_endgame -= isolated_pawn_penalty_endgame;
				}
				if ((black_passed_masks[square] & pos->bitboards[WP]) == 0)
				{
					score_opening -= passed_pawn_bonus[get_rank[square]];
					score_endgame -= passed_pawn_bonus[get_rank[square]];
				}
				break;
			case BN:
				score_opening -= positional_score[oGame][KNIGHT][mirror_score[square]];
				score_endgame -= positional_score[eGame][KNIGHT][mirror_score[square]];
				break;
			case BB:
				score_opening -= positional_score[oGame][BISHOP][mirror_score[square]];
				score_endgame -= positional_score[eGame][BISHOP][mirror_score[square]];
				score_opening -= (CountBits(get_bishop_attacks(square, pos->occupancies[both])) - bishop_unit) * bishop_mobility_opening;
				score_endgame -= (CountBits(get_bishop_attacks(square, pos->occupancies[both])) - bishop_unit) * bishop_mobility_endgame;
				break;
			case BR:
				score_opening -= positional_score[oGame][ROOK][mirror_score[square]];
				score_endgame -= positional_score[eGame][ROOK][mirror_score[square]];
				if ((pos->bitboards[BP] & file_masks[square]) == 0)
				{
					score_opening -= semi_open_file_score;
					score_endgame -= semi_open_file_score;
				}
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & file_masks[square]) == 0)
				{
					score_opening -= open_file_score;
					score_endgame -= open_file_score;
				}
				break;
			case BQ:
				score_opening -= positional_score[oGame][QUEEN][mirror_score[square]];
				score_endgame -= positional_score[eGame][QUEEN][mirror_score[square]];
				score_opening -= (CountBits(get_queen_attacks(square, pos->occupancies[both])) - queen_unit) * queen_mobility_opening;
				score_endgame -= (CountBits(get_queen_attacks(square, pos->occupancies[both])) - queen_unit) * queen_mobility_endgame;
				break;
			case BK:
				score_opening -= positional_score[oGame][KING][mirror_score[square]];
				score_endgame -= positional_score[eGame][KING][mirror_score[square]];
				if ((pos->bitboards[BP] & file_masks[square]) == 0)
				{
					score_opening += semi_open_file_score;
					score_endgame += semi_open_file_score;
				}
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & file_masks[square]) == 0)
				{
					score_opening += open_file_score;
					score_endgame += open_file_score;
				}
				score_opening -= CountBits(king_attacks[square] & pos->occupancies[black]) * king_shield_bonus;
				score_endgame -= CountBits(king_attacks[square] & pos->occupancies[black]) * king_shield_bonus;
				break;
			}
			pop_bit(bitboard, square);
		}
	}
	if (phase > 24)
		phase = 24;
	score = (score_opening * phase + score_endgame * (24 - phase)) / 24;
	return (pos->side == white) ? score : -score;
}

static void ClearHashTable() {
	memset(hash_table, 0, hash_entries * sizeof(TTEntry));
}

static void InitHashTable(int mb) {
	int hash_def = 1000000 * mb;
	hash_entries = hash_def / sizeof(TTEntry);
	if (hash_table != NULL) {
		printf("Clearing hash memory...\n");
		free(hash_table);
	}
	hash_table = (TTEntry*)malloc(hash_entries * sizeof(TTEntry));
	if (hash_table == NULL) {
		mb /= 2;
		printf("Couldn't allocate memory for hash table, tryinr %dMB...", mb);
		InitHashTable(mb);
	}
	else {
		ClearHashTable();
		printf("Hash table %dMB\n", mb);
	}
}

static int Permill() {
	int pm = 0;
	for (int BN = 0; BN < 1000; BN++)
		if (hash_table[BN].hash)
			pm++;
	return pm;
}

static inline int read_hash_entry(Position* pos, int alpha, int beta, int depth) {
	TTEntry* hash_entry = &hash_table[pos->hash % hash_entries];
	if (hash_entry->hash == pos->hash)
	{
		if (hash_entry->depth >= depth)
		{
			int score = hash_entry->score;
			if (hash_entry->flag == EXACT)
				return score;
			if ((hash_entry->flag == LOWER) &&
				(score <= alpha))
				return alpha;
			if ((hash_entry->flag == UPPER) &&
				(score >= beta))
				return beta;
		}
	}
	return no_hash_entry;
}

static inline void write_hash_entry(Position* pos, int score, int depth, int hash_flag) {
	TTEntry* hash_entry = &hash_table[pos->hash % hash_entries];
	hash_entry->hash = pos->hash;
	hash_entry->score = score;
	hash_entry->flag = hash_flag;
	hash_entry->depth = depth;
}

static inline void enable_pv_scoring(MoveList* move_list, int ply) {
	follow_pv = 0;
	for (int count = 0; count < move_list->count; count++) {
		if (pv_table[0][ply] == move_list->moves[count]) {
			score_pv = 1;
			follow_pv = 1;
		}
	}
}

static inline int score_move(Position* pos, int move, int ply) {
	if (score_pv) {
		if (pv_table[0][ply] == move) {
			score_pv = 0;
			return 20000;
		}
	}
	if (get_move_capture(move))
	{
		int target_piece = WP;
		int start_piece, end_piece;
		if (pos->side == white) { start_piece = BP; end_piece = BK; }
		else { start_piece = WP; end_piece = WK; }
		for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
		{
			if (get_bit(pos->bitboards[bb_piece], get_move_target(move)))
			{
				target_piece = bb_piece;
				break;
			}
		}
		return MVV_LVA(get_move_piece(move), target_piece) + 10000;
	}
	else
	{
		if (killer_moves[0][ply] == move)
			return 9000;
		else if (killer_moves[1][ply] == move)
			return 8000;
		else
			return history_moves[get_move_piece(move)][get_move_target(move)];
	}
	return 0;
}

static inline void SortMoves(Position* pos, MoveList* move_list, int ply) {
	int move_scores[256];
	for (int count = 0; count < move_list->count; count++)
		move_scores[count] = score_move(pos, move_list->moves[count], ply);
	for (int current_move = 0; current_move < move_list->count; current_move++)
	{
		for (int next_move = current_move + 1; next_move < move_list->count; next_move++)
		{
			if (move_scores[current_move] < move_scores[next_move])
			{
				int temp_score = move_scores[current_move];
				move_scores[current_move] = move_scores[next_move];
				move_scores[next_move] = temp_score;
				int temp_move = move_list->moves[current_move];
				move_list->moves[current_move] = move_list->moves[next_move];
				move_list->moves[next_move] = temp_move;
			}
		}
	}
}

static inline int IsRepetition(Position* pos) {
	for (int index = 0; index < repetition_index; index++)
		if (repetition_table[index] == pos->hash)
			return 1;
	return 0;
}

static inline int SearchQuiescence(Position* pos, int alpha, int beta, int ply) {
	if (CheckUp(pos))
		return 0;
	int evaluation = Evaluate(pos);
	if (ply > MAX_PLY - 1)
		return evaluation;
	if (evaluation >= beta)
		return beta;
	if (evaluation > alpha)
		alpha = evaluation;
	MoveList move_list[1];
	GenerateMoves(pos, move_list);
	SortMoves(pos, move_list, ply);
	for (int count = 0; count < move_list->count; count++) {
		ply++;
		repetition_index++;
		repetition_table[repetition_index] = pos->hash;
		Position npos = *pos;
		if (!MakeMove(&npos, move_list->moves[count], only_captures)) {
			ply--;
			repetition_index--;
			continue;
		}
		int score = -SearchQuiescence(&npos, -beta, -alpha, ply + 1);
		ply--;
		repetition_index--;
		if (info.stop) return 0;
		if (score > alpha) {
			alpha = score;
			if (score >= beta)
				return beta;
		}
	}
	return alpha;
}

static void PrintInfo(int depth, int score) {
	printf("info depth %d score ", depth);
	if (abs(score) < MATE - MAX_PLY)
		printf("cp %d", score);
	else
		printf("mate %d", (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1));
	printf(" nodes %lld time %lld hashfull %d pv", info.nodes, GetTimeMs() - info.timeStart, Permill());
	for (int BN = 0; BN < pv_length[0]; BN++)
		printf(" %s", MoveToUci(pv_table[0][BN]));
	printf("\n");
}

static inline int SearchAlpha(Position* pos, int alpha, int beta, int ply, int depth) {
	pv_length[ply] = ply;
	int score;
	int hash_flag = LOWER;
	if (ply && IsRepetition(pos))
		return 0;
	int inPv = beta - alpha > 1;

	TTEntry* ttEntry = &hash_table[pos->hash % hash_entries];
	if (ttEntry->hash == pos->hash) {
		if (!inPv && ttEntry->depth >= depth) {
			if (ttEntry->flag == EXACT)return ttEntry->score;
			if (ttEntry->flag == LOWER && ttEntry->score <= alpha)return ttEntry->score;
			if (ttEntry->flag == UPPER && ttEntry->score >= beta)return ttEntry->score;
		}
	}
	else
		depth -= depth > 3;

	int inCheck = is_square_attacked(pos, (pos->side == white) ? LSB(pos->bitboards[WK]) : LSB(pos->bitboards[BK]), pos->side ^ 1);
	if (inCheck)
		depth = max(1, depth + 1);
	if (depth < 1)
		return SearchQuiescence(pos, alpha, beta, ply + 1);
	if (CheckUp(pos))
		return 0;
	if (ply > MAX_PLY - 1)
		return Evaluate(pos);
	if (depth >= 3 && inCheck == 0 && ply)
	{
		ply++;
		repetition_index++;
		repetition_table[repetition_index] = pos->hash;
		Position npos = *pos;
		if (npos.enpassant != no_sq)
			npos.hash ^= enpassant_keys[npos.enpassant];
		npos.enpassant = no_sq;
		npos.side ^= 1;
		npos.hash ^= side_key;
		score = -SearchAlpha(&npos, -beta, -beta + 1, ply + 1, depth - 1 - 2);
		ply--;
		repetition_index--;
		if (score >= beta)
			return beta;
	}
	MoveList move_list[1];
	GenerateMoves(pos, move_list);
	if (follow_pv)enable_pv_scoring(move_list, ply);
	SortMoves(pos, move_list, ply);
	int legalMoves = 0;
	for (int count = 0; count < move_list->count; count++) {
		repetition_index++;
		repetition_table[repetition_index] = pos->hash;
		Position npos = *pos;
		if (MakeMove(&npos, move_list->moves[count], all_moves) == 0) {
			repetition_index--;
			continue;
		}
		if (!legalMoves)
			//if (!legalMoves || depth < 4 || inCheck)
			score = -SearchAlpha(&npos, -beta, -alpha, ply + 1, depth - 1);
		else {
			int BR = !inPv;
			score = -SearchAlpha(&npos, -alpha - 1, -alpha, ply + 1, depth - 1 - BR);
			if (BR && score > alpha)
				score = -SearchAlpha(&npos, -alpha - 1, -alpha, ply + 1, depth - 1);
			if ((score > alpha) && (score < beta))
				score = -SearchAlpha(&npos, -beta, -alpha, ply + 1, depth - 1);
		}
		repetition_index--;
		if (info.stop)
			return 0;
		legalMoves++;
		if (score > alpha) {
			hash_flag = EXACT;
			if (get_move_capture(move_list->moves[count]) == 0)
				history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])] += depth;
			alpha = score;
			pv_table[ply][ply] = move_list->moves[count];
			for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++)
				pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
			pv_length[ply] = pv_length[ply + 1];
			if (!ply && info.post)
				PrintInfo(depth, score);
			if (alpha >= beta) {
				hash_flag = UPPER;
				if (get_move_capture(move_list->moves[count]) == 0) {
					killer_moves[1][ply] = killer_moves[0][ply];
					killer_moves[0][ply] = move_list->moves[count];
				}
				break;
			}
		}
	}
	if (!legalMoves)
		return inCheck ? ply - MATE : 0;
	write_hash_entry(pos, alpha, depth, hash_flag);
	return alpha;
}

static void SearchIteratively(Position* pos) {
	follow_pv = 0;
	score_pv = 0;
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));
	memset(pv_table, 0, sizeof(pv_table));
	memset(pv_length, 0, sizeof(pv_length));
	int score = 0;
	int alpha = -INF;
	int beta = INF;
	for (int depth = 1; depth <= info.depthLimit; depth++) {
		follow_pv = 1;
		int aspH = 16, aspL = 16;
		do {
			if (depth > 4) {
				alpha = score - aspL;
				beta = score + aspH;
			}
			score = SearchAlpha(pos, alpha, beta, 0, depth);
			if (score <= alpha) {
				alpha -= aspL;
				aspL *= 2;
			}
			else if (score >= beta) {
				beta += aspH;
				aspH *= 2;
			}
			else
				break;
		} while (!info.stop);
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit / 2)
			break;
	}
	if (info.post)
		printf("bestmove %s\n", MoveToUci(pv_table[0][0]));
}

static int UciToMove(Position* pos, char* move_string) {
	MoveList ml;
	GenerateMoves(pos, &ml);
	for (int BN = 0; BN < ml.count; BN++) {
		int move = ml.moves[BN];
		char* uci = MoveToUci(move);
		if (strncmp(move_string, uci, strlen(uci)) == 0)
			return move;
	}
	return 0;
}

static void ParsePosition(Position* pos, char* command) {
	command += 9;
	char* current_char = command;
	if (strncmp(command, "startpos", 8) == 0)
		SetFen(pos, START_FEN);
	else {
		current_char = strstr(command, "fen");
		if (current_char == NULL)
			SetFen(pos, START_FEN);
		else {
			current_char += 4;
			SetFen(pos, current_char);
		}
	}
	current_char = strstr(command, "moves");
	if (current_char != NULL) {
		current_char += 6;
		while (*current_char) {
			int move = UciToMove(pos, current_char);
			if (move == 0)
				break;
			repetition_index++;
			repetition_table[repetition_index] = pos->hash;
			MakeMove(pos, move, all_moves);
			while (*current_char && *current_char != ' ')
				current_char++;
			current_char++;
		}
	}
}

static void ParseGo(Position* pos, char* command) {
	ResetInfo();
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movestogo = 32;
	char* argument = NULL;
	if (argument = strstr(command, "infinite")) {}
	if (argument = strstr(command, "binc"))
		binc = atoi(argument + 5);
	if (argument = strstr(command, "winc"))
		winc = atoi(argument + 5);
	if (argument = strstr(command, "wtime"))
		wtime = atoi(argument + 6);
	if (argument = strstr(command, "btime"))
		btime = atoi(argument + 6);
	if ((argument = strstr(command, "movestogo")))
		movestogo = atoi(argument + 10);
	if ((argument = strstr(command, "movetime")))
		info.timeLimit = atoi(argument + 9);
	if ((argument = strstr(command, "depth")))
		info.depthLimit = atoi(argument + 6);
	if (argument = strstr(command, "nodes"))
		info.nodesLimit = atoi(argument + 5);
	int time = pos->side ? btime : wtime;
	int inc = pos->side ? binc : winc;
	if (time)
		info.timeLimit = min(time / movestogo + inc, time / 2);
	SearchIteratively(pos);
}

static void UciBench(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	SetFen(pos, START_FEN);
	info.depthLimit = 0;
	info.post = FALSE;
	U64 elapsed = 0;
	while (elapsed < 3000) {
		++info.depthLimit;
		SearchIteratively(pos);
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

static inline void UciPerformance(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	while (GetTimeMs() - info.timeStart < 3000) {
		PerftDriver(pos, info.depthLimit++);
		printf(" %2d. %8llu %12llu\n", info.depthLimit, GetTimeMs() - info.timeStart, info.nodes);
	}
	PrintSummary(GetTimeMs() - info.timeStart, info.nodes);
}

void UciCommand(Position* pos, char* input) {
	if (!strncmp(input, "ucinewgame", 10))
		ClearHashTable();
	else if (!strncmp(input, "uci", 3))
	{
		printf("id name %s\n", NAME);
		printf("option name Hash type spin default %d min %d max %d\n", hash_def, hash_min, hash_max);
		printf("uciok\n");
	}
	else if (!strncmp(input, "isready", 7))
		printf("readyok\n");
	else if (!strncmp(input, "position", 8))
		ParsePosition(pos, input);
	else if (!strncmp(input, "go", 2))
		ParseGo(pos, input);
	else if (!strncmp(input, "perft", 5))
		UciPerformance(pos);
	else if (!strncmp(input, "bench", 5))
		UciBench(pos);
	else if (!strncmp(input, "quit", 4))
		exit(0);
	else if (!strncmp(input, "print", 5))
		PrintBoard(pos);
	else if (!strncmp(input, "setoption name Hash value ", 26)) {
		int mb = hash_def;
		sscanf(input, "%*s %*s %*s %*s %d", &mb);
		if (mb < hash_min) mb = hash_min;
		if (mb > hash_max) mb = hash_max;
		InitHashTable(mb);
	}
}

static void UciLoop(Position* pos) {
	//PrintBitboard(pawn_attacks[white][c2]);
	//PrintBitboard(bitboards[WP]);
	//PrintBitboard(not_a_file);
	//PrintBitboard(1ULL << c2);
	char line[4000];
	while (fgets(line, sizeof(line), stdin))
		UciCommand(pos, line);
}

static void Init() {
	init_leapers_attacks();
	init_sliders_attacks(BISHOP);
	init_sliders_attacks(ROOK);
	InitRandomKeys();
	InitEvaluationMasks();
	InitHashTable(hash_def);
}

int main() {
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);
	printf("%s %s\n", NAME, VERSION);
	Init();
	Position pos;
	SetFen(&pos, START_FEN);
	UciLoop(&pos);
	free(hash_table);
	return 0;
}