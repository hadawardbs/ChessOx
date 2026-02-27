#ifndef OXTA_CORE_SEE_HPP
#define OXTA_CORE_SEE_HPP

#include "position.hpp"
#include "moves.hpp"
#include "types.hpp"
#include "bitboard.hpp"
#include "attacks.hpp"
#include <algorithm>

namespace Oxta {

// Piece Values for SEE (approximate)
// None, Pawn, Knight, Bishop, Rook, Queen, King
constexpr int SeeValues[] = { 0, 100, 320, 330, 500, 900, 20000, 0 };

inline PieceType type_of_piece(Piece p) {
    return static_cast<PieceType>(p & 7);
}

class SEE {
public:
    static bool see_ge(const Position& pos, Move move, int threshold = 0) {
        if (move.is_castle()) return 0 >= threshold; // Castling is usually neutral or good? Assume 0.

        Square from = move.from();
        Square to = move.to();

        // Handle En Passant
        Piece captured = (move.is_en_passant()) ? ((pos.side_to_move() == WHITE) ? B_PAWN : W_PAWN) : pos.piece_on(to);

        // Initial Gain: Value of captured piece
        int gain[32];
        int d = 0;

        gain[d] = (captured != NO_PIECE) ? SeeValues[type_of_piece(captured)] : 0;

        PieceType attacker_type = type_of_piece(pos.piece_on(from));

        // Handle Promotion for the first attacker
        if (move.is_promotion()) {
            attacker_type = move.promotion_type();
        }

        Bitboard occ = pos.pieces();

        // Make the first move on the board (virtually)
        // Remove attacker from 'from'
        Bitboards::clear_bit(occ, from);
        // Add attacker to 'to' (implicitly handled by occ updates later? No, we just need to know the square is occupied or not for sliders)
        // Actually, the piece moves to 'to'. So 'to' is occupied by the attacker now.
        // But for ray attacks, we pass 'occ'.
        Bitboards::set_bit(occ, to);

        // But wait, if we clear 'from' and set 'to', we update occ correctly for the NEXT attacker.
        // The first attacker is now at 'to'.

        // En Passant special case: Remove the pawn behind
        if (move.is_en_passant()) {
             Bitboards::clear_bit(occ, shift(to, (pos.side_to_move() == WHITE ? SOUTH : NORTH)));
        }

        Color side = ~pos.side_to_move(); // Next side to capture

        // Iterate captures
        while (true) {
            d++;
            gain[d] = SeeValues[attacker_type] - gain[d-1]; // Score is Value(Victim) - Value(Sequence)

            // Pruning: If score is already so bad that opponent won't capture, we can stop?
            // Standard SEE usually just fills the array.
            if (std::max(-gain[d-1], gain[d]) < threshold) {
                 // Optimization possible here
            }

            // Find least valuable attacker
            Square attacker_sq = get_least_valuable_attacker(pos, to, occ, side, attacker_type);

            if (attacker_sq == SQ_NONE) break;

            // Remove attacker
            Bitboards::clear_bit(occ, attacker_sq);
            // The attacker moves to 'to'. 'to' remains occupied.
            // (We already set 'to' bit).

            side = ~side;
        }

        // Propagate Minimax
        while (--d > 0) {
            gain[d-1] = -std::max(-gain[d-1], gain[d]);
        }

        return gain[0] >= threshold;
    }

private:
    static Square get_least_valuable_attacker(const Position& pos, Square to, Bitboard occ, Color stm, PieceType& type) {
        // Pawns
        // Must mask with 'occ' to ensure we don't reuse pieces that were "captured/moved" in the SEE sequence
        Bitboard pawns = pos.pieces(PAWN, stm) & occ;
        Bitboard attackers = pawns & Attacks::pawn_attacks(to, ~stm);
        if (attackers) {
            type = PAWN;
            return Bitboards::lsb(attackers);
        }

        // Knights
        Bitboard knights = pos.pieces(KNIGHT, stm) & occ;
        attackers = knights & Attacks::knight_attacks(to);
        if (attackers) {
            type = KNIGHT;
            return Bitboards::lsb(attackers);
        }

        // Bishops
        Bitboard bishops = pos.pieces(BISHOP, stm) & occ;
        Bitboard b_att = Attacks::bishop_attacks(to, occ);
        attackers = bishops & b_att;
        if (attackers) {
            type = BISHOP;
            return Bitboards::lsb(attackers);
        }

        // Rooks
        Bitboard rooks = pos.pieces(ROOK, stm) & occ;
        Bitboard r_att = Attacks::rook_attacks(to, occ);
        attackers = rooks & r_att;
        if (attackers) {
            type = ROOK;
            return Bitboards::lsb(attackers);
        }

        // Queens
        Bitboard queens = pos.pieces(QUEEN, stm) & occ;
        attackers = queens & (b_att | r_att);
        if (attackers) {
            type = QUEEN;
            return Bitboards::lsb(attackers);
        }

        // King
        Bitboard king = pos.pieces(KING, stm) & occ;
        attackers = king & Attacks::king_attacks(to);
        if (attackers) {
            type = KING;
            return Bitboards::lsb(attackers);
        }

        return SQ_NONE;
    }
};

} // namespace Oxta

#endif // OXTA_CORE_SEE_HPP
