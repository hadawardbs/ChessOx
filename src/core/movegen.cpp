#include "movegen.hpp"
#include "attacks.hpp"
#include "bitboard.hpp"

namespace Oxta::MoveGen {

// Helper: Check if a square is attacked by a specific side
bool is_square_attacked(const Position& pos, Square s, Color by_side) {
    Bitboard occ = pos.pieces();

    // Pawns
    // If we want to know if 's' is attacked by 'by_side' pawns:
    // We pretend there is a pawn of '~by_side' at 's', and see if it attacks any 'by_side' pawn.
    if (Attacks::pawn_attacks(s, ~by_side) & pos.pieces(PAWN, by_side)) return true;

    // Knights
    if (Attacks::knight_attacks(s) & pos.pieces(KNIGHT, by_side)) return true;

    // Kings
    if (Attacks::king_attacks(s) & pos.pieces(KING, by_side)) return true;

    // Bishops/Queens
    Bitboard bq = pos.pieces(BISHOP, by_side) | pos.pieces(QUEEN, by_side);
    if (bq && (Attacks::bishop_attacks(s, occ) & bq)) return true;

    // Rooks/Queens
    Bitboard rq = pos.pieces(ROOK, by_side) | pos.pieces(QUEEN, by_side);
    if (rq && (Attacks::rook_attacks(s, occ) & rq)) return true;

    return false;
}

template<Color Us>
void generate_pawn_moves(const Position& pos, MoveList& list) {
    constexpr Color Them = ~Us;
    constexpr Direction Up = (Us == WHITE ? NORTH : SOUTH);
    constexpr Direction UpRight = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    Bitboard pawns = pos.pieces(Us == WHITE ? W_PAWN : B_PAWN);
    Bitboard empty = ~pos.pieces();
    Bitboard enemies = pos.pieces(Them);

    // Single Push
    Bitboard single_push = (Us == WHITE ? (pawns << 8) : (pawns >> 8)) & empty;

    Bitboard b = single_push;
    while (b) {
        Square to = Bitboards::pop_lsb(b);
        Square from = shift(to, -Up);
        if ((Us == WHITE && to >= SQ_A8) || (Us == BLACK && to <= SQ_H1)) {
            list.add(Move(from, to, Move::PROMO_Q));
            list.add(Move(from, to, Move::PROMO_R));
            list.add(Move(from, to, Move::PROMO_B));
            list.add(Move(from, to, Move::PROMO_N));
        } else {
            list.add(Move(from, to, Move::QUIET));
            if ((Us == WHITE && (from >= SQ_A2 && from <= SQ_H2)) ||
                (Us == BLACK && (from >= SQ_A7 && from <= SQ_H7))) {
                Square push_to = shift(to, Up);
                if (Bitboards::check_bit(empty, push_to)) {
                    list.add(Move(from, push_to, Move::DOUBLE_PUSH));
                }
            }
        }
    }

    auto gen_captures = [&](Direction dir) {
        Bitboard caps = 0;
        if (Us == WHITE) {
             if (dir == NORTH_WEST) caps = (pawns << 7) & ~0x8080808080808080ULL;
             else caps = (pawns << 9) & ~0x0101010101010101ULL;
        } else {
             if (dir == SOUTH_EAST) caps = (pawns >> 7) & ~0x0101010101010101ULL;
             else caps = (pawns >> 9) & ~0x8080808080808080ULL;
        }

        if (pos.ep_square() != SQ_NONE) {
             Bitboard ep_mask = (1ULL << pos.ep_square());
             Bitboard ep_hits = caps & ep_mask;
             while (ep_hits) {
                 Square to = Bitboards::pop_lsb(ep_hits);
                 Square from = shift(to, -dir);
                 list.add(Move(from, to, Move::EP_CAPTURE));
             }
        }

        caps &= enemies;
        while (caps) {
            Square to = Bitboards::pop_lsb(caps);
            Square from = shift(to, -dir);
             if ((Us == WHITE && to >= SQ_A8) || (Us == BLACK && to <= SQ_H1)) {
                list.add(Move(from, to, Move::PROMO_CAPTURE_Q));
                list.add(Move(from, to, Move::PROMO_CAPTURE_R));
                list.add(Move(from, to, Move::PROMO_CAPTURE_B));
                list.add(Move(from, to, Move::PROMO_CAPTURE_N));
            } else {
                list.add(Move(from, to, Move::CAPTURE));
            }
        }
    };

    gen_captures(UpLeft);
    gen_captures(UpRight);
}

MoveList generate_all(const Position& pos) {
    MoveList list;
    Color us = pos.side_to_move();
    Color them = ~us;
    Bitboard us_pieces = pos.pieces(us);
    Bitboard them_pieces = pos.pieces(them);
    Bitboard all_pieces = us_pieces | them_pieces;

    if (us == WHITE) generate_pawn_moves<WHITE>(pos, list);
    else generate_pawn_moves<BLACK>(pos, list);

    Bitboard knights = pos.pieces(us == WHITE ? W_KNIGHT : B_KNIGHT);
    while (knights) {
        Square from = Bitboards::pop_lsb(knights);
        Bitboard att = Attacks::knight_attacks(from) & ~us_pieces;
        while (att) {
            Square to = Bitboards::pop_lsb(att);
            if (Bitboards::check_bit(them_pieces, to)) list.add(Move(from, to, Move::CAPTURE));
            else list.add(Move(from, to, Move::QUIET));
        }
    }

    Bitboard king = pos.pieces(us == WHITE ? W_KING : B_KING);
    if (king) {
        Square from = Bitboards::lsb(king);
        Bitboard att = Attacks::king_attacks(from) & ~us_pieces;
        while (att) {
            Square to = Bitboards::pop_lsb(att);
            if (Bitboards::check_bit(them_pieces, to)) list.add(Move(from, to, Move::CAPTURE));
            else list.add(Move(from, to, Move::QUIET));
        }
    }

    Bitboard bishops = pos.pieces(us == WHITE ? W_BISHOP : B_BISHOP) | pos.pieces(us == WHITE ? W_QUEEN : B_QUEEN);
    while (bishops) {
        Square from = Bitboards::pop_lsb(bishops);
        Bitboard att = Attacks::bishop_attacks(from, all_pieces) & ~us_pieces;
        while (att) {
            Square to = Bitboards::pop_lsb(att);
             if (Bitboards::check_bit(them_pieces, to)) list.add(Move(from, to, Move::CAPTURE));
            else list.add(Move(from, to, Move::QUIET));
        }
    }

    Bitboard rooks = pos.pieces(us == WHITE ? W_ROOK : B_ROOK) | pos.pieces(us == WHITE ? W_QUEEN : B_QUEEN);
    while (rooks) {
        Square from = Bitboards::pop_lsb(rooks);
        Bitboard att = Attacks::rook_attacks(from, all_pieces) & ~us_pieces;
        while (att) {
            Square to = Bitboards::pop_lsb(att);
             if (Bitboards::check_bit(them_pieces, to)) list.add(Move(from, to, Move::CAPTURE));
            else list.add(Move(from, to, Move::QUIET));
        }
    }

    // Castling with SAFETY CHECKS
    int cr = pos.castling_rights();
    if (us == WHITE) {
        // King Side (E1 -> G1)
        if ((cr & 1) &&
            !Bitboards::check_bit(all_pieces, SQ_F1) &&
            !Bitboards::check_bit(all_pieces, SQ_G1)) {

            // Check if E1, F1, G1 are attacked
            if (!is_square_attacked(pos, SQ_E1, them) &&
                !is_square_attacked(pos, SQ_F1, them) &&
                !is_square_attacked(pos, SQ_G1, them)) {
                list.add(Move(SQ_E1, SQ_G1, Move::CASTLE_KS));
            }
        }
        // Queen Side (E1 -> C1)
        if ((cr & 2) &&
            !Bitboards::check_bit(all_pieces, SQ_D1) &&
            !Bitboards::check_bit(all_pieces, SQ_C1) &&
            !Bitboards::check_bit(all_pieces, SQ_B1)) {

            if (!is_square_attacked(pos, SQ_E1, them) &&
                !is_square_attacked(pos, SQ_D1, them) &&
                !is_square_attacked(pos, SQ_C1, them)) {
                list.add(Move(SQ_E1, SQ_C1, Move::CASTLE_QS));
            }
        }
    } else {
        // King Side (E8 -> G8)
        if ((cr & 4) &&
            !Bitboards::check_bit(all_pieces, SQ_F8) &&
            !Bitboards::check_bit(all_pieces, SQ_G8)) {

            if (!is_square_attacked(pos, SQ_E8, them) &&
                !is_square_attacked(pos, SQ_F8, them) &&
                !is_square_attacked(pos, SQ_G8, them)) {
                list.add(Move(SQ_E8, SQ_G8, Move::CASTLE_KS));
            }
        }
        // Queen Side (E8 -> C8)
        if ((cr & 8) &&
            !Bitboards::check_bit(all_pieces, SQ_D8) &&
            !Bitboards::check_bit(all_pieces, SQ_C8) &&
            !Bitboards::check_bit(all_pieces, SQ_B8)) {

            if (!is_square_attacked(pos, SQ_E8, them) &&
                !is_square_attacked(pos, SQ_D8, them) &&
                !is_square_attacked(pos, SQ_C8, them)) {
                list.add(Move(SQ_E8, SQ_C8, Move::CASTLE_QS));
            }
        }
    }

    return list;
}

MoveList generate_captures(const Position& pos) {
    MoveList list;
    Color us = pos.side_to_move();
    Color them = ~us;
    Bitboard us_pieces = pos.pieces(us);
    Bitboard them_pieces = pos.pieces(them);
    Bitboard all_pieces = us_pieces | them_pieces;

    // Pawn Captures (and promotions)
    constexpr Color Us = WHITE; // Template logic would be better but let's keep it simple
    auto gen_pawn_caps = [&](auto color_tag) {
        constexpr Color U = (decltype(color_tag)::value == WHITE ? WHITE : BLACK);
        constexpr Direction UpRight = (U == WHITE ? NORTH_EAST : SOUTH_WEST);
        constexpr Direction UpLeft = (U == WHITE ? NORTH_WEST : SOUTH_EAST);
        Bitboard pawns = pos.pieces(U == WHITE ? W_PAWN : B_PAWN);
        
        auto sub_gen = [&](Direction dir) {
            Bitboard caps = 0;
            if (U == WHITE) {
                if (dir == NORTH_WEST) caps = (pawns << 7) & ~0x8080808080808080ULL;
                else caps = (pawns << 9) & ~0x0101010101010101ULL;
            } else {
                if (dir == SOUTH_EAST) caps = (pawns >> 7) & ~0x0101010101010101ULL;
                else caps = (pawns >> 9) & ~0x8080808080808080ULL;
            }
            Bitboard ep_hits = (pos.ep_square() != SQ_NONE) ? (caps & (1ULL << pos.ep_square())) : 0;
            while (ep_hits) {
                Square to = Bitboards::pop_lsb(ep_hits);
                list.add(Move(shift(to, -dir), to, Move::EP_CAPTURE));
            }
            caps &= them_pieces;
            while (caps) {
                Square to = Bitboards::pop_lsb(caps);
                Square from = shift(to, -dir);
                if ((U == WHITE && to >= SQ_A8) || (U == BLACK && to <= SQ_H1)) {
                    list.add(Move(from, to, Move::PROMO_CAPTURE_Q));
                    list.add(Move(from, to, Move::PROMO_CAPTURE_R));
                    list.add(Move(from, to, Move::PROMO_CAPTURE_B));
                    list.add(Move(from, to, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(from, to, Move::CAPTURE));
                }
            }
        };
        sub_gen(UpLeft);
        sub_gen(UpRight);
    };

    if (us == WHITE) gen_pawn_caps(std::integral_constant<Color, WHITE>{});
    else gen_pawn_caps(std::integral_constant<Color, BLACK>{});

    // Knights
    Bitboard knights = pos.pieces(us == WHITE ? W_KNIGHT : B_KNIGHT);
    while (knights) {
        Square from = Bitboards::pop_lsb(knights);
        Bitboard att = Attacks::knight_attacks(from) & them_pieces;
        while (att) list.add(Move(from, Bitboards::pop_lsb(att), Move::CAPTURE));
    }

    // Bishops / Queens
    Bitboard bishops = pos.pieces(us == WHITE ? W_BISHOP : B_BISHOP) | pos.pieces(us == WHITE ? W_QUEEN : B_QUEEN);
    while (bishops) {
        Square from = Bitboards::pop_lsb(bishops);
        Bitboard att = Attacks::bishop_attacks(from, all_pieces) & them_pieces;
        while (att) list.add(Move(from, Bitboards::pop_lsb(att), Move::CAPTURE));
    }

    // Rooks / Queens
    Bitboard rooks = pos.pieces(us == WHITE ? W_ROOK : B_ROOK) | pos.pieces(us == WHITE ? W_QUEEN : B_QUEEN);
    while (rooks) {
        Square from = Bitboards::pop_lsb(rooks);
        Bitboard att = Attacks::rook_attacks(from, all_pieces) & them_pieces;
        while (att) list.add(Move(from, Bitboards::pop_lsb(att), Move::CAPTURE));
    }

    // King
    Bitboard king = pos.pieces(us == WHITE ? W_KING : B_KING);
    if (king) {
        Square from = Bitboards::lsb(king);
        Bitboard att = Attacks::king_attacks(from) & them_pieces;
        while (att) list.add(Move(from, Bitboards::pop_lsb(att), Move::CAPTURE));
    }

    return list;
}

} // namespace Oxta::MoveGen
