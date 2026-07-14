// UTF-8 / UTF-32 utility helpers
#pragma once

#include <string>
#include <cstdint>

#include <vector>

// Convert UTF-8 encoded string to UTF-32 codepoint string
std::u32string utf8_to_utf32(const std::string& s);

// Convert UTF-32 string to UTF-8 encoded string
std::string utf32_to_utf8(const std::u32string& s);

// Convert UTF-8 to platform wide string (`std::wstring`). On Windows this
// produces UTF-16 in `wchar_t` (surrogate pairs for codepoints > 0xFFFF).
std::wstring utf8_to_wstring(const std::string& s);

// Convert platform wide string to UTF-8. Handles UTF-16 surrogate pairs on Windows.
std::string wstring_to_utf8(const std::wstring& ws);

// Decode UTF-8 into codepoints and also return the starting byte index for each codepoint.
// outByteIndex will have same length as returned u32string; outByteIndex[i] is the
// byte offset in the original string where codepoint i starts.
std::u32string utf8_to_utf32_with_offsets(const std::string& s, std::vector<int>& outByteIndex);
