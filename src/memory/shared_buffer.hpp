#ifndef OXTA_MEMORY_SHARED_BUFFER_HPP
#define OXTA_MEMORY_SHARED_BUFFER_HPP

#include <cstddef>
#include <string>
#include <stdexcept>
#include <iostream>

#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#endif

namespace Oxta::Memory {

class SharedBuffer {
public:
    virtual ~SharedBuffer() {}
    virtual void* data() = 0;
    virtual size_t size() const = 0;
    virtual const std::string& name() const = 0;
};

#ifdef __linux__
class SharedBufferLinux : public SharedBuffer {
public:
    SharedBufferLinux(const std::string& name, size_t size) : m_name(name), m_size(size) {
        // Use shm_open for portability across processes
        // (memfd_create is anonymous, harder to share without passing FD via socket)
        // shm_open creates /dev/shm/<name>
        m_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
        if (m_fd == -1) {
            throw std::runtime_error("Failed to shm_open");
        }

        if (ftruncate(m_fd, size) == -1) {
            throw std::runtime_error("Failed to ftruncate");
        }

        m_ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_ptr == MAP_FAILED) {
            throw std::runtime_error("Failed to mmap");
        }

        // Zero out init
        std::memset(m_ptr, 0, size);
    }

    ~SharedBufferLinux() {
        if (m_ptr && m_ptr != MAP_FAILED) {
            munmap(m_ptr, m_size);
        }
        if (m_fd != -1) {
            close(m_fd);
            shm_unlink(m_name.c_str()); // Auto unlink? Or keep for Python?
            // Usually keeping it allows the other process to attach.
            // We should unlink only when strictly done or let OS cleanup reboot.
            // For this session, we unlink in destructor might be aggressive if Python attaches later.
            // Better: Don't unlink in destructor if we want persistence.
            // Actually, for proper cleanup, we should. But for testing, let's keep it.
        }
    }

    void* data() override { return m_ptr; }
    size_t size() const override { return m_size; }
    const std::string& name() const override { return m_name; }

private:
    std::string m_name;
    size_t m_size;
    int m_fd = -1;
    void* m_ptr = nullptr;
};
#endif

// Factory
inline SharedBuffer* create_shared_buffer(const std::string& name, size_t size) {
#ifdef __linux__
    return new SharedBufferLinux(name, size);
#else
    return nullptr; // TODO Windows
#endif
}

} // namespace Oxta::Memory

#endif // OXTA_MEMORY_SHARED_BUFFER_HPP
