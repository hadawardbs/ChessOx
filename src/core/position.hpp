#ifndef OXTA_CORE_POSITION_HPP
#define OXTA_CORE_POSITION_HPP

#include "types.hpp"
#include "bitboard.hpp"
#include "zobrist.hpp"
#include "moves.hpp"
#include "attacks.hpp"
#include <string>
#include <string_view>

namespace Oxta {

class Position {
public:
    Position() { clear(); }

    void clear() {
        for (int i = 0; i < PIECE_NB; ++i) byPiece[i] = 0;
        for (int i = 0; i < COLOR_NB; ++i) byColor[i] = 0;
        stm = WHITE;
        castlingRights = 0;
        epSquare = SQ_NONE;
        rule50 = 0;
        plies = 0;
        zobrist = 0;
    }

    // FEN Parsing
    void set(const std::string& fen);

    // Accessors
    Bitboard pieces(PieceType pt, Color c) const {
        int p = make_piece(pt, c);
        return byPiece[p];
    }

    // Overload for direct Piece access (Fixes ambiguity)
    Bitboard pieces(Piece p) const {
        return byPiece[p];
    }

    Bitboard pieces(Color c) const {
        return byColor[c];
    }

    Bitboard pieces() const {
        return byColor[WHITE] | byColor[BLACK];
    }

    Piece piece_on(Square s) const {
        Bitboard b = (1ULL << s);
        if (!(pieces() & b)) return NO_PIECE;
        for (int i = 1; i < PIECE_NB; ++i) {
            if (byPiece[i] & b) return static_cast<Piece>(i);
        }
        return NO_PIECE;
    }

    Color side_to_move() const { return stm; }
    Square ep_square() const { return epSquare; }
    int castling_rights() const { return castlingRights; } // 4 bits: WK, WQ, BK, BQ
    uint64_t key() const { return zobrist; }

    // State Modification
    void do_move(Move m);

    struct StateInfo {
        int castlingRights;
        Square epSquare;
        int rule50;
        uint64_t zobrist;
        Piece capturedPiece;
    };

    void do_move(Move m, StateInfo& state);
    void undo_move(Move m, const StateInfo& state);

    void do_null_move(StateInfo& state);
    void undo_null_move(const StateInfo& state);

private:
    Bitboard byPiece[PIECE_NB];
    Bitboard byColor[COLOR_NB];
    Color stm;
    int castlingRights; // Bit 0: WK, 1: WQ, 2: BK, 3: BQ
    Square epSquare;
    int rule50;
    int plies; // Halfmoves from start
    uint64_t zobrist;

    // Helpers
    int make_piece(PieceType pt, Color c) const {
        return pt + (c == WHITE ? 0 : 8);
    }

    void put_piece(Piece p, Square s) {
        Bitboard b = (1ULL << s);
        byPiece[p] |= b;
        byColor[p < 9 ? WHITE : BLACK] |= b;
    }

    void remove_piece(Piece p, Square s) {
        Bitboard b = (1ULL << s);
        byPiece[p] &= ~b;
        byColor[p < 9 ? WHITE : BLACK] &= ~b;
    }

    void move_piece(Piece p, Square from, Square to) {
        remove_piece(p, from);
        put_piece(p, to);
    }
};

} // namespace Oxta

#endif // OXTA_CORE_POSITION_HPP
