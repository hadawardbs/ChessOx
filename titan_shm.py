import mmap
import os
import struct
import time

class SharedRingBuffer:
    def __init__(self, name, size=4*1024*1024):
        self.name = name
        self.size = size
        self.fd = -1
        self.buf = None

        # Open /dev/shm/<name>
        try:
            self.fd = os.open(f"/dev/shm/{name}", os.O_RDWR)
            self.buf = mmap.mmap(self.fd, size)
        except FileNotFoundError:
            print(f"Shared memory {name} not found. Ensure C++ engine is running with --shared-mem {name}")
            raise

    def write(self, data):
        # Read header: write_idx (0), read_idx (4), capacity (8)
        header = struct.unpack('III', self.buf[:12])
        w = header[0]
        r = header[1]
        cap = header[2]

        data_offset = 64 # sizeof(RingHeader) aligned to 64

        # Space check
        used = (w - r) if w >= r else (cap - (r - w))
        avail = cap - used - 1

        if len(data) > avail:
            return False

        # Write
        for b in data:
            self.buf[data_offset + w] = b
            w = (w + 1) % cap

        # Update write_idx atomic-ish
        struct.pack_into('I', self.buf, 0, w)
        return True

    def read(self):
        header = struct.unpack('III', self.buf[:12])
        w = header[0]
        r = header[1]
        cap = header[2]

        used = (w - r) if w >= r else (cap - (r - w))
        if used == 0:
            return None

        data_offset = 64
        res = bytearray()

        # Read until empty? Or framed?
        # Protocol: Type(1) + Size(2)
        if used < 3: return None

        # Peek header
        type_b = self.buf[data_offset + r]
        size_b1 = self.buf[data_offset + (r+1)%cap]
        size_b2 = self.buf[data_offset + (r+2)%cap]
        msg_len = size_b1 | (size_b2 << 8)

        if used < 3 + msg_len: return None

        # Read Full Message
        for i in range(3 + msg_len):
            res.append(self.buf[data_offset + r])
            r = (r + 1) % cap

        struct.pack_into('I', self.buf, 4, r)
        return res

    def close(self):
        if self.buf: self.buf.close()
        if self.fd != -1: os.close(self.fd)
