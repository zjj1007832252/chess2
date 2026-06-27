#include "ai.h"
#include <algorithm>
#include <climits>
#include <cstdint>

namespace chess {

// Base material values (centipawns). King value is large so that losing it
// dominates everything else.
static const int V_KING   = 10000;
static const int V_ROOK   = 900;
static const int V_KNIGHT = 400;
static const int V_CANNON = 450;
static const int V_ADVISOR= 200;
static const int V_BISHOP = 200;
static const int V_PAWN   = 100;

// Pawn positional bonus: advanced pawns are worth more, and crossing the river
// grants lateral mobility. Indexed by row from the owner's own side (0 = home).
static const int s_redPawnTable[10] = {
    /* r0 (enemy back rank) */  70, 90, 110, 130, 120, 130, 110, 90, 70
};
// We'll build the table programmatically for clarity below.

AI::AI() {}

void AI::setDifficulty(Difficulty d)
{
    m_diff = d;
    switch (d) {
    case Difficulty::Easy:   m_maxDepth = 2; m_randomness = 120; break;
    case Difficulty::Normal: m_maxDepth = 4; m_randomness = 25;  break;
    case Difficulty::Hard:   m_maxDepth = 5; m_randomness = 0;   break;
    }
}

void AI::setSeed(unsigned s) { m_rng = s ? s : 1; }

// xorshift32 — fast, deterministic PRNG.
static inline unsigned rnd(unsigned &state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Piece base value.
static int baseValue(Piece p)
{
    switch (p) {
    case Piece::RK: case Piece::BK: return V_KING;
    case Piece::RA: case Piece::BA: return V_ADVISOR;
    case Piece::RB: case Piece::BB: return V_BISHOP;
    case Piece::RN: case Piece::BN: return V_KNIGHT;
    case Piece::RR: case Piece::BR: return V_ROOK;
    case Piece::RC: case Piece::BC: return V_CANNON;
    case Piece::RP: case Piece::BP: return V_PAWN;
    default: return 0;
    }
}

// Pawn bonus table. For Red, advancing means decreasing r, so progress = 9 - r.
// For Black, progress = r. Crossed-river pawns (progress >= 5) get lateral value.
static int pawnBonus(Side s, int r, int c)
{
    int prog = (s == Side::Red) ? (9 - r) : r;
    int bonus = 0;
    if (prog >= 5) {                       // crossed the river
        bonus += 30;
        // central files are more valuable once across.
        int centerness = 4 - std::abs(c - 4);
        bonus += centerness * 6;
    }
    bonus += prog * 4;                     // slight push for advancement
    return bonus;
}

// Knight/cannon slight center bonus.
static int centralBonus(int r, int c)
{
    int rc = 4 - std::abs(r - 4);          // 0..4
    int cc = 4 - std::abs(c - 4);
    return rc + cc;                        // up to 8
}

int AI::pieceValue(Piece p, int r, int c) const
{
    int v = baseValue(p);
    Side s = sideOf(p);
    switch (p) {
    case Piece::RP: case Piece::BP: v += pawnBonus(s, r, c); break;
    case Piece::RN: case Piece::BN: v += centralBonus(r, c) * 2; break;
    case Piece::RC: case Piece::BC: v += centralBonus(r, c); break;
    default: break;
    }
    return v;
}

// Evaluate from the perspective of the side to move.
int AI::evaluate(const Board &b) const
{
    int score = 0;   // positive => good for Red
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 9; ++c) {
            Piece p = b.at(r, c);
            if (p == Piece::None) continue;
            int v = pieceValue(p, r, c);
            score += (sideOf(p) == Side::Red) ? v : -v;
        }
    }
    // Convert to side-to-move perspective.
    return (b.sideToMove() == Side::Red) ? score : -score;
}

// Order moves: captures first (MVV-LVA-ish), encouraging alpha-beta cutoffs.
static void orderMoves(std::vector<Move> &moves)
{
    std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
        return (a.captured != Piece::None) > (b.captured != Piece::None);
    });
}

int AI::negamax(Board &b, int depth, int alpha, int beta, Move *pv)
{
    ++m_nodes;
    if (depth <= 0) {
        return evaluate(b);
    }

    std::vector<Move> moves = b.generateMoves();
    orderMoves(moves);

    int best = INT_MIN + 1; // avoid overflow when negating
    bool any = false;

    for (const Move &m : moves) {
        Piece captured = b.at(m.tr, m.tc);
        Move rec = m;
        rec.captured = captured;        // remember for exact unmake
        b.makeMove(rec);
        b.toggleSide();

        int val;
        // If this move captures a king, the game is over in our favour.
        if (captured == Piece::RK || captured == Piece::BK) {
            val = V_KING + depth;       // prefer faster mates
        } else {
            // Check legality: skip moves that leave our own king in check.
            // (inCheck here is on the side that just moved.)
            Side justMoved = sideOf(b.at(m.tr, m.tc));
            if (b.inCheck(justMoved)) {
                b.toggleSide();
                b.unmakeMove(rec);
                continue;
            }
            val = -negamax(b, depth - 1, -beta, -alpha, nullptr);
        }

        b.toggleSide();
        b.unmakeMove(rec);

        any = true;
        if (val > best) {
            best = val;
            if (pv) *pv = rec;
        }
        if (val > alpha) alpha = val;
        if (alpha >= beta) break;        // beta cutoff
    }

    if (!any) {
        // No legal moves: checkmate or stalemate. Treat as a loss for the side
        // to move (Chinese chess has no stalemate draw that benefits a side).
        return -V_KING - depth;
    }
    return best;
}

Move AI::chooseMove(const Board &bInput)
{
    m_nodes = 0;
    Board b = bInput;
    std::vector<Move> legal = b.generateLegalMoves();
    if (legal.empty()) return Move{0, 0, 0, 0};

    orderMoves(legal);

    // Evaluate every root move, then pick the best (with optional noise).
    struct Scored { Move m; int score; };
    std::vector<Scored> scored;
    scored.reserve(legal.size());

    for (const Move &m : legal) {
        Move rec = m;
        rec.captured = b.at(m.tr, m.tc);
        b.makeMove(rec);
        b.toggleSide();

        int val;
        if (rec.captured == Piece::RK || rec.captured == Piece::BK) {
            val = V_KING;
        } else {
            val = -negamax(b, m_maxDepth - 1, INT_MIN + 1, INT_MAX - 1, nullptr);
        }

        b.toggleSide();
        b.unmakeMove(rec);

        if (m_randomness > 0)
            val += static_cast<int>(rnd(m_rng) % (unsigned)(m_randomness * 2 + 1)) - m_randomness;

        scored.push_back({rec, val});
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored &a, const Scored &b) { return a.score > b.score; });

    // On easier levels, occasionally pick among near-best moves for variety.
    int bestScore = scored.front().score;
    std::vector<Move> candidates;
    for (auto &s : scored)
        if (bestScore - s.score <= m_randomness) candidates.push_back(s.m);

    if (candidates.empty()) return scored.front().m;
    unsigned idx = rnd(m_rng) % (unsigned)candidates.size();
    return candidates[idx];
}

} // namespace chess
