#include "float_utils.h"
#include <cmath>

namespace ldt {

bool floatGreater(float a, float b, float eps) {
    return a > b + eps;
}

bool floatGreaterEqual(float a, float b, float eps) {
    return a + eps >= b;
}

bool floatNotEqual(float a, float b, float eps) {
    return std::fabs(a - b) > eps;
}

bool floatEqual(float a, float b, float eps) {
    return std::fabs(a - b) <= eps;
}

} // namespace ldt
