#include "uitypes.h"
#include <string>
#include <stdexcept>

namespace ldt
{
    namespace ui
    {
        Color Color::fromHex(const std::string &hex)
        {
            if (hex == "none")
            {
                return Color(0, 0, 0, 0);
            }

            if (hex.empty() || hex[0] != '#')
            {
                return Color(0, 0, 0, 0);
            }

            std::string hexStr = hex.substr(1);
            try
            {
                unsigned long value = std::stoul(hexStr, nullptr, 16);

                float r, g, b, a = 1.0f;

                if (hexStr.length() == 6)
                {
                    r = ((value >> 16) & 0xFF) / 255.0f;
                    g = ((value >> 8) & 0xFF) / 255.0f;
                    b = (value & 0xFF) / 255.0f;
                }
                else if (hexStr.length() == 8)
                {
                    r = ((value >> 24) & 0xFF) / 255.0f;
                    g = ((value >> 16) & 0xFF) / 255.0f;
                    b = ((value >> 8) & 0xFF) / 255.0f;
                    a = (value & 0xFF) / 255.0f;
                }
                else
                {
                    return Color(0, 0, 0, 1);
                }

                return Color(r, g, b, a);
            }
            catch (...)
            {
                return Color(0, 0, 0, 0);
            }
        }
    }
}
