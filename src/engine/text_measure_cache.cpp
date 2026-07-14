#include "text_measure_cache.h"

namespace ldt {

TextMeasureCache::Metrics TextMeasureCache::get(
    const std::string& text, const std::string& fontFamily,
    float fontSize, bool bold, float wrapW, ITextMeasurer* measurer)
{
    Key key{ text, fontFamily, fontSize, bold };
    bool unwrapped = (wrapW <= 0 || wrapW >= 100000.0f);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (unwrapped || it->second.intrinsicWidth <= wrapW)
            return it->second;
    }

    TextMetrics tm = measurer->measureText(text, { fontFamily, fontSize }, wrapW);
    Metrics result{ tm.width, tm.lineHeight, tm.ascent, tm.descent };

    if (unwrapped) {
        cache_[key] = result;
    } else {
        TextMetrics tmUn = measurer->measureText(text, { fontFamily, fontSize }, -1.0f);
        cache_[key] = { tmUn.width, tmUn.lineHeight, tmUn.ascent, tmUn.descent };
    }
    return result;
}

} // namespace ldt
