#include "board.h"
#include <algorithm>

namespace chess {

Board::Board() { reset(); }

void Board::reset()
{
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 9; ++c)
            m_b[r][c] = Piece::None;

    // Black back rank (row 0): 車 馬 象 士 將 士 象 馬 車
    m_b[0][0] = Piece::BR; m_b[0][1] = Piece::BN; m_b[0][2] = Piece::BB;
    m_b[0][3] = Piece::BA; m_b[0][4] = Piece::BK; m_b[0][5] = Piece::BA;
    m_b[0][6] = Piece::BB; m_b[0][7] = Piece::BN; m_b[0][8] = Piece::BR;
    m_b[2][1] = Piece::BC; m_b[2][7] = Piece::BC;                 // cannons
    m_b[3][0] = Piece::BP; m_b[3][2] = Piece::BP; m_b[3][4] = Piece::BP;
    m_b[3][6] = Piece::BP; m_b[3][8] = Piece::BP;                 // pawns

    // Red back rank (row 9): 車 馬 相 仕 帥 仕 相 馬 車
    m_b[9][0] = Piece::RR; m_b[9][1] = Piece::RN; m_b[9][2] = Piece::RB;
    m_b[9][3] = Piece::RA; m_b[9][4] = Piece::RK; m_b[9][5] = Piece::RA;
    m_b[9][6] = Piece::RB; m_b[9][7] = Piece::RN; m_b[9][8] = Piece::RR;
    m_b[7][1] = Piece::RC; m_b[7][7] = Piece::RC;
    m_b[6][0] = Piece::RP; m_b[6][2] = Piece::RP; m_b[6][4] = Piece::RP;
    m_b[6][6] = Piece::RP; m_b[6][8] = Piece::RP;

    m_side = Side::Red;
}

bool Board::inBoard(int r, int c) { return r >= 0 && r < 10 && c >= 0 && c < 9; }

// ---- per-piece pseudo-legal generators ----

void Board::generateMovesFrom(int r, int c, std::vector<Move> &out) const
{
    Piece p = m_b[r][c];
    switch (p) {
    case Piece::RK: case Piece::BK: genKing(r, c, out); break;
    case Piece::RA: case Piece::BA: genAdvisor(r, c, out); break;
    case Piece::RB: case Piece::BB: genBishop(r, c, out); break;
    case Piece::RN: case Piece::BN: genKnight(r, c, out); break;
    case Piece::RR: case Piece::BR: genRook(r, c, out); break;
    case Piece::RC: case Piece::BC: genCannon(r, c, out); break;
    case Piece::RP: case Piece::BP: genPawn(r, c, out); break;
    default: break;
    }
}

// Helper to add a move if the destination is empty or holds an enemy piece.
static inline void addMove(int r, int c, int tr, int tc,
                           Piece src, const Piece b[10][9], std::vector<Move> &out)
{
    Piece tp = b[tr][tc];
    if (tp != Piece::None && sideOf(tp) == sideOf(src)) return; // own piece blocks
    out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, tp});
}

void Board::genKing(int r, int c, std::vector<Move> &out) const
{
    Side me = sideOf(m_b[r][c]);
    int rMin = (me == Side::Red) ? 7 : 0;
    int rMax = (me == Side::Red) ? 9 : 2;
    static const int dr[4] = {1, -1, 0, 0};
    static const int dc[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; ++i) {
        int tr = r + dr[i], tc = c + dc[i];
        if (!inBoard(tr, tc) || tc < 3 || tc > 5) continue;
        if (tr < rMin || tr > rMax) continue;
        addMove(r, c, tr, tc, m_b[r][c], m_b, out);
    }
}

void Board::genAdvisor(int r, int c, std::vector<Move> &out) const
{
    Side me = sideOf(m_b[r][c]);
    int rMin = (me == Side::Red) ? 7 : 0;
    int rMax = (me == Side::Red) ? 9 : 2;
    static const int dr[4] = {1, 1, -1, -1};
    static const int dc[4] = {1, -1, 1, -1};
    for (int i = 0; i < 4; ++i) {
        int tr = r + dr[i], tc = c + dc[i];
        if (!inBoard(tr, tc) || tc < 3 || tc > 5) continue;
        if (tr < rMin || tr > rMax) continue;
        addMove(r, c, tr, tc, m_b[r][c], m_b, out);
    }
}

void Board::genBishop(int r, int c, std::vector<Move> &out) const
{
    Side me = sideOf(m_b[r][c]);
    // Cannot cross the river: Red rows 5..9, Black rows 0..4.
    int rMin = (me == Side::Red) ? 5 : 0;
    int rMax = (me == Side::Red) ? 9 : 4;
    static const int dr[4] = {2, 2, -2, -2};
    static const int dc[4] = {2, -2, 2, -2};
    static const int er[4] = {1, 1, -1, -1};   // "elephant eye"
    static const int ec[4] = {1, -1, 1, -1};
    for (int i = 0; i < 4; ++i) {
        int tr = r + dr[i], tc = c + dc[i];
        if (!inBoard(tr, tc) || tr < rMin || tr > rMax) continue;
        if (m_b[r + er[i]][c + ec[i]] != Piece::None) continue; // eye blocked
        addMove(r, c, tr, tc, m_b[r][c], m_b, out);
    }
}

void Board::genKnight(int r, int c, std::vector<Move> &out) const
{
    // 8 L-moves; the "leg" square must be empty (蹩马腿).
    static const int dr[8] = {-2, -2, 2, 2, -1, 1, -1, 1};
    static const int dc[8] = {-1, 1, -1, 1, -2, -2, 2, 2};
    static const int lr[8] = {-1, -1, 1, 1, 0, 0, 0, 0};
    static const int lc[8] = {0, 0, 0, 0, -1, -1, 1, 1};
    for (int i = 0; i < 8; ++i) {
        int tr = r + dr[i], tc = c + dc[i];
        if (!inBoard(tr, tc)) continue;
        if (m_b[r + lr[i]][c + lc[i]] != Piece::None) continue;
        addMove(r, c, tr, tc, m_b[r][c], m_b, out);
    }
}

void Board::genRook(int r, int c, std::vector<Move> &out) const
{
    static const int dr[4] = {1, -1, 0, 0};
    static const int dc[4] = {0, 0, 1, -1};
    Piece src = m_b[r][c];
    for (int i = 0; i < 4; ++i) {
        int tr = r + dr[i], tc = c + dc[i];
        while (inBoard(tr, tc)) {
            Piece tp = m_b[tr][tc];
            if (tp == Piece::None) {
                out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, Piece::None});
            } else {
                if (sideOf(tp) != sideOf(src))
                    out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, tp});
                break;
            }
            tr += dr[i]; tc += dc[i];
        }
    }
}

void Board::genCannon(int r, int c, std::vector<Move> &out) const
{
    static const int dr[4] = {1, -1, 0, 0};
    static const int dc[4] = {0, 0, 1, -1};
    Piece src = m_b[r][c];
    for (int i = 0; i < 4; ++i) {
        int tr = r + dr[i], tc = c + dc[i];
        // Slide over empty squares (quiet moves).
        while (inBoard(tr, tc) && m_b[tr][tc] == Piece::None) {
            out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, Piece::None});
            tr += dr[i]; tc += dc[i];
        }
        if (!inBoard(tr, tc)) continue;       // no screen found
        // tr,tc is the screen; jump over it and look for an enemy to capture.
        tr += dr[i]; tc += dc[i];
        while (inBoard(tr, tc)) {
            Piece tp = m_b[tr][tc];
            if (tp != Piece::None) {
                if (sideOf(tp) != sideOf(src))
                    out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, tp});
                break;
            }
            tr += dr[i]; tc += dc[i];
        }
    }
}

void Board::genPawn(int r, int c, std::vector<Move> &out) const
{
    Side me = sideOf(m_b[r][c]);
    // Red advances up (r decreases); Black advances down (r increases).
    int fwd = (me == Side::Red) ? -1 : 1;
    bool crossed = (me == Side::Red) ? (r <= 4) : (r >= 5);

    int tr = r + fwd, tc = c;
    if (inBoard(tr, tc)) {
        Piece tp = m_b[tr][tc];
        if (tp == Piece::None || sideOf(tp) != me)
            out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, tp});
    }
    if (crossed) {
        for (int dc : {-1, 1}) {
            tr = r; tc = c + dc;
            if (!inBoard(tr, tc)) continue;
            Piece tp = m_b[tr][tc];
            if (tp == Piece::None || sideOf(tp) != me)
                out.push_back({(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, tp});
        }
    }
}

// ---- move list assembly ----

std::vector<Move> Board::generateMoves() const
{
    std::vector<Move> moves;
    moves.reserve(48);
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 9; ++c) {
            Piece p = m_b[r][c];
            if (p == Piece::None || sideOf(p) != m_side) continue;
            generateMovesFrom(r, c, moves);
        }
    return moves;
}

// ---- king safety ----

bool Board::findKing(Side s, int &kr, int &kc) const
{
    Piece k = (s == Side::Red) ? Piece::RK : Piece::BK;
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 9; ++c)
            if (m_b[r][c] == k) { kr = r; kc = c; return true; }
    return false;
}

bool Board::inCheck(Side s) const
{
    int kr, kc;
    if (!findKing(s, kr, kc)) return true; // missing king => treated as captured

    Side opp = (s == Side::Red) ? Side::Black : Side::Red;

    // Flying general: kings may not face each other on an open file.
    int okr, okc;
    if (findKing(opp, okr, okc) && okc == kc) {
        bool blocked = false;
        int lo = std::min(kr, okr) + 1, hi = std::max(kr, okr) - 1;
        for (int r = lo; r <= hi; ++r)
            if (m_b[r][kc] != Piece::None) { blocked = true; break; }
        if (!blocked) return true;
    }

    // Can any enemy move land on our king? (pseudo-legal; no recursion into safety)
    std::vector<Move> ms;
    ms.reserve(16);
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 9; ++c) {
            Piece p = m_b[r][c];
            if (p == Piece::None || sideOf(p) != opp) continue;
            ms.clear();
            generateMovesFrom(r, c, ms);
            for (auto &m : ms)
                if (m.tr == kr && m.tc == kc) return true;
        }
    }
    return false;
}

bool Board::leavesKingSafe(int fr, int fc, int tr, int tc)
{
    Piece mover = m_b[fr][fc];
    Piece captured = m_b[tr][tc];
    m_b[tr][tc] = mover;
    m_b[fr][fc] = Piece::None;
    bool safe = !inCheck(sideOf(mover));
    m_b[fr][fc] = mover;
    m_b[tr][tc] = captured;
    return safe;
}

bool Board::isKingSafeAfterMove(int fr, int fc, int tr, int tc) const
{
    Board copy = *this;
    Piece mover = copy.m_b[fr][fc];
    copy.m_b[tr][tc] = mover;
    copy.m_b[fr][fc] = Piece::None;
    return !copy.inCheck(sideOf(mover));
}

std::vector<Move> Board::generateLegalMoves() const
{
    std::vector<Move> result;
    std::vector<Move> pseudo = generateMoves();
    result.reserve(pseudo.size());
    for (auto &m : pseudo)
        if (isKingSafeAfterMove(m.fr, m.fc, m.tr, m.tc))
            result.push_back(m);
    return result;
}

// ---- make / unmake ----

void Board::makeMove(const Move &m)
{
    m_b[m.tr][m.tc] = m_b[m.fr][m.fc];
    m_b[m.fr][m.fc] = Piece::None;
    // Side toggling is intentionally NOT done here: callers (tryMove, AI search)
    // manage sideToMove so they can batch make/unmake consistently.
}

void Board::unmakeMove(const Move &m)
{
    m_b[m.fr][m.fc] = m_b[m.tr][m.tc];
    m_b[m.tr][m.tc] = m.captured; // restore the previously captured piece
}

bool Board::tryMove(int fr, int fc, int tr, int tc, Move &out)
{
    if (!inBoard(fr, fc) || !inBoard(tr, tc)) return false;
    Piece p = m_b[fr][fc];
    if (p == Piece::None || sideOf(p) != m_side) return false;

    std::vector<Move> ms;
    generateMovesFrom(fr, fc, ms);
    const Move *match = nullptr;
    for (auto &m : ms)
        if (m.tr == tr && m.tc == tc) { match = &m; break; }
    if (!match) return false;
    if (!leavesKingSafe(fr, fc, tr, tc)) return false;

    out = *match;
    makeMove(out);
    toggleSide();
    return true;
}

bool Board::isCheckmate() const
{
    if (!inCheck(m_side)) return false;
    return generateLegalMoves().empty();
}

bool Board::isStalemate() const
{
    if (inCheck(m_side)) return false;
    return generateLegalMoves().empty();
}

} // namespace chess
