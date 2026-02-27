#ifndef OXTA_CORE_MOVEGEN_HPP
#define OXTA_CORE_MOVEGEN_HPP

#include "position.hpp"
#include "moves.hpp"

namespace Oxta::MoveGen {

// Generate pseudo-legal moves
// Returns a filled MoveList.
// Does NOT filter for legality (King in check after move).
// That is usually done by 'is_valid' or checking 'attackers_to(king)' after make_move.
// But for PERFT, a standard approach is: Generate All -> Make -> If Illegal Unmake & Continue.
MoveList generate_all(const Position& pos);
MoveList generate_captures(const Position& pos);

} // namespace Oxta::MoveGen

#endif // OXTA_CORE_MOVEGEN_HPP
