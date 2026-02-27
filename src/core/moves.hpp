#ifndef OXTA_CORE_MOVES_HPP
#define OXTA_CORE_MOVES_HPP

#include "types.hpp"
#include <vector>
#include <string>

namespace Oxta {

enum MoveType : int {
    NORMAL = 0,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14
};

// Simplified Move Class wrapping a uint16_t
class Move {
public:
    uint16_t data;

    static constexpr int FLAG_NONE = 0;
    static constexpr int FLAG_DOUBLE_PAWN = 1 << 12;
    static constexpr int FLAG_CASTLING = 2 << 12;
    static constexpr int FLAG_EN_PASSANT = 3 << 12;
    static constexpr int FLAG_PROMO = 8 << 12;

    enum Flag : int {
        QUIET = 0,
        DOUBLE_PUSH = 1,
        CASTLE_KS = 2,
        CASTLE_QS = 3,
        CAPTURE = 4,
        EP_CAPTURE = 5,
        PROMO_N = 8,
        PROMO_B = 9,
        PROMO_R = 10,
        PROMO_Q = 11,
        PROMO_CAPTURE_N = 12,
        PROMO_CAPTURE_B = 13,
        PROMO_CAPTURE_R = 14,
        PROMO_CAPTURE_Q = 15
    };

    constexpr Move() : data(0) {}
    constexpr Move(uint16_t d) : data(d) {}
    constexpr Move(Square from, Square to, Flag flag = QUIET) {
        data = static_cast<uint16_t>(to | (from << 6) | (flag << 12));
    }

    constexpr Square to() const { return static_cast<Square>(data & 0x3F); }
    constexpr Square from() const { return static_cast<Square>((data >> 6) & 0x3F); }
    constexpr int flags() const { return (data >> 12) & 0xF; }

    constexpr bool is_capture() const { return (data >> 12) & 0x4; }
    constexpr bool is_promotion() const { return (data >> 12) & 0x8; }
    constexpr bool is_en_passant() const { return flags() == EP_CAPTURE; }
    constexpr bool is_castle() const { return flags() == CASTLE_KS || flags() == CASTLE_QS; }

    constexpr PieceType promotion_type() const {
        if (!is_promotion()) return NO_PIECE_TYPE;
        int p = flags() & 3;
        return static_cast<PieceType>(KNIGHT + p);
    }

    bool operator==(const Move& m) const { return data == m.data; }
    bool operator!=(const Move& m) const { return data != m.data; }

    static const Move NONE;
    static const Move NULL_MOVE;
};

inline const Move Move::NONE(0);
inline const Move Move::NULL_MOVE(0xFFFF);

// Simple fixed-size move list for performance
struct MoveList {
    Move moves[256];
    int scores[256]; // Added scores for sorting
    int count = 0;

    void add(Move m) {
        moves[count++] = m;
    }

    Move* begin() { return moves; }
    Move* end() { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end() const { return moves + count; }

    Move operator[](int i) const { return moves[i]; }
    void clear() { count = 0; }
    int size() const { return count; }
};

} // namespace Oxta

#endif // OXTA_CORE_MOVES_HPP
