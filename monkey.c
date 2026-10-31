#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_PLY 64
#define MAX_MOVES 256
#define MATE 32000
#define INF 32001
#define S8 signed __int8
#define U8 unsigned __int8
#define S16 signed __int16
#define U16 unsigned __int16
#define S32 signed __int32
#define S64 signed __int64
#define U64 unsigned __int64
#define FALSE 0
#define TRUE 1
#define NAME "Monkey"
#define VERSION "2026-10-02"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define RANK_1_BB 0x00000000000000FFULL
#define RANK_2_BB 0x000000000000FF00ULL
#define RANK_3_BB 0x0000000000FF0000ULL
#define RANK_4_BB 0x00000000FF000000ULL
#define RANK_5_BB 0x000000FF00000000ULL
#define RANK_6_BB 0x0000FF0000000000ULL
#define RANK_7_BB 0x00FF000000000000ULL
#define RANK_8_BB 0xFF00000000000000ULL
#define FILE_A_BB 0x0101010101010101ULL
#define FILE_B_BB 0x0202020202020202ULL
#define FILE_C_BB 0x0404040404040404ULL
#define FILE_D_BB 0x0808080808080808ULL
#define FILE_E_BB 0x1010101010101010ULL
#define FILE_F_BB 0x2020202020202020ULL
#define FILE_G_BB 0x4040404040404040ULL
#define FILE_H_BB 0x8080808080808080ULL
#define BIT(square) (1ULL << (square))
#define FLIP_BB(bitboard) _byteswap_uint64(bitboard)
#define FLIP_SQ(sq) ((sq) ^ 0x38)
#define TIME_MS() GetTickCount64()
#define LSB(bitboard) _tzcnt_u64(bitboard)
#define COUNT(bitboard) _mm_popcnt_u64(bitboard)

enum Color { WHITE, BLACK, COLOR_NB };
enum Piece { WPAWN, WKNIGHT, WBISHOP, WROOK, WQUEEN, WKING, BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, BKING, PIECE_NB };
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PT_NB };
enum Bound { UPPER, LOWER, EXACT };
enum File{ FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };
enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };
enum Castle { CASTLE_WK = 0b0001, CASTLE_WQ = 0b0010, CASTLE_BK = 0b0100, CASTLE_BQ = 0b1000 };
enum Square{
	SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
	SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
	SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
	SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
	SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
	SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
	SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
	SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
	SQ_NB
};

typedef struct {
	U8 flipped;
	U8 move50;
	U8 castleFlags;
	U64 pieces[PIECE_NB];
	U64 ep;
}Position;

typedef union {
	U16 move;
	struct {
		U8 from;
		U8 to;
		U8 promo;
		U8 flags;
	};
}Move;

typedef struct {
	S16 score;
	Move move;
	Move killer1;
	Move killer2;
} Stack;

typedef struct {
	U64 hash;
	Move move;
	S16 score;
	U8 depth;
	U8 flag;
}TTEntry;

typedef struct {
	U8 post;
	U8 stop;
	U8 depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}SearchInfo;

const int insufVal[PT_NB] = { 5,2,3,5,5,0 };
const int phaseVal[PT_NB] = { 0,1,1,2,4,0 };
int historyCount = 0;
U64 keys[848];
U64 historyHash[1024];
const U64 tt_count = 64ULL << 15;
U64 bbSquare[64];
U64 bbKnightAttack[64];
U64 bbKingAttack[64];
int hh[2][64][64];
SearchInfo info;
Stack stack[128];
TTEntry tt[64ULL << 15];
int mg_material[PT_NB] = { 82, 337, 365, 477, 1025, 0 };
int eg_material[PT_NB] = { 94, 281, 297, 512,  936, 0 };
int mx_material[PT_NB] = { 94, 337, 365, 512, 1025, 0 };
int material[7] = { 100,320,330,500,900,0,0 };
int LINEAR_REDUCTION[64][64];

int boardCastle[64] = {
	 7, 15, 15, 15,  3, 15, 15, 11,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	13, 15, 15, 15, 12, 15, 15, 14
};

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

int* mg_table[PT_NB] = {
	mg_pawn_table,
	mg_knight_table,
	mg_bishop_table,
	mg_rook_table,
	mg_queen_table,
	mg_king_table
};

int* eg_table[PT_NB] = {
	eg_pawn_table,
	eg_knight_table,
	eg_bishop_table,
	eg_rook_table,
	eg_queen_table,
	eg_king_table
};

int mg_pst[PT_NB][64];
int eg_pst[PT_NB][64];
int mx_pst[PT_NB][64];

U64 rook_magic[64] = {
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
U64 bishop_magic[64] = {
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

static const int rook_relevant_bits[64] = {
	12,11,11,11,11,11,11,12,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	12,11,11,11,11,11,11,12
};

static const int bishop_relevant_bits[64] = {
	6,5,5,5,5,5,5,6,
	5,5,5,5,5,5,5,5,
	5,5,7,7,7,7,5,5,
	5,5,7,9,9,7,5,5,
	5,5,7,9,9,7,5,5,
	5,5,7,7,7,7,5,5,
	5,5,5,5,5,5,5,5,
	6,5,5,5,5,5,5,6
};

static U64 rookMask[64];
static U64 bishopMask[64];
U64 bishopAttacks[64][512];
U64 rookAttacks[64][4096];

void UciCommand(Position* pos, char* line);

static inline void TTClear() { memset(tt, 0, sizeof(tt)); }
static inline void HHClear() { memset(hh, 0, sizeof(hh)); }
static inline void SSClear() { memset(stack, 0, sizeof(stack)); }
static inline U64 East(const U64 bb) { return (bb << 1) & ~FILE_A_BB; }
static inline U64 West(const U64 bb) { return (bb >> 1) & ~FILE_H_BB; }
static inline U64 North(const U64 bb) { return bb << 8; }
static inline U64 South(const U64 bb) { return bb >> 8; }
static inline U64 NW(const U64 bb) { return (bb << 7) & ~FILE_H_BB; }
static inline U64 NE(const U64 bb) { return (bb << 9) & ~FILE_A_BB; }
static inline U64 SW(const U64 bb) { return (bb >> 9) & ~FILE_H_BB; }
static inline U64 SE(const U64 bb) { return (bb >> 7) & ~FILE_A_BB; }
static inline int PieceColorOf(int piece) { return piece / PT_NB; }
static inline int PieceTypeOf(int piece) { return piece % PT_NB; }
static inline int FileOf(int sq) { return sq % 8; }
static inline int RankOf(int sq) { return sq / 8; }
static inline int Center(int rank, int file) { return -abs(rank * 2 - 7) / 2 - abs(file * 2 - 7) / 2; }
static inline int CenterSq(int sq) { return Center(RankOf(sq), FileOf(sq)); }
static inline int MakePiece(int flipped, int pt) { return flipped * PT_NB + pt; }

//create mask for slider excluding edge squares
static U64 MaskRookAttacks(int sq) {
	U64 attacks = 0ULL;
	int r = sq / 8, f = sq % 8;
	for (int rr = r + 1; rr <= 6; ++rr) attacks |= (1ULL << (rr * 8 + f));
	for (int rr = r - 1; rr >= 1; --rr) attacks |= (1ULL << (rr * 8 + f));
	for (int ff = f + 1; ff <= 6; ++ff) attacks |= (1ULL << (r * 8 + ff));
	for (int ff = f - 1; ff >= 1; --ff) attacks |= (1ULL << (r * 8 + ff));
	return attacks;
}

//create mask for slider excluding edge squares
static U64 MaskBishopAttacks(int sq) {
	U64 attacks = 0ULL;
	int r = sq / 8, f = sq % 8;
	for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; ++rr, ++ff) attacks |= (1ULL << (rr * 8 + ff));
	for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; ++rr, --ff) attacks |= (1ULL << (rr * 8 + ff));
	for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; --rr, ++ff) attacks |= (1ULL << (rr * 8 + ff));
	for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; --rr, --ff) attacks |= (1ULL << (rr * 8 + ff));
	return attacks;
}

U64 SetOccupancy(int index, int bits_in_mask, U64 attack_mask){
	U64 occupancy = 0ULL;
	for (int count = 0; count < bits_in_mask; count++){
		int square = LSB(attack_mask);
		attack_mask &= attack_mask - 1;
		if (index & (1 << count))
			occupancy |= (1ULL << square);
	}
	return occupancy;
}

// New magic-based accessors
static U64 BishopAttack(int sq, const U64 blockers) {
	U64 occ = blockers & bishopMask[sq];
	int bits = bishop_relevant_bits[sq];
	U64 index = (occ * bishop_magic[sq]) >> (64 - bits);
	return bishopAttacks[sq][(size_t)index];
}

static U64 RookAttack(int sq, const U64 blockers) {
	U64 occ = blockers & rookMask[sq];
	int bits = rook_relevant_bits[sq];
	U64 index = (occ * rook_magic[sq]) >> (64 - bits);
	return rookAttacks[sq][(size_t)index];
}

static U64 Rand64() {
	static U64 next = 1;
	next = next * 12345104729 + 104723;
	return next;
}

static void Swap(U64* a, U64* b) {
	U64 temp = *a;
	*a = *b;
	*b = temp;
}

//Return occupancy mask for a color (0 = side-to-move, 1 = opponent)
static inline U64 ColorOccupancy(const Position* pos, int flipped) {
	int start = flipped * PT_NB;
	U64 occ = 0;
	for (int pt = 0; pt < PT_NB; ++pt)
		occ |= pos->pieces[start + pt];
	return occ;
}

//Return occupancy mask for all pieces
static inline U64 AllOccupancy(const Position* pos) {
	return ColorOccupancy(pos, 0) | ColorOccupancy(pos, 1);
}

//Determine piece type on square (ignoring color). Returns PT_NB if empty
static int PieceTypeOn(Position* pos, int sq) {
	const U64 bb = bbSquare[sq];
	for (int i = PAWN; i < PT_NB; ++i)
		if ((pos->pieces[MakePiece(0, i)] | pos->pieces[MakePiece(1, i)]) & bb)
			return i;
	return PT_NB;
}

static int PieceOnSquare(Position* pos, int sq) {
	const U64 bb = bbSquare[sq];
	for (int i = 0; i < PIECE_NB; ++i)
		if (pos->pieces[i] & bb)
			return i;
	return PIECE_NB;
}

//Raycast helper
static U64 Ray(const U64 bb, const U64 blockers, U64(*f)(U64)) {
	U64 mask = f(bb);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	return mask;
}

static inline U64 KnightAttackBB(const U64 bb) {
	return (((bb << 15) | (bb >> 17)) & 0x7F7F7F7F7F7F7F7FULL) | (((bb << 17) | (bb >> 15)) & 0xFEFEFEFEFEFEFEFEULL) |
		(((bb << 10) | (bb >> 6)) & 0xFCFCFCFCFCFCFCFCULL) | (((bb << 6) | (bb >> 10)) & 0x3F3F3F3F3F3F3F3FULL);
}

static inline U64 KnightAttack(const int sq) {
	return bbKnightAttack[sq];
}

static inline U64 BishopAttackBB(U64 bb, U64 blockers) {
	return Ray(bb, blockers, NW) | Ray(bb, blockers, NE) | Ray(bb, blockers, SW) | Ray(bb, blockers, SE);
}

static inline U64 RookAttackBB(U64 bb, U64 blockers) {
	return Ray(bb, blockers, North) | Ray(bb, blockers, East) | Ray(bb, blockers, South) | Ray(bb, blockers, West);
}

static inline U64 KingAttackBB(const U64 bb) {
	return (bb << 8) | (bb >> 8) | (((bb >> 1) | (bb >> 9) | (bb << 7)) & 0x7F7F7F7F7F7F7F7FULL) |
		(((bb << 1) | (bb << 9) | (bb >> 7)) & 0xFEFEFEFEFEFEFEFEULL);
}

static inline U64 KingAttack(const int sq) {
	return bbKingAttack[sq];
}

//Flip the whole position: flip bitboards and swap sides (first 6 <-> last 6)
static void FlipPosition(Position* pos) {
	// Flip ep bitboard and piece bitboards, then swap halves so index 0..5 (us) becomes opponent
	for (int i = 0; i < PT_NB; ++i) {
		U64 a = FLIP_BB(pos->pieces[i]);
		U64 b = FLIP_BB(pos->pieces[PT_NB + i]);
		pos->pieces[i] = b;
		pos->pieces[PT_NB + i] = a;
	}
	pos->ep = FLIP_BB(pos->ep);
	pos->castleFlags = ((pos->castleFlags >> 2) | (pos->castleFlags << 2)) & 0xf;
	pos->flipped = !pos->flipped;
}

static U64 GetHash(const Position* pos) {
	U64 hash = pos->flipped;
	for (S32 pt = PAWN; pt < PT_NB; ++pt) {
		U64 copy = pos->pieces[MakePiece(0, pt)]; // side-to-move pieces
		while (copy) {
			const S32 sq = LSB(copy);
			copy &= copy - 1;
			hash ^= keys[pt * 64 + sq];
		}
		copy = pos->pieces[PT_NB + pt]; // opponent pieces
		while (copy) {
			const S32 sq = LSB(copy);
			copy &= copy - 1;
			hash ^= keys[(pt + 6) * 64 + sq];
		}
	}
	if (pos->ep)
		hash ^= keys[12 * 64 + LSB(pos->ep)];
	if (pos->castleFlags)
		hash ^= keys[13 * 64 + pos->castleFlags];
	return hash;
}

static void PrintBoard(Position* pos) {
	Position npos = *pos;
	if (npos.flipped)
		FlipPosition(&npos);
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	printf(t);
	for (int r = 7; r >= 0; r--) {
		printf(s);
		printf(" %d |", r + 1);
		for (int f = 0; f < 8; f++) {
			int sq = r * 8 + f;
			int piece = PieceOnSquare(&npos, sq);
			printf(" %c |", "ANBRQKanbrqk "[piece]);
		}
		printf(" %d \n", r + 1);
	}
	printf(s);
	printf(t);
	char castling[5] = "KQkq";
	for (int n = 0; n < 4; n++)
		if (!(npos.castleFlags & (1 << n)))
			castling[n] = '-';
	printf("side     : %16s\n", pos->flipped ? "black" : "white");
	printf("castling : %16s\n", castling);
	printf("hash     : %16llx\n", GetHash(pos));
	printf("score    : %16d\n", EvalPosition(pos));
}

static int CheckUp() {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && TIME_MS() - info.timeStart > info.timeLimit)
			info.stop = TRUE;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = TRUE;
	}
	return info.stop;
}

static U64 Attacked(Position* pos, int sq, int byColor) {
	const U64 bb = bbSquare[sq];
	const U64 N = pos->pieces[MakePiece(byColor,KNIGHT)];
	const U64 BQ = pos->pieces[MakePiece(byColor,BISHOP)] | pos->pieces[MakePiece(byColor,QUEEN)];
	const U64 RQ = pos->pieces[MakePiece(byColor,ROOK)] | pos->pieces[MakePiece(byColor,QUEEN)];
	const U64 pawns = pos->pieces[MakePiece(byColor,PAWN)];
	const U64 pawn_attacks = byColor ? SW(pawns) | SE(pawns) : NW(pawns) | NE(pawns);
	U64 blockers = AllOccupancy(pos);
	return (pawn_attacks & bb) | (N & KnightAttack(sq)) |
		(BishopAttack(sq, blockers) & BQ) |
		(RookAttack(sq, blockers) & RQ) |
		(KingAttack(sq) & pos->pieces[MakePiece(byColor,KING)]);
}

static void AddMove(Move* const moveList, int* num_moves, const int from, const int to, const int promo) {
	Move* m = &moveList[(*num_moves)++];
	m->from = from;
	m->to = to;
	m->promo = promo;
}

/* Pawn move generation for the side-to-move */
static void GeneratePawnMoves(Move* const moveList, int* num_moves, U64 to_mask, const int offset) {
	while (to_mask) {
		const U64 to = LSB(to_mask);
		to_mask &= to_mask - 1;
		if (to >= 56 || to <= 7) {
			AddMove(moveList, num_moves, to + offset, to, KNIGHT);
			AddMove(moveList, num_moves, to + offset, to, BISHOP);
			AddMove(moveList, num_moves, to + offset, to, ROOK);
			AddMove(moveList, num_moves, to + offset, to, QUEEN);
		}
		else
			AddMove(moveList, num_moves, to + offset, to, PT_NB);
	}
}

/* Generate moves for piece type for side-to-move */
static void GeneratePieceMoves(Move* const moveList, int* num_moves, const Position* pos, const U64 blockers,const int piece, const U64 to_mask, U64(*func)(int, U64)) {
	U64 copy = pos->pieces[piece]; // piece bitboard for side-to-move
	while (copy) {
		const int fr = (int)LSB(copy);
		copy &= copy - 1;
		U64 moves = func(fr, blockers) & to_mask;
		while (moves) {
			const int to = (int)LSB(moves);
			moves &= moves - 1;
			AddMove(moveList, num_moves, fr, to, PT_NB);
		}
	}
}

static int GenerateMoves(const Position* pos, Move* const moveList, int only_captures) {
	int num_moves = 0;
	const U64 bbUs = ColorOccupancy(pos, WHITE);
	const U64 bbThem = ColorOccupancy(pos, BLACK);
	const U64 all = bbUs | bbThem;
	const U64 to_mask = only_captures ? bbThem : ~bbUs;
	const U64 pawnsUs = pos->pieces[WPAWN];
	U64 maskToPawn = North(pawnsUs) & ~all & (only_captures ? 0xFF00000000000000ULL : 0xFFFFFFFFFFFF0000ULL);
	GeneratePawnMoves(moveList, &num_moves, maskToPawn, -8);
	if (!only_captures)
		GeneratePawnMoves(moveList, &num_moves, North(North(pawnsUs & 0xFF00ULL) & ~all) & ~all, -16);
	GeneratePawnMoves(moveList, &num_moves, NW(pawnsUs) & (bbThem | pos->ep), -7);
	GeneratePawnMoves(moveList, &num_moves, NE(pawnsUs) & (bbThem | pos->ep), -9);
	GeneratePieceMoves(moveList, &num_moves, pos, all, WKNIGHT, to_mask, KnightAttack);
	GeneratePieceMoves(moveList, &num_moves, pos, all, WBISHOP, to_mask, BishopAttack);
	GeneratePieceMoves(moveList, &num_moves, pos, all, WQUEEN, to_mask, BishopAttack);
	GeneratePieceMoves(moveList, &num_moves, pos, all, WROOK, to_mask, RookAttack);
	GeneratePieceMoves(moveList, &num_moves, pos, all, WQUEEN, to_mask, RookAttack);
	GeneratePieceMoves(moveList, &num_moves, pos, all, WKING, to_mask, KingAttack);
	if (!only_captures && (pos->castleFlags & CASTLE_WK) && !(all & 0x60ULL) && !Attacked(pos, SQ_E1,BLACK) && !Attacked(pos, SQ_F1,BLACK))
		AddMove(moveList, &num_moves, SQ_E1, SQ_G1, PT_NB);
	if (!only_captures && (pos->castleFlags & CASTLE_WQ) && !(all & 0xEULL) && !Attacked(pos, SQ_E1,BLACK) && !Attacked(pos, SQ_D1,BLACK))
		AddMove(moveList, &num_moves, SQ_E1, SQ_C1, PT_NB);
	return num_moves;
}

static int IsRepetition(Position* pos, U64 hash) {
	int limit = max(0, historyCount - pos->move50);
	for (int n = historyCount - 4; n >= limit; n -= 2)
		if (historyHash[n] == hash)
			return TRUE;
	return FALSE;
}

static void SetFen(Position* pos, char* fen) {
	memset(pos, 0, sizeof(Position));
	int sq = 56;
	while (*fen && *fen != ' ') {
		U64 bb = 1ull << sq;
		switch (*fen) {
		case '1': sq += 1; break;
		case '2': sq += 2; break;
		case '3': sq += 3; break;
		case '4': sq += 4; break;
		case '5': sq += 5; break;
		case '6': sq += 6; break;
		case '7': sq += 7; break;
		case '8': sq += 8; break;
		case 'P': pos->pieces[WPAWN] |= bb; ++sq; break;
		case 'N': pos->pieces[WKNIGHT] |= bb; ++sq; break;
		case 'B': pos->pieces[WBISHOP] |= bb; ++sq; break;
		case 'R': pos->pieces[WROOK] |= bb; ++sq; break;
		case 'Q': pos->pieces[WQUEEN] |= bb; ++sq; break;
		case 'K': pos->pieces[WKING] |= bb; ++sq; break;
		case 'p': pos->pieces[BPAWN] |= bb; ++sq; break;
		case 'n': pos->pieces[BKNIGHT] |= bb; ++sq; break;
		case 'b': pos->pieces[BBISHOP] |= bb; ++sq; break;
		case 'r': pos->pieces[BROOK] |= bb; ++sq; break;
		case 'q': pos->pieces[BQUEEN] |= bb; ++sq; break;
		case 'k': pos->pieces[BKING] |= bb; ++sq; break;
		case '/': sq -= 16; break;
		}
		fen++;
	}
	fen++;
	int flipped = *fen == 'w' ? WHITE : BLACK;
	while (*fen && *fen != ' ')
		fen++;
	fen++;
	while (*fen && *fen != ' ') {
		switch (*fen) {
		case 'K': pos->castleFlags |= CASTLE_WK; break;
		case 'Q': pos->castleFlags |= CASTLE_WQ; break;
		case 'k': pos->castleFlags |= CASTLE_BK; break;
		case 'q': pos->castleFlags |= CASTLE_BQ; break;
		}
		fen++;
	}
	fen++;
	if (*fen != '-') {
		const int sq = (fen[0] - 'a') + 8 * (fen[1] - '1');
		pos->ep = bbSquare[sq];
	}
	while (*fen && *fen != ' ') fen++; fen++;
	pos->move50 = atoi(fen);
	if (flipped)
		FlipPosition(pos);
}

static char* ParseToken(char* string, char* token) {
	while (*string == ' ')
		string++;
	while (*string != ' ' && *string != '\0' && *string != '\n')
		*token++ = *string++;
	*token = '\0';
	return string;
}

/* Make move for side-to-move representation (pieces[0..5] = our pieces) */
/* Returns 1 if move is legal (not leaving king in check), otherwise 0 */
static int MakeMove(Position* pos, const Move* move) {
	const int pt = PieceTypeOn(pos, move->from);
	const int ptCaptured = PieceTypeOn(pos, move->to);
	const U64 bbFrom = bbSquare[move->from];
	const U64 bbTo = bbSquare[move->to];
	pos->move50++;
	if (ptCaptured != PT_NB || pt == PAWN)
		pos->move50 = 0;
	// Move our piece bitboard (side-to-move is in first half)
	pos->pieces[pt] ^= bbFrom | bbTo;
	// Handle en-passant capture
	if (pt == PAWN && bbTo == pos->ep)
		pos->pieces[BPAWN] ^= bbTo >> 8; // remove opponent pawn
	pos->ep = 0x0ULL;
	// Set ep if double pawn push
	if (pt == PAWN && move->to - move->from == 16)
		pos->ep = bbTo >> 8;
	// Capture
	if (ptCaptured != PT_NB)
		pos->pieces[MakePiece(BLACK, ptCaptured)] ^= bbTo; // opponent piece removed
	// King move may imply rook move for castling (operate on our rook bitboard)
	if (pt == KING) {
		const U64 bb = move->to - move->from == 2 ? 0xa0ULL : move->to - move->from == -2 ? 0x9ULL : 0x0ULL;
		pos->pieces[WROOK] ^= bb;
	}
	// Promotion
	if (pt == PAWN && move->to >= 56) {
		pos->pieces[WPAWN] ^= bbTo;
		pos->pieces[MakePiece(WHITE,move->promo)] ^= bbTo;
	}
	FlipPosition(pos);
	pos->castleFlags &= boardCastle[move->from] & boardCastle[move->to];
	return !Attacked(pos, (int)LSB(pos->pieces[BKING]),WHITE);
}

static char* MoveToUci(Move move, int flip) {
	static char str[6] = { 0 };
	str[0] = 'a' + FileOf(move.from);
	str[1] = '1' + (flip ? (7 - RankOf(move.from)) : RankOf(move.from));
	str[2] = 'a' + FileOf(move.to);
	str[3] = '1' + (flip ? (7 - RankOf(move.to)) : RankOf(move.to));
	str[4] = "\0nbrq\0\0"[move.promo];
	return str;
}

static Move UciToMove(char* s, int flip) {
	Move m;
	m.from = (s[0] - 'a');
	int f = (s[1] - '1');
	m.from += 8 * (flip ? 7 - f : f);
	m.to = (s[2] - 'a');
	f = (s[3] - '1');
	m.to += 8 * (flip ? 7 - f : f);
	m.promo = PT_NB;
	switch (s[4]) {
	case 'N':
	case 'n':
		m.promo = KNIGHT;
		break;
	case 'B':
	case 'b':
		m.promo = BISHOP;
		break;
	case 'R':
	case 'r':
		m.promo = ROOK;
		break;
	case 'Q':
	case 'q':
		m.promo = QUEEN;
		break;
	}
	return m;
}

static void PrintBitboard(U64 bb) {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	printf(t);
	for (int r = 7; r >= 0; r--) {
		printf(s);
		printf(" %d |", r + 1);
		for (int f = 0; f < 8; f++) {
			int sq = r * 8 + f;
			printf(" %c |", bb & 1ull << sq ? 'x' : ' ');
		}
		printf(" %d \n", r + 1);
	}
	printf(s);
	printf(t);
}

static U64 Attacks(int pt, int sq, U64 blockers) {
	switch (pt) {
	case ROOK:
		return RookAttack(sq, blockers);
	case BISHOP:
		return BishopAttack(sq, blockers);
	case QUEEN:
		return RookAttack(sq, blockers) | BishopAttack(sq, blockers);
	case KNIGHT:
		return KnightAttack(sq);
	case KING:
		return KingAttack(sq);
	default:
		return 0;
	}
}

static int EvalPosition(Position* pos) {
	int phase = 0;
	int score = 0;
	int scoreMg = 0;
	int scoreEg = 0;
	int insufficient[2] = { 0 };
	for (int c = WHITE; c < COLOR_NB; ++c) {
		U64 occUs = ColorOccupancy(pos, 0);
		U64 bbBlockers = AllOccupancy(pos);
		U64 bbStart1 = pos->pieces[BPAWN];
		U64 bbControl1 = SW(bbStart1) | SE(bbStart1);
		for (int pt = PAWN; pt < PT_NB; ++pt) {
			U64 copy = pos->pieces[pt];
			while (copy) {
				const int fr = (int)LSB(copy);
				copy &= copy - 1;
				scoreMg += mg_pst[pt][fr];
				scoreEg += eg_pst[pt][fr];
				phase += phaseVal[pt];
				insufficient[c] += insufVal[pt];
				if (pt > PAWN && pt < KING) {
					U64 bbAttack = Attacks(pt, fr, bbBlockers);
					score += COUNT(bbAttack & ~bbControl1 &~occUs);
				}
			}
		}
		U64 bbStart0 = pos->pieces[KING];
		U64 file0 = FILE_A_BB << FileOf(LSB(bbStart0));
		file0 |= East(file0) | West(file0);
		U64 bbAttack0 = file0 & ~(FILE_D_BB | FILE_E_BB);
		bbAttack0 &= (pos->pieces[PAWN]);
		scoreMg += COUNT(bbAttack0 & RANK_2_BB) * 16;
		scoreMg += COUNT(bbAttack0 & RANK_3_BB) * 8;
		FlipPosition(pos);
		score = -score;
		scoreMg = -scoreMg;
		scoreEg = -scoreEg;
	}
	if (phase > 24) phase = 24;
	score += (scoreMg * phase + scoreEg * (24 - phase)) / 24;
	if (max(insufficient[0], insufficient[1]) < 5)return 0;
	if (insufficient[score < 0] < 4)return 0;
	return (100 - pos->move50) * score / 100;
}

static int IsPseudolegalMove(const Position* pos, const Move move) {
	Move moves[256];
	const int num_moves = GenerateMoves((Position*)pos, moves, 0);
	for (int i = 0; i < num_moves; ++i)
		if (moves[i].from == move.from && moves[i].to == move.to)
			return 1;
	return 0;
}

static void PrintPv(const Position* pos, const Move move) {
	if (!IsPseudolegalMove(pos, move))
		return;
	const Position npos = *pos;
	if (!MakeMove((Position*)&npos, &move))
		return;
	printf(" %s", MoveToUci(move, pos->flipped));
	const U64 hash = GetHash(&npos);
	TTEntry* tt_entry = tt + (hash % tt_count);
	if (tt_entry->hash != hash || IsRepetition((Position*)&npos, hash))
		return;
	historyHash[historyCount++] = hash;
	PrintPv(&npos, tt_entry->move);
	historyCount--;
}

static int Permill() {
	int pm = 0;
	for (int n = 0; n < 1000; n++)
		if (tt[n].hash)
			pm++;
	return pm;
}

static void PrintInfo(Position* pos, int depth, int score) {
	printf("info depth %d score ", depth);
	if (abs(score) < MATE - MAX_PLY)
		printf("cp %d", score);
	else
		printf("mate %d", (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1));
	printf(" time %lld", TIME_MS() - info.timeStart);
	printf(" nodes %lld", info.nodes);
	printf(" hashfull %d pv", Permill());
	PrintPv(pos, stack[0].move);
	printf("\n");
}

static S16 SearchAlpha(Position* pos, int alpha, int beta, int depth, int ply, Stack* stack, int doNull) {
	if (CheckUp())
		return 0;
	int  mate_value = MATE - ply;
	if (alpha < -mate_value)
		alpha = -mate_value;
	if (beta > mate_value - 1)
		beta = mate_value - 1;
	if (alpha >= beta)
		return alpha;
	const int staticEval = stack[ply].score = EvalPosition(pos);
	if (ply >= MAX_PLY)
		return staticEval;
	const U64 inCheck = Attacked(pos, (int)LSB(pos->pieces[WKING]),BLACK);
	if (inCheck)
		depth = max(1, depth + 1);
	int inQuiescence = depth < 1;
	const U64 hash = GetHash(pos);
	if (ply && !inQuiescence)
		if (pos->move50 >= 100 || IsRepetition(pos, hash))
			return 0;
	TTEntry* tt_entry = tt + (hash % tt_count);
	Move ttMove = { 0 };
	int inPv = beta - alpha > 1;
	if (tt_entry->hash == hash) {
		ttMove = tt_entry->move;
		if (!inPv && tt_entry->depth >= depth) {
			if (tt_entry->flag == EXACT)return tt_entry->score;
			if (tt_entry->flag == LOWER && tt_entry->score <= alpha)return tt_entry->score;
			if (tt_entry->flag == UPPER && tt_entry->score >= beta)return tt_entry->score;
		}
	}
	else
		depth -= depth > 3;
	if (inQuiescence && alpha < staticEval) {
		alpha = staticEval;
		if (alpha >= beta)
			return beta;
	}

	int improving = ply > 1 && staticEval > stack[ply - 2].score;
	if (ply && !inQuiescence && !inCheck && !inPv) {
		// Reverse futility pruning
		if (depth < 8) {
			if (staticEval - 71 * (depth - improving) >= beta)
				return staticEval;

			inQuiescence = staticEval + 238 * depth < alpha;
		}

		// Null move pruning
		if (depth > 2 && staticEval >= beta && staticEval >= stack[ply].score && doNull
			&&(pos->pieces[KNIGHT] || pos->pieces[BISHOP] || pos->pieces[ROOK] || pos->pieces[QUEEN])) {
			Position npos = *pos;
			FlipPosition(&npos);
			npos.ep = 0;
			if (-SearchAlpha(&npos,
				-beta,
				-alpha,
				depth - 4 - depth / 5 - min((staticEval - beta) / 196, 3),
				ply + 1,
				stack,
				0) >= beta)
				return beta;
		}
	}
	historyHash[historyCount++] = hash;
	U8 tt_flag = LOWER;
	Move movesList[MAX_MOVES];
	S64 scoreList[MAX_MOVES];
	Move quietList[MAX_MOVES];
	S16 score;
	int quietCount = 0;
	int legalCount = 0;
	const int movesCount = GenerateMoves(pos, movesList, inQuiescence);
	for (int j = 0; j < movesCount; ++j) {
		Move m = movesList[j];
		const int ptSou = PieceTypeOn(pos, m.from);
		int ptDes = m.promo == PT_NB ? PieceTypeOn(pos, m.to) : m.promo;
		if (m.move == ttMove.move)
			scoreList[j] = 1LL << 62;
		else if (ptDes != PT_NB)
			scoreList[j] = (1LL << 54) + ptDes * 10 - ptSou;
		else if (m.move == stack[ply].killer1.move)
			scoreList[j] = 1LL << 49;
		else if (m.move == stack[ply].killer2.move)
			scoreList[j] = 1LL << 48;
		else
			scoreList[j] = hh[pos->flipped][m.from][m.to];
	}
	for (int i = 0; i < movesCount; ++i) {
		int bstIdx = i;
		for (int j = i + 1; j < movesCount; ++j)
			if (scoreList[bstIdx] < scoreList[j])
				bstIdx = j;
		Move move = movesList[bstIdx];
		scoreList[bstIdx] = scoreList[i];
		movesList[bstIdx] = movesList[i];

		const int gain = material[move.promo] + material[PieceTypeOn(pos, move.to)];

		// Delta pruning
		if (inQuiescence && !inCheck &&
			staticEval + 50 + mx_material[PieceTypeOn(pos, move.to)] < alpha)
			break;

		Position npos = *pos;
		if (!MakeMove(&npos, &move))
			continue;
		if (!legalCount || depth < 4)
			score = -SearchAlpha(&npos, -beta, -alpha, depth - 1, ply + 1, stack, 1);
		else {
			int r = LINEAR_REDUCTION[min(63, depth)][min(63, legalCount)];
			if (inPv)
				r--;
			if (r < 0)
				r = 0;
			if (r > depth - 2)
				r = depth - 2;
			score = -SearchAlpha(&npos, -alpha - 1, -alpha, depth - 1 - r, ply + 1, stack, 1);
			if (r && score > alpha)
				score = -SearchAlpha(&npos, -alpha - 1, -alpha, depth - 1, ply + 1, stack, 1);
			if (score > alpha && score < beta)
				score = -SearchAlpha(&npos, -beta, -alpha, depth - 1, ply + 1, stack, 1);
		}
		if (info.stop)
			break;
		legalCount++;
		int isQuiet = move.promo == PT_NB && PieceTypeOn((Position*)pos, move.to) == PT_NB && (PieceTypeOn((Position*)pos, move.from) != PAWN || bbSquare[move.to] != pos->ep);
		if (isQuiet)
			quietList[quietCount++] = move;
		if (alpha < score) {
			alpha = score;
			tt_flag = EXACT;
			stack[ply].move = move;
			if (!ply && info.post)
				PrintInfo(pos, depth, score);
			if (alpha >= beta) {
				tt_flag = UPPER;
				if (!gain) {
					stack[ply].killer2 = stack[ply].killer1;
					stack[ply].killer1 = move;
					int bonus = depth * depth;
					hh[pos->flipped][move.from][move.to] +=
						depth * depth - depth * depth * hh[pos->flipped][move.from][move.to] / 512;
					for (int j = 0; j < quietCount; ++j) {
						hh[pos->flipped][quietList[j].from][quietList[j].to] -=
							bonus + bonus * hh[pos->flipped][quietList[j].from][quietList[j].to] / 512;
					}
				}
				break;
			}
		}
	}
	historyCount--;
	if (info.stop)
		return 0;
	if (!legalCount)
		return inQuiescence ? alpha : inCheck ? ply - MATE : 0;
	tt_entry->hash = hash;
	tt_entry->move = stack[ply].move;
	tt_entry->depth = max(0, depth);
	tt_entry->score = alpha;
	tt_entry->flag = tt_flag;
	return alpha;
}

static void SearchIteratively(Position* pos) {
	TTClear();
	SSClear();
	int score = 0;
	int alpha = -MATE;
	int beta = MATE;
	for (int depth = 1; depth <= info.depthLimit; ++depth) {
		int aspH = 16, aspL = 16;
		do {
			if (depth > 4) {
				alpha = score - aspL;
				beta = score + aspH;
			}
			score = SearchAlpha(pos, alpha, beta, depth, 0, stack, 1);
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
		if (info.stop)
			break;
		if (info.timeLimit && TIME_MS() - info.timeStart > info.timeLimit / 2)
			break;
	}
	if (info.post) {
		char* uci = MoveToUci(stack[0].move, pos->flipped);
		printf("bestmove %s\n", uci);
		fflush(stdout);
	}
}

static void ResetInfo() {
	info.timeStart = TIME_MS();
	info.timeLimit = 0;
	info.depthLimit = MAX_PLY;
	info.nodesLimit = 0;
	info.nodes = 0;
	info.stop = FALSE;
	info.post = TRUE;
}

static inline void PerftDriver(Position* pos, int depth) {
	Move moves[256];
	const int num_moves = GenerateMoves(pos, moves, 0);
	for (int n = 0; n < num_moves; n++) {
		Position npos = *pos;
		if (!MakeMove(&npos, &moves[n]))
			continue;
		if (depth)
			PerftDriver(&npos, depth - 1);
		else
			info.nodes++;
	}
}

static int ShrinkNumber(U64 n) {
	if (n < 10000)
		return 0;
	if (n < 10000000)
		return 1;
	if (n < 10000000000)
		return 2;
	return 3;
}

static void PrintSummary(U64 time, U64 nodes) {
	U64 nps = (nodes * 1000) / max(time, 1);
	const char* units[] = { "", "k", "m", "g" };
	int sn = ShrinkNumber(nps);
	int p = pow(10, sn * 3);
	int b = pow(10, 3);
	printf("-----------------------------\n");
	printf("Time        : %llu\n", time);
	printf("Nodes       : %llu\n", nodes);
	printf("Nps         : %llu (%llu%s/s)\n", nps, nps / p, units[sn]);
	printf("-----------------------------\n");
}

static void PrintPerformanceHeader() {
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}

//performance test
static void UciPerformance(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	U64 elapsed = 0;
	while (elapsed < 4000) {
		info.nodes = 0;
		info.timeStart = TIME_MS();
		PerftDriver(pos, info.depthLimit++);
		elapsed = TIME_MS() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

//start benchmark
static void UciBench(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	info.post = FALSE;
	U64 elapsed = 0;
	while (elapsed < 8000) {
		++info.depthLimit;
		SearchIteratively(pos);
		elapsed = TIME_MS() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

static void ParsePosition(Position* pos, char* ptr) {
	char token[80], fen[80];
	ptr = ParseToken(ptr, token);
	if (strcmp(token, "fen") == 0) {
		fen[0] = '\0';
		while (1) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0' || strcmp(token, "moves") == 0)
				break;
			strcat(fen, token);
			strcat(fen, " ");
		}
		SetFen(pos, fen);
	}
	else {
		ptr = ParseToken(ptr, token);
		SetFen(pos, START_FEN);
	}
	historyCount = 0;
	if (strcmp(token, "moves") == 0) {
		while (1) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0')
				break;
			Move m = UciToMove(token, pos->flipped);
			//if (PieceTypeOnSquare(pos, m.to) != PT_NB || PieceTypeOnSquare(pos, m.from) == PAWN)
			if(!pos->move50)
				historyCount = 0;
			historyHash[historyCount++] = GetHash(pos);
			MakeMove(pos, &m);
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
	if (argument = strstr(command, "binc"))
		binc = atoi(argument + 5);
	if (argument = strstr(command, "winc"))
		winc = atoi(argument + 5);
	if (argument = strstr(command, "wtime"))
		wtime = max(1, atoi(argument + 6));
	if (argument = strstr(command, "btime"))
		btime = max(1, atoi(argument + 6));
	if ((argument = strstr(command, "movestogo")))
		movestogo = atoi(argument + 10);
	if ((argument = strstr(command, "movetime")))
		info.timeLimit = atoi(argument + 9);
	if ((argument = strstr(command, "depth")))
		info.depthLimit = atoi(argument + 6);
	if (argument = strstr(command, "nodes"))
		info.nodesLimit = atoi(argument + 5);
	int time = pos->flipped ? btime : wtime;
	int inc = pos->flipped ? binc : winc;
	if (time)
		info.timeLimit = max(1, min(time / movestogo + inc, time / 2));
	SearchIteratively(pos);
}

void UciCommand(Position* pos, char* line) {
	if (!strncmp(line, "ucinewgame", 10))HHClear();
	else if (!strncmp(line, "uci", 3)) {
		printf("id name %s\nuciok\n", NAME);
		fflush(stdout);
	}
	else if (!strncmp(line, "isready", 7)) {
		printf("readyok\n");
		fflush(stdout);
	}
	else if (!strncmp(line, "go", 2))ParseGo(pos, line + 2);
	else if (!strncmp(line, "position", 8))ParsePosition(pos, line + 8);
	else if (!strncmp(line, "print", 5))PrintBoard(pos);
	else if (!strncmp(line, "perft", 5))UciPerformance(pos);
	else if (!strncmp(line, "bench", 5))UciBench(pos);
}

static void UciLoop(Position* pos) {
	//PrintBitboard(0x60ULL);
	//PrintBitboard(0xEULL);
	//PrintBitboard(0x6000000000000000ULL);
	//PrintBitboard(0x0E00000000000000ULL);
	//UciCommand(pos, "position startpos moves d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 g1f3 c7c5 e2e3 e8g8 a2a3");
	//UciCommand(pos, "position fen 8/5Bn1/PPpkp3/2pppP1P/2RPP1P1/4r3/1N5b/Q2b3K b - - 9 96");
	//UciCommand(pos,"print");
	char line[4000];
	while (fgets(line, sizeof(line), stdin))
		UciCommand(pos, line);
}

static void Init() {
	for (int i = 0; i < 848; ++i)
		keys[i] = Rand64();
	for (int sq = 0; sq < 64; ++sq) {
		U64 bb = BIT(sq);
		bbSquare[sq] = bb;
		bbKnightAttack[sq] = KnightAttackBB(bb);
		bbKingAttack[sq] = KingAttackBB(bb);
	}
	for (int pt = PAWN; pt <= KING; pt++)
		for (int sq = 0; sq < 64; sq++) {
			int mg = mg_material[pt] + mg_table[pt][FLIP_SQ(sq)];
			int eg = eg_material[pt] + eg_table[pt][FLIP_SQ(sq)];
			int mx = max(mg, eg);
			mg_pst[pt][sq] = mg;
			eg_pst[pt][sq] = eg;
			mx_pst[pt][sq] = mx;
		}
	for (int depth = 1; depth < 64; depth++)
		for (int legal = 1; legal < 64; legal++)
			LINEAR_REDUCTION[depth][legal] = (int)(0.75 + log(depth) * log(legal) / 2.25);
}

static void InitMagics() {
	// initialize masks and tables
	memset(bishopAttacks, 0, sizeof(bishopAttacks));
	memset(rookAttacks, 0, sizeof(rookAttacks));
	for (int sq = 0; sq < 64; ++sq) {
		bishopMask[sq] = MaskBishopAttacks(sq);
		rookMask[sq] = MaskRookAttacks(sq);

		int bbits = bishop_relevant_bits[sq];
		int rbits = rook_relevant_bits[sq];

		size_t bsize = 1ULL << bbits;
		size_t rsize = 1ULL << rbits;

		// fill bishop attacks
		for (size_t index = 0; index < bsize; ++index) {
			U64 occ = SetOccupancy((int)index, bbits, bishopMask[sq]);
			U64 attack = BishopAttackBB(1ULL << sq, occ);
			U64 key = (occ * bishop_magic[sq]) >> (64 - bbits);
			bishopAttacks[sq][key] = attack;
		}
		// fill rook attacks
		for (size_t index = 0; index < rsize; ++index) {
			U64 occ = SetOccupancy((int)index, rbits, rookMask[sq]);
			U64 attack = RookAttackBB(1ULL << sq, occ);
			U64 key = (occ * rook_magic[sq]) >> (64 - rbits);
			rookAttacks[sq][key] = attack;
		}
	}
}

int main(const int argc, const char** argv) {
	Position pos;
	Init();
	InitMagics();
	printf("%s %s\n", NAME, VERSION);
	SetFen(&pos, START_FEN);
	UciLoop(&pos);
}