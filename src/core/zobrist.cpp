#include "zobrist.hpp"

namespace Oxta::Zobrist {

Bitboard psq[PIECE_NB][SQUARE_NB];
Bitboard en_passant[FILE_NB + 1];
Bitboard castling[16];
Bitboard side_to_move;

} // namespace Oxta::Zobrist
