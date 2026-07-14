#include "utf8util.h"

std::u32string utf8_to_utf32(const std::string& s) {
    std::u32string out;
    size_t i = 0;
    const uint32_t REPLACEMENT = 0xFFFD;
    while (i < s.size()) {
        unsigned char c0 = static_cast<unsigned char>(s[i]);
        uint32_t code = REPLACEMENT;
        if (c0 <= 0x7F) {
            code = c0; i += 1;
        } else if ((c0 & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 < s.size()) {
                unsigned char c1 = static_cast<unsigned char>(s[i+1]);
                if ((c1 & 0xC0) == 0x80) {
                    uint32_t v = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
                    // Overlong check: must be >= 0x80
                    if (v >= 0x80) {
                        code = v; i += 2;
                    } else {
                        // invalid (overlong)
                        i += 1;
                    }
                } else {
                    i += 1;
                }
            } else {
                i += 1;
            }
        } else if ((c0 & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 < s.size()) {
                unsigned char c1 = static_cast<unsigned char>(s[i+1]);
                unsigned char c2 = static_cast<unsigned char>(s[i+2]);
                if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
                    uint32_t v = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F);
                    // Overlong and surrogate checks: must be >= 0x800 and not in 0xD800..0xDFFF
                    if (v >= 0x800 && !(v >= 0xD800 && v <= 0xDFFF)) {
                        code = v; i += 3;
                    } else {
                        i += 1;
                    }
                } else {
                    i += 1;
                }
            } else {
                i += 1;
            }
        } else if ((c0 & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 < s.size()) {
                unsigned char c1 = static_cast<unsigned char>(s[i+1]);
                unsigned char c2 = static_cast<unsigned char>(s[i+2]);
                unsigned char c3 = static_cast<unsigned char>(s[i+3]);
                if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                    uint32_t v = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F);
                    // valid range up to 0x10FFFF
                    if (v >= 0x10000 && v <= 0x10FFFF) {
                        code = v; i += 4;
                    } else {
                        i += 1;
                    }
                } else {
                    i += 1;
                }
            } else {
                i += 1;
            }
        } else {
            // invalid lead byte
            i += 1;
        }
        out.push_back((char32_t)code);
    }
    return out;
}

std::string utf32_to_utf8(const std::u32string& s) {
    std::string out;
    for (char32_t c : s) {
        if (c <= 0x7f) {
            out.push_back((char)c);
        } else if (c <= 0x7ff) {
            out.push_back((char)(0xc0 | ((c >> 6) & 0x1f)));
            out.push_back((char)(0x80 | (c & 0x3f)));
        } else if (c <= 0xffff) {
            // encode BMP codepoints (note: surrogate codepoints should not appear here)
            out.push_back((char)(0xe0 | ((c >> 12) & 0x0f)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (c & 0x3f)));
        } else {
            out.push_back((char)(0xf0 | ((c >> 18) & 0x07)));
            out.push_back((char)(0x80 | ((c >> 12) & 0x3f)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (c & 0x3f)));
        }
    }
    return out;
}

// Convert UTF-8 to std::wstring
std::wstring utf8_to_wstring(const std::string& s) {
    std::u32string u32 = utf8_to_utf32(s);
    std::wstring out;
    out.reserve(u32.size());
    if (sizeof(wchar_t) == 2) {
        // UTF-16 (Windows): emit surrogate pairs for codepoints > 0xFFFF
        for (char32_t cp : u32) {
            if (cp <= 0xFFFF) {
                out.push_back((wchar_t)cp);
            } else {
                uint32_t v = cp - 0x10000;
                wchar_t high = (wchar_t)((v >> 10) + 0xD800);
                wchar_t low = (wchar_t)((v & 0x3FF) + 0xDC00);
                out.push_back(high);
                out.push_back(low);
            }
        }
    } else {
        // wchar_t is 32-bit; direct cast
        for (char32_t cp : u32) out.push_back((wchar_t)cp);
    }
    return out;
}

// Convert std::wstring to UTF-8
std::string wstring_to_utf8(const std::wstring& ws) {
    std::u32string u32;
    u32.reserve(ws.size());
    if (sizeof(wchar_t) == 2) {
        // UTF-16 input: handle surrogate pairs
        for (size_t i = 0; i < ws.size(); ++i) {
            wchar_t w = ws[i];
            if (w >= 0xD800 && w <= 0xDBFF) {
                // high surrogate
                if (i + 1 < ws.size()) {
                    wchar_t w2 = ws[i+1];
                    if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                        uint32_t high = (uint32_t)(w - 0xD800);
                        uint32_t low = (uint32_t)(w2 - 0xDC00);
                        uint32_t cp = (high << 10) + low + 0x10000;
                        u32.push_back((char32_t)cp);
                        ++i; // consumed low surrogate
                        continue;
                    }
                }
                // unmatched high surrogate -> replacement
                u32.push_back((char32_t)0xFFFD);
            } else if (w >= 0xDC00 && w <= 0xDFFF) {
                // unmatched low surrogate
                u32.push_back((char32_t)0xFFFD);
            } else {
                u32.push_back((char32_t)w);
            }
        }
    } else {
        // wchar_t is 32-bit
        for (wchar_t w : ws) {
            u32.push_back((char32_t)w);
        }
    }
    return utf32_to_utf8(u32);
}

std::u32string utf8_to_utf32_with_offsets(const std::string& s, std::vector<int>& outByteIndex) {
    outByteIndex.clear();
    std::u32string out;
    size_t i = 0;
    const uint32_t REPLACEMENT = 0xFFFD;
    while (i < s.size()) {
        size_t bytePos = i;
        unsigned char c0 = static_cast<unsigned char>(s[i]);
        uint32_t code = REPLACEMENT;
        size_t len = 1;
        if (c0 <= 0x7F) {
            code = c0; len = 1;
        } else if ((c0 & 0xE0) == 0xC0) {
            if (i + 1 < s.size()) {
                unsigned char c1 = static_cast<unsigned char>(s[i+1]);
                if ((c1 & 0xC0) == 0x80) {
                    uint32_t v = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
                    if (v >= 0x80) { code = v; len = 2; }
                }
            }
        } else if ((c0 & 0xF0) == 0xE0) {
            if (i + 2 < s.size()) {
                unsigned char c1 = static_cast<unsigned char>(s[i+1]);
                unsigned char c2 = static_cast<unsigned char>(s[i+2]);
                if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
                    uint32_t v = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F);
                    if (v >= 0x800 && !(v >= 0xD800 && v <= 0xDFFF)) { code = v; len = 3; }
                }
            }
        } else if ((c0 & 0xF8) == 0xF0) {
            if (i + 3 < s.size()) {
                unsigned char c1 = static_cast<unsigned char>(s[i+1]);
                unsigned char c2 = static_cast<unsigned char>(s[i+2]);
                unsigned char c3 = static_cast<unsigned char>(s[i+3]);
                if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                    uint32_t v = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F);
                    if (v >= 0x10000 && v <= 0x10FFFF) { code = v; len = 4; }
                }
            }
        }
        out.push_back((char32_t)code);
        outByteIndex.push_back((int)bytePos);
        i += len;
    }
    return out;
}
