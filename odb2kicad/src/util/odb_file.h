#pragma once
#include <istream>
#include <memory>
#include <string>

namespace odb2kicad {

// ODB++ lets any file of a job be stored compressed with UNIX compress, under the same name
// plus ".z" (or ".Z"): features.z, components.z, eda/data.z ... These helpers make the two
// forms one: the plain file when it exists, else its compressed twin, decompressed in memory.

// True when `path`, `path`.z or `path`.Z is a regular file
bool odbFileExists(const std::string& path);

// The file's text as a stream; null when neither form exists or the compressed one is damaged
std::unique_ptr<std::istream> openOdbFile(const std::string& path);

// UNIX compress (LZW, magic 1f 9d) -> the original bytes. False when `in` is not a compress
// stream or is cut short.
bool unixDecompress(const std::string& in, std::string& out);

} // namespace odb2kicad
