"""Read-only Nintendo DS filesystem and Gen V text parsing.

Does not read save files. Format references are documented in THIRD_PARTY.md.
"""
import struct
u16 = lambda b, p: struct.unpack_from('<H', b, p)[0]
u32 = lambda b, p: struct.unpack_from('<I', b, p)[0]

class ROM:
    def __init__(self, path):
        self.f = path.open('rb')
        self.header = self.read(0, 512)
        self.code = self.header[12:16].decode('ascii')
        if self.code != 'IRDI': raise ValueError(f'Only Italian White 2 (IRDI) is supported; got {self.code}')
        self.fnt = self.read(u32(self.header, 0x40), u32(self.header, 0x44))
        self.fat = self.read(u32(self.header, 0x48), u32(self.header, 0x4C))
        self.files = {}
        self.walk(0xF000, '')

    def read(self, offset, size):
        self.f.seek(offset)
        data = self.f.read(size)
        assert len(data) == size
        return data

    def walk(self, did, prefix):
        p = (did & 0xFFF) * 8
        cursor, fid = u32(self.fnt, p), u16(self.fnt, p + 4)
        while self.fnt[cursor]:
            tag = self.fnt[cursor]; cursor += 1
            size = tag & 127
            name = self.fnt[cursor:cursor + size].decode('ascii'); cursor += size
            path = prefix + name
            if tag & 128:
                sub = u16(self.fnt, cursor); cursor += 2
                self.walk(sub, path + '/')
            else:
                self.files[path] = fid; fid += 1

    def file(self, name):
        fid = self.files[name]
        a, b = struct.unpack_from('<II', self.fat, fid * 8)
        return self.read(a, b - a)

    def narc(self, name):
        data = self.file(name)
        assert data[:4] == b'NARC'
        p = u16(data, 12); entries = None; payload = None
        while p < len(data):
            size = u32(data, p + 4)
            assert size >= 8
            if data[p:p+4] == b'BTAF':
                entries = [struct.unpack_from('<II', data, p+12+i*8) for i in range(u16(data,p+8))]
            elif data[p:p+4] == b'GMIF': payload = data[p+8:p+size]
            p += size
        assert entries is not None and payload is not None
        return [payload[a:b] for a,b in entries]

def text_entries(data):
    section = u32(data, 12)
    result = []
    for i in range(u16(data, 2)):
        p = section + 4 + i*8
        ofs, count = u32(data,p), u16(data,p+4)
        if not count: result.append(''); continue
        words = list(struct.unpack_from('<'+'H'*count, data, section+ofs))
        key = words[-1] ^ 0xFFFF
        for j in range(count-1,-1,-1):
            words[j] ^= key
            key = ((key>>3) | (key<<13)) & 0xFFFF
        if words[0] == 0xF100:
            bits = sum(w << (16*j) for j,w in enumerate(words[1:]))
            words = []
            while bits:
                ch = bits & 511; bits >>= 9
                if ch & 255 == 255: break
                if ch not in (0,1): words.append(ch)
        result.append(''.join(chr(w) if 20 < w < 0xF000 else ' ' for w in words if w != 0xFFFF).strip())
    return result

