#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <cstdint>
#include <string>
#include <cassert>

namespace chess {

// Piece type (Chinese chess: 帥/將, 仕/士, 相/象, 馬, 車, 炮/砲, 兵/卒)
enum class Piece : uint8_t {
    None = 0,
    // Red pieces (side == Red)
    RK, RA, RB, RN, RR, RC, RP, // 帥 仕 相 馬 車 炮 兵
    // Black pieces (side == Black)
    BK, BA, BB, BN, BR, BC, BP, // 將 士 象 馬 車 砲 卒
};

enum class Side : uint8_t { Red = 0, Black = 1 };

inline Side sideOf(Piece p)
{
    // RK..RP are indices 1..7 (Red), BK..BP are 8..14 (Black).
    return static_cast<Side>((static_cast<uint8_t>(p) - 1) / 7);
}

inline bool isRed(Piece p)  { return p != Piece::None && sideOf(p) == Side::Red; }
inline bool isBlack(Piece p){ return p != Piece::None && sideOf(p) == Side::Black; }

inline bool operator==(Piece a, Piece b){ return static_cast<uint8_t>(a) == static_cast<uint8_t>(b); }
inline bool operator!=(Piece a, Piece b){ return !(a == b); }

// A move from (fr,fc) to (tr,tc). `captured` holds the piece that was on the
// destination (Piece::None if the move is quiet). Stored so make/unmake is exact.
struct Move {
    int8_t fr = 0, fc = 0, tr = 0, tc = 0;
    Piece captured = Piece::None;
};

// 9 columns (file) x 10 rows (rank). board[r][c].
// Red side starts at rows 0..4 (top of internal array when rendered for Red host),
// Black side at rows 5..9. We keep Red as the "bottom" by convention in rendering,
// but the model is orientation-agnostic: side to move just alternates.
class Board
{
public:
    Board();
    void reset();

    Piece at(int r, int c) const { assert(inBoard(r, c)); return m_b[r][c]; }
    void set(int r, int c, Piece p) { assert(inBoard(r, c)); m_b[r][c] = p; }

    Side sideToMove() const { return m_side; }
    void setSideToMove(Side s) { m_side = s; }
    void toggleSide() { m_side = (m_side == Side::Red ? Side::Black : Side::Red); }

    // Generate all pseudo-legal moves for the side to move.
    std::vector<Move> generateMoves() const;
    // Generate only moves that leave our own king safe (legal).
    std::vector<Move> generateLegalMoves() const;

    // Apply a move. Does NOT verify legality; caller should ensure it is legal.
    void makeMove(const Move &m);
    // Undo a move previously applied (restores captured piece and side).
    void unmakeMove(const Move &m);

    // Is the given side's king currently in check?
    bool inCheck(Side s) const;
    // Is the side to move checkmated? (in check and no legal moves)
    bool isCheckmate() const;
    // Is the side to move stalemated? (no legal moves, not in check)
    bool isStalemate() const;

    // Locate the king of side s. Returns false if not found.
    bool findKing(Side s, int &kr, int &kc) const;

    // True if (r,c) is inside the board.
    static bool inBoard(int r, int c);

    // For the human/AI flow: try to make a move from (fr,fc) to (tr,tc).
    // Returns true and fills `out` if the move is legal (and applies it).
    bool tryMove(int fr, int fc, int tr, int tc, Move &out);

    // For the AI: count how many moves from the given square are pseudo-legal
    // (used as a lightweight mobility proxy elsewhere). Kept minimal here.
    void generateMovesFrom(int r, int c, std::vector<Move> &out) const;

private:
    // Helpers per piece type. `out` collects pseudo-legal moves (king-safety not checked).
    void genKing(int r, int c, std::vector<Move> &out) const;
    void genAdvisor(int r, int c, std::vector<Move> &out) const;
    void genBishop(int r, int c, std::vector<Move> &out) const;
    void genKnight(int r, int c, std::vector<Move> &out) const;
    void genRook(int r, int c, std::vector<Move> &out) const;
    void genCannon(int r, int c, std::vector<Move> &out) const;
    void genPawn(int r, int c, std::vector<Move> &out) const;

    // Does moving the piece at (fr,fc) to (tr,tc) leave our king safe?
    // Non-const version: temporarily mutates board (faster, used by tryMove).
    bool leavesKingSafe(int fr, int fc, int tr, int tc);
    // Const version: uses board copy (used by generateLegalMoves).
    bool isKingSafeAfterMove(int fr, int fc, int tr, int tc) const;

    Side m_side = Side::Red;
    Piece m_b[10][9];
};

} // namespace chess
#endif // BOARD_H
