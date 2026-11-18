#include "util/odb_file.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace odb2kicad {

static bool isFile(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool odbFileExists(const std::string& path) {
    return isFile(path) || isFile(path + ".z") || isFile(path + ".Z");
}

// compress(1): a 3-byte header (1f 9d, then the largest code size in the low 5 bits and the
// block-mode flag in the top bit), then LZW codes packed low bit first. Codes start 9 bits wide
// and widen when the table outgrows them; in block mode code 256 clears the table. At either
// event the stream skips to the next multiple of eight codes of the old width, counted from
// where that width began.
bool unixDecompress(const std::string& in, std::string& out) {
    out.clear();
    if (in.size() < 3 || (unsigned char)in[0] != 0x1f || (unsigned char)in[1] != 0x9d) return false;
    const int maxBits = (unsigned char)in[2] & 0x1f;
    const bool blockMode = ((unsigned char)in[2] & 0x80) != 0;
    if (maxBits < 9 || maxBits > 16) return false;
    const int maxMaxCode = 1 << maxBits;

    std::vector<int> prefix(maxMaxCode, 0);
    std::vector<unsigned char> suffix(maxMaxCode, 0);
    for (int i = 0; i < 256; i++) suffix[i] = (unsigned char)i;
    std::vector<unsigned char> stack;
    stack.reserve(maxMaxCode);

    const size_t totalBits = in.size() * 8;
    size_t pos = 24, groupStart = 24;
    int nBits = 9;
    int maxCode = (1 << nBits) - 1;
    int freeEnt = blockMode ? 257 : 256;
    int oldCode = -1;
    unsigned char finChar = 0;
    auto align = [&]() {
        const size_t group = (size_t)nBits * 8;
        const size_t rem = (pos - groupStart) % group;
        if (rem) pos += group - rem;
        groupStart = pos;
    };
    out.reserve(in.size() * 3);
    while (pos + (size_t)nBits <= totalBits) {
        if (freeEnt > maxCode) {  // the table has outgrown the code width
            align();
            nBits++;
            maxCode = nBits == maxBits ? maxMaxCode : (1 << nBits) - 1;
            continue;
        }
        const size_t byte = pos >> 3;
        unsigned long v = (unsigned char)in[byte];
        if (byte + 1 < in.size()) v |= (unsigned long)(unsigned char)in[byte + 1] << 8;
        if (byte + 2 < in.size()) v |= (unsigned long)(unsigned char)in[byte + 2] << 16;
        int code = (int)((v >> (pos & 7)) & ((1UL << nBits) - 1));
        pos += (size_t)nBits;

        if (oldCode == -1) {      // the first code is a literal
            if (code >= 256) return false;
            finChar = (unsigned char)code;
            oldCode = code;
            out.push_back((char)finChar);
            continue;
        }
        if (code == 256 && blockMode) {
            for (int i = 0; i < 256; i++) prefix[i] = 0;
            freeEnt = 256;
            align();
            nBits = 9;
            maxCode = (1 << nBits) - 1;
            continue;
        }
        const int inCode = code;
        stack.clear();
        if (code >= freeEnt) {    // the code being defined right now: old string + its first byte
            if (code > freeEnt) return false;
            stack.push_back(finChar);
            code = oldCode;
        }
        while (code >= 256) {
            stack.push_back(suffix[code]);
            code = prefix[code];
        }
        finChar = suffix[code];
        stack.push_back(finChar);
        for (size_t i = stack.size(); i-- > 0;) out.push_back((char)stack[i]);
        if (freeEnt < maxMaxCode) {
            prefix[freeEnt] = oldCode;
            suffix[freeEnt] = finChar;
            freeEnt++;
        }
        oldCode = inCode;
    }
    return true;
}

std::unique_ptr<std::istream> openOdbFile(const std::string& path) {
    if (isFile(path)) {
        auto f = std::make_unique<std::ifstream>(path);
        if (f->is_open()) return f;
        return nullptr;
    }
    for (const char* ext : {".z", ".Z"}) {
        if (!isFile(path + ext)) continue;
        std::ifstream f(path + ext, std::ios::binary);
        if (!f.is_open()) return nullptr;
        std::ostringstream raw;
        raw << f.rdbuf();
        std::string text;
        if (!unixDecompress(raw.str(), text)) return nullptr;
        return std::make_unique<std::istringstream>(std::move(text));
    }
    return nullptr;
}

} // namespace odb2kicad
