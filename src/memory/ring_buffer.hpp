#ifndef OXTA_MEMORY_RING_BUFFER_HPP
#define OXTA_MEMORY_RING_BUFFER_HPP

#include "shared_buffer.hpp"
#include <atomic>
#include <cstring>

namespace Oxta::Memory {

// Message Protocol
// Type (1 byte) | Size (2 bytes) | Data (...)
enum MsgType : uint8_t {
    MSG_NONE = 0,
    MSG_EVAL_REQ = 1,
    MSG_EVAL_RES = 2,
    MSG_STOP = 255
};

struct alignas(64) RingHeader {
    std::atomic<uint32_t> write_idx;
    std::atomic<uint32_t> read_idx;
    uint32_t capacity;
    uint32_t padding;
};

class RingBuffer {
public:
    RingBuffer(SharedBuffer* buf) : m_buf(buf) {
        m_header = static_cast<RingHeader*>(buf->data());
        m_data = static_cast<uint8_t*>(buf->data()) + sizeof(RingHeader);
        m_capacity = buf->size() - sizeof(RingHeader);

        // Init header if fresh
        // Caveat: How to know if fresh?
        // Assume first creator zeros it. Our SharedBuffer constructor zeros it.
        // We might want to set capacity explicitly.
        if (m_header->capacity == 0) {
            m_header->capacity = m_capacity;
            m_header->write_idx = 0;
            m_header->read_idx = 0;
        }
    }

    bool write(const void* data, size_t len) {
        uint32_t w = m_header->write_idx.load(std::memory_order_relaxed);
        uint32_t r = m_header->read_idx.load(std::memory_order_acquire);

        // Check free space
        // Wait, standard ring math:
        // Size used = (w - r + cap) % cap ?
        // Let's use absolute indices (monotonic) for simpler math, modulo only on access.
        // But atomic wrap around is hard.
        // Let's use masked indices if size is power of 2.
        // m_capacity isn't guaranteed power of 2 here.

        // Simple implementation:
        // space = capacity - (w - r) (if we assume w >= r logically, but w wraps)
        // Correct space:
        size_t used = (w >= r) ? (w - r) : (m_capacity - (r - w));
        size_t avail = m_capacity - used - 1; // Always keep 1 byte empty

        if (len > avail) return false;

        // Write
        for (size_t i = 0; i < len; ++i) {
            m_data[(w + i) % m_capacity] = static_cast<const uint8_t*>(data)[i];
        }

        m_header->write_idx.store((w + len) % m_capacity, std::memory_order_release);
        return true;
    }

    bool read(void* out_data, size_t len) {
        uint32_t w = m_header->write_idx.load(std::memory_order_acquire);
        uint32_t r = m_header->read_idx.load(std::memory_order_relaxed);

        size_t used = (w >= r) ? (w - r) : (m_capacity - (r - w));
        if (used < len) return false;

        for (size_t i = 0; i < len; ++i) {
            static_cast<uint8_t*>(out_data)[i] = m_data[(r + i) % m_capacity];
        }

        m_header->read_idx.store((r + len) % m_capacity, std::memory_order_release);
        return true;
    }

    size_t available() const {
        uint32_t w = m_header->write_idx.load(std::memory_order_acquire);
        uint32_t r = m_header->read_idx.load(std::memory_order_relaxed);
        return (w >= r) ? (w - r) : (m_capacity - (r - w));
    }

private:
    SharedBuffer* m_buf;
    RingHeader* m_header;
    uint8_t* m_data;
    uint32_t m_capacity;
};

} // namespace Oxta::Memory

#endif // OXTA_MEMORY_RING_BUFFER_HPP
