#ifndef OXTA_CORE_ATTACKS_HPP
#define OXTA_CORE_ATTACKS_HPP

#include "types.hpp"
#include "bitboard.hpp"

namespace Oxta::Attacks {

// Initialize all attack tables
void init();

// Leaper attacks
Bitboard pawn_attacks(Square s, Color c);
Bitboard knight_attacks(Square s);
Bitboard king_attacks(Square s);

// Slider attacks (using PEXT)
Bitboard bishop_attacks(Square s, Bitboard occupancy);
Bitboard rook_attacks(Square s, Bitboard occupancy);
Bitboard queen_attacks(Square s, Bitboard occupancy);

} // namespace Oxta::Attacks

#endif // OXTA_CORE_ATTACKS_HPP
