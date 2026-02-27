#include "position.hpp"
#include <sstream>
#include <vector>

namespace Oxta {

void Position::set(const std::string& fen) {
    clear();
    std::stringstream ss(fen);
    std::string placement, side, castling, ep, half, full;
    ss >> placement >> side >> castling >> ep >> half >> full;

    // 1. Placement
    int rank = 7;
    int file = 0;
    for (char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit(c)) {
            file += (c - '0');
        } else {
            Square s = static_cast<Square>(rank * 8 + file);
            Piece p = NO_PIECE;
            switch (c) {
                case 'P': p = W_PAWN; break;
                case 'N': p = W_KNIGHT; break;
                case 'B': p = W_BISHOP; break;
                case 'R': p = W_ROOK; break;
                case 'Q': p = W_QUEEN; break;
                case 'K': p = W_KING; break;
                case 'p': p = B_PAWN; break;
                case 'n': p = B_KNIGHT; break;
                case 'b': p = B_BISHOP; break;
                case 'r': p = B_ROOK; break;
                case 'q': p = B_QUEEN; break;
                case 'k': p = B_KING; break;
            }
            put_piece(p, s);
            zobrist ^= Zobrist::psq[p][s];
            file++;
        }
    }

    // 2. Side
    stm = (side == "w") ? WHITE : BLACK;
    if (stm == BLACK) zobrist ^= Zobrist::side_to_move;

    // 3. Castling
    if (castling != "-") {
        for (char c : castling) {
            if (c == 'K') castlingRights |= 1;
            else if (c == 'Q') castlingRights |= 2;
            else if (c == 'k') castlingRights |= 4;
            else if (c == 'q') castlingRights |= 8;
        }
    }
    zobrist ^= Zobrist::castling[castlingRights];

    // 4. EP
    if (ep != "-") {
        int f = ep[0] - 'a';
        int r = ep[1] - '1';
        epSquare = static_cast<Square>(r * 8 + f);
        zobrist ^= Zobrist::en_passant[f];
    } else {
        epSquare = SQ_NONE;
        zobrist ^= Zobrist::en_passant[FILE_NB];
    }

    // 5. Clocks (optional)
    try {
        if (!half.empty()) rule50 = std::stoi(half);
        if (!full.empty()) plies = (std::stoi(full) - 1) * 2 + (stm == BLACK ? 1 : 0);
    } catch(...) {}
}

void Position::do_move(Move m, StateInfo& state) {
    // Save state
    state.castlingRights = castlingRights;
    state.epSquare = epSquare;
    state.rule50 = rule50;
    state.zobrist = zobrist;
    state.capturedPiece = NO_PIECE;

    Square from = m.from();
    Square to = m.to();
    int flags = m.flags();
    Piece p = piece_on(from);

    // Valid move assumption: p exists and belongs to stm.

    // Update Zobrist for moving piece
    zobrist ^= Zobrist::psq[p][from];
    zobrist ^= Zobrist::psq[p][to]; // Will correct if promotion later

    // Handle Capture
    if (m.is_capture() && !m.is_en_passant()) {
        Piece cap = piece_on(to);
        state.capturedPiece = cap;
        remove_piece(cap, to);
        zobrist ^= Zobrist::psq[cap][to];
    }

    // Move the piece
    move_piece(p, from, to);

    // Special moves
    if (m.is_promotion()) {
        Piece promo = static_cast<Piece>(m.promotion_type() + (stm == WHITE ? 0 : 8));
        remove_piece(p, to); // Remove pawn
        put_piece(promo, to); // Add promo piece
        zobrist ^= Zobrist::psq[p][to]; // Remove pawn hash
        zobrist ^= Zobrist::psq[promo][to]; // Add promo hash
    } else if (m.is_en_passant()) {
        Square capSq = static_cast<Square>(to + (stm == WHITE ? -8 : 8));
        Piece cap = (stm == WHITE ? B_PAWN : W_PAWN);
        state.capturedPiece = cap;
        remove_piece(cap, capSq);
        zobrist ^= Zobrist::psq[cap][capSq];
    } else if (m.is_castle()) {
        if (flags == Move::CASTLE_KS) {
            Square rFrom = (stm == WHITE ? SQ_H1 : SQ_H8);
            Square rTo   = (stm == WHITE ? SQ_F1 : SQ_F8);
            Piece r = (stm == WHITE ? W_ROOK : B_ROOK);
            move_piece(r, rFrom, rTo);
            zobrist ^= Zobrist::psq[r][rFrom];
            zobrist ^= Zobrist::psq[r][rTo];
        } else {
            Square rFrom = (stm == WHITE ? SQ_A1 : SQ_A8);
            Square rTo   = (stm == WHITE ? SQ_D1 : SQ_D8);
            Piece r = (stm == WHITE ? W_ROOK : B_ROOK);
            move_piece(r, rFrom, rTo);
            zobrist ^= Zobrist::psq[r][rFrom];
            zobrist ^= Zobrist::psq[r][rTo];
        }
    }

    // Update Castling Rights
    if (castlingRights) {
        zobrist ^= Zobrist::castling[castlingRights];

        // If King moves, lose all rights
        if (p == W_KING) castlingRights &= ~3;
        else if (p == B_KING) castlingRights &= ~12;

        // If Rook moves or is captured, lose rights
        // Simplified check: Mask out based on squares involved
        // This handles both moving a rook and capturing a rook
        // Masks:
        // A1 (0): ~2 (WQ)
        // H1 (7): ~1 (WK)
        // A8 (56): ~8 (BQ)
        // H8 (63): ~4 (BK)

        auto update_cr = [&](Square s) {
            if (s == SQ_A1) castlingRights &= ~2;
            else if (s == SQ_H1) castlingRights &= ~1;
            else if (s == SQ_A8) castlingRights &= ~8;
            else if (s == SQ_H8) castlingRights &= ~4;
        };

        update_cr(from);
        update_cr(to);

        zobrist ^= Zobrist::castling[castlingRights];
    }

    // Update EP Square
    // XOR out old EP
    if (epSquare != SQ_NONE) {
        zobrist ^= Zobrist::en_passant[epSquare % 8];
    } else {
        zobrist ^= Zobrist::en_passant[FILE_NB];
    }

    if (flags == Move::DOUBLE_PUSH) {
        epSquare = static_cast<Square>(to + (stm == WHITE ? -8 : 8));
        zobrist ^= Zobrist::en_passant[epSquare % 8];
    } else {
        epSquare = SQ_NONE;
        zobrist ^= Zobrist::en_passant[FILE_NB];
    }

    // Flip side
    stm = ~stm;
    zobrist ^= Zobrist::side_to_move;

    rule50++;
    if (p == W_PAWN || p == B_PAWN || m.is_capture()) rule50 = 0;
    plies++;
}

void Position::undo_move(Move m, const StateInfo& state) {
    stm = ~stm; // Flip back

    Square from = m.from();
    Square to = m.to();
    int flags = m.flags();

    // Retrieve original piece type
    Piece p = piece_on(to);
    // If promotion, piece_on(to) is the promoted piece (e.g., Queen), but we moved a Pawn.
    if (m.is_promotion()) {
        p = (stm == WHITE ? W_PAWN : B_PAWN);
        remove_piece(piece_on(to), to); // Remove Queen
        put_piece(p, from); // Put Pawn back
    } else {
        move_piece(p, to, from);
    }

    // Restore captured piece
    if (state.capturedPiece != NO_PIECE) {
        Square capSq = to;
        if (m.is_en_passant()) {
            capSq = static_cast<Square>(to + (stm == WHITE ? -8 : 8));
        }
        put_piece(state.capturedPiece, capSq);
    }

    // Un-castle
    if (m.is_castle()) {
        if (flags == Move::CASTLE_KS) {
            Square rFrom = (stm == WHITE ? SQ_H1 : SQ_H8);
            Square rTo   = (stm == WHITE ? SQ_F1 : SQ_F8);
            Piece r = (stm == WHITE ? W_ROOK : B_ROOK);
            move_piece(r, rTo, rFrom);
        } else {
            Square rFrom = (stm == WHITE ? SQ_A1 : SQ_A8);
            Square rTo   = (stm == WHITE ? SQ_D1 : SQ_D8);
            Piece r = (stm == WHITE ? W_ROOK : B_ROOK);
            move_piece(r, rTo, rFrom);
        }
    }

    // Restore state
    castlingRights = state.castlingRights;
    epSquare = state.epSquare;
    rule50 = state.rule50;
    zobrist = state.zobrist;
}

void Position::do_null_move(StateInfo& state) {
    state.castlingRights = castlingRights;
    state.epSquare = epSquare;
    state.rule50 = rule50;
    state.zobrist = zobrist;
    state.capturedPiece = NO_PIECE;

    if (epSquare != SQ_NONE) {
        zobrist ^= Zobrist::en_passant[epSquare % 8];
        epSquare = SQ_NONE;
        zobrist ^= Zobrist::en_passant[FILE_NB];
    }

    stm = ~stm;
    zobrist ^= Zobrist::side_to_move;

    rule50++;
    plies++;
}

void Position::undo_null_move(const StateInfo& state) {
    stm = ~stm;
    epSquare = state.epSquare;
    rule50 = state.rule50;
    zobrist = state.zobrist;
    plies--;
}

} // namespace Oxta
