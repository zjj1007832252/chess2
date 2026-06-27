#ifndef AI_H
#define AI_H

#include "board.h"
#include <vector>

namespace chess {

// Difficulty presets tune search depth and randomness, giving a friendlier
// spread from "easy" (shallow + noisy) to "hard" (deeper, deterministic).
enum class Difficulty { Easy, Normal, Hard };

class AI
{
public:
    AI();

    void setDifficulty(Difficulty d);
    Difficulty difficulty() const { return m_diff; }

    // Choose a move for the side to move on `b`. Returns a legal move; if none
    // exist (game over), returns a default Move with fr==tr && fc==tc.
    Move chooseMove(const Board &b);

    // Set a fixed seed for the RNG (useful for reproducibility / testing).
    void setSeed(unsigned s);

private:
    // Negamax with alpha-beta. Returns score from the perspective of the side
    // to move. `depth` in plies. `pv` (optional) receives the best move at root.
    int negamax(Board &b, int depth, int alpha, int beta, Move *pv);

    // Static evaluation from the perspective of the side to move.
    int evaluate(const Board &b) const;

    // Material + positional value for a single piece at (r,c).
    int pieceValue(Piece p, int r, int c) const;

    Difficulty m_diff = Difficulty::Normal;
    int m_maxDepth = 4;     // search depth, set from difficulty
    int m_randomness = 0;   // top-N tolerance band for "Easy" variety
    unsigned m_rng = 0xC0FFEEu;

    int m_nodes = 0;        // nodes visited (for stats / time guards)
};

} // namespace chess
#endif // AI_H
