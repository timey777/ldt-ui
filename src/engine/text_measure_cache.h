#pragma once

#include <string>
#include <unordered_map>
#include "render/text_measurer.h"

namespace ldt {

// =========================================================================
// TextMeasureCache — standalone intrinsic-metrics cache for text measurement.
//
// Caches unwrapped text metrics keyed by (text, fontFamily, fontSize, bold).
// On resize, most text doesn't change — cache hits avoid expensive
// text-shaping calls entirely.
//
// Usage:
//   TextMeasureCache cache;
//   auto m = cache.get(text, family, size, bold, wrapW, measurer);
//   float w = m.intrinsicWidth;
//   float h = m.lineHeight;
// =========================================================================
class TextMeasureCache {
public:
    struct Metrics {
        float intrinsicWidth = 0;
        float lineHeight = 0;
        float ascent = 0;
        float descent = 0;
    };

    Metrics get(const std::string& text, const std::string& fontFamily,
                float fontSize, bool bold, float wrapW, ITextMeasurer* measurer);

private:
    struct Key {
        std::string text;
        std::string fontFamily;
        float fontSize = 0;
        bool bold = false;
        bool operator==(const Key& o) const {
            return text == o.text && fontFamily == o.fontFamily &&
                   fontSize == o.fontSize && bold == o.bold;
        }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            size_t h = std::hash<std::string>{}(k.text);
            h ^= std::hash<std::string>{}(k.fontFamily) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>{}(k.fontSize) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.bold) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<Key, Metrics, KeyHash> cache_;
};

} // namespace ldt
