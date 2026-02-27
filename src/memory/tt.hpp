#ifndef OXTA_MEMORY_TT_HPP
#define OXTA_MEMORY_TT_HPP

#include "../core/types.hpp"
#include "../core/moves.hpp"
#include <vector>
#include <cstring>
#include <algorithm> // for std::fill

namespace Oxta {

enum TTFlag : uint8_t {
    TT_NONE = 0,
    TT_EXACT = 1,
    TT_LOWER = 2,
    TT_UPPER = 3
};

struct alignas(16) TTEntry {
    uint64_t key;      // Zobrist Key
    Move move;         // Best Move
    int16_t value;     // Score
    uint8_t depth;     // Depth
    uint8_t gen;       // Generation (Aging)
    uint8_t flag;      // Bound
    uint8_t padding;   // Align

    bool is_empty() const { return key == 0 && depth == 0; }
    void clear() {
        key = 0; move = Move(0); value = 0; depth = 0; gen = 0; flag = 0; padding = 0;
    }
};

// 4-way Bucket (fits in 64-byte cache line)
struct alignas(64) TTCluster {
    TTEntry entry[4];
};

class TranspositionTable {
public:
    TranspositionTable() : clusters(nullptr), num_clusters(0), mask(0), l1_table(nullptr), l1_mask(0) {}
    ~TranspositionTable() {
        if (clusters) delete[] clusters;
        if (l1_table) delete[] l1_table;
    }

    void resize(size_t size_mb) {
        if (clusters) delete[] clusters;
        if (l1_table) delete[] l1_table;

        // L1 Cache: Fixed small size (e.g., 512KB ~ 32k entries) for fast tactics
        // Fits in L3 cache mostly.
        size_t l1_size = 32768;
        l1_table = new TTEntry[l1_size];
        l1_mask = l1_size - 1;

        // L2 Cache: Remaining memory
        // Cluster size = 64 bytes
        uint64_t bytes = size_mb * 1024 * 1024;
        num_clusters = bytes / sizeof(TTCluster);

        // Power of 2
        size_t size = 1;
        while (size <= num_clusters) size <<= 1;
        size >>= 1;

        num_clusters = size;
        mask = num_clusters - 1;

        clusters = new TTCluster[num_clusters];
        clear();
    }

    void clear() {
        std::memset(clusters, 0, num_clusters * sizeof(TTCluster));
        std::memset(l1_table, 0, (l1_mask + 1) * sizeof(TTEntry));
        generation = 0;
    }

    void new_search() {
        generation++;
    }

    // Hierarchical Save
    void save(uint64_t k, Move m, int16_t v, uint8_t d, TTFlag f) {
        // L1: Always Replace (Tactical Cache)
        TTEntry& l1 = l1_table[k & l1_mask];
        write_entry(l1, k, m, v, d, f);

        // L2: Strategic Promotion (Depth > 2)
        if (d > 2) {
            save_l2(k, m, v, d, f);
        }
    }

    bool probe(uint64_t k, Move& m, int16_t& v, uint8_t& d, TTFlag& f) {
        // Check L1 first
        TTEntry& l1 = l1_table[k & l1_mask];
        if (l1.key == k) {
            read_entry(l1, m, v, d, f);
            return true;
        }

        // Check L2
        TTCluster* cluster = &clusters[k & mask];
        for (int i = 0; i < 4; ++i) {
            TTEntry& e = cluster->entry[i];
            if (e.key == k) {
                read_entry(e, m, v, d, f);
                e.gen = generation; // Refresh age
                return true;
            }
        }
        return false;
    }

private:
    TTCluster* clusters;
    size_t num_clusters;
    uint64_t mask;

    TTEntry* l1_table;
    uint64_t l1_mask;

    uint8_t generation = 0;

    void save_l2(uint64_t k, Move m, int16_t v, uint8_t d, TTFlag f) {
        TTCluster* cluster = &clusters[k & mask];
        int replace_idx = -1;
        int min_score = 10000;

        for (int i = 0; i < 4; ++i) {
            TTEntry& e = cluster->entry[i];

            if (e.key == k) {
                if (d >= e.depth || e.gen != generation || f == TT_EXACT) {
                    write_entry(e, k, m, v, d, f);
                }
                return;
            }

            int score = e.depth;
            if (e.gen != generation) score -= 100;

            if (score < min_score) {
                min_score = score;
                replace_idx = i;
            }
        }

        if (replace_idx != -1) {
            write_entry(cluster->entry[replace_idx], k, m, v, d, f);
        }
    }

    void write_entry(TTEntry& e, uint64_t k, Move m, int16_t v, uint8_t d, TTFlag f) {
        e.key = k;
        e.move = m;
        e.value = v;
        e.depth = d;
        e.flag = f;
        e.gen = generation;
    }

    void read_entry(const TTEntry& e, Move& m, int16_t& v, uint8_t& d, TTFlag& f) {
        m = e.move;
        v = e.value;
        d = e.depth;
        f = static_cast<TTFlag>(e.flag);
    }
};

} // namespace Oxta

#endif // OXTA_MEMORY_TT_HPP
