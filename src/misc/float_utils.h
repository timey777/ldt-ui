#pragma once

#include <cmath>

namespace ldt {

// 简单的浮点比较工具，避免直接使用 == 或 > 导致的精度问题
bool floatGreater(float a, float b, float eps = 0.01f);

bool floatGreaterEqual(float a, float b, float eps = 0.01f);

bool floatNotEqual(float a, float b, float eps = 1e-6f);

bool floatEqual(float a, float b, float eps = 1e-6f);

} // namespace ldt
