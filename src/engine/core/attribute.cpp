#include "attribute.h"

#include <cmath>

// 构造
Attribute::Attribute() = default;
Attribute::Attribute(bool b) : value_(b) {}
Attribute::Attribute(int i) : value_(i) {}
Attribute::Attribute(float f) : value_(f) {}
Attribute::Attribute(const char* s) : value_(std::string(s)) {}
Attribute::Attribute(std::string s) : value_(std::move(s)) {}
Attribute::Attribute(std::vector<float> arr) : value_(std::move(arr)) {}
Attribute::Attribute(ObjectType obj) : value_(std::move(obj)) {}

Attribute::Attribute(const std::unordered_map<std::string, Attribute>& obj) {
    ObjectType m;
    for (const auto& kv : obj) {
        m.emplace(kv.first, std::make_shared<Attribute>(kv.second));
    }
    value_ = std::move(m);
}

// 类型检查
bool Attribute::isNull() const { return std::holds_alternative<std::monostate>(value_); }
bool Attribute::isBool() const { return std::holds_alternative<bool>(value_); }
bool Attribute::isInt() const { return std::holds_alternative<int>(value_); }
bool Attribute::isFloat() const { return std::holds_alternative<float>(value_); }
bool Attribute::isString() const { return std::holds_alternative<std::string>(value_); }
bool Attribute::isArray() const { return std::holds_alternative<std::vector<float>>(value_); }
bool Attribute::isObject() const { return std::holds_alternative<ObjectType>(value_); }

// 深度比较
bool Attribute::equals(const Attribute& other) const {
    if (value_.index() != other.value_.index()) return false;
    if (isNull()) return other.isNull();
    if (isBool()) return other.isBool() && as<bool>() == other.as<bool>();
    if (isInt()) return other.isInt() && asInt() == other.asInt();
    if (isFloat()) return other.isFloat() && std::fabs(asFloat() - other.asFloat()) < 0.0001f;
    if (isString()) return other.isString() && as<std::string>() == other.as<std::string>();
    if (isArray()) {
        if (!other.isArray()) return false;
        const auto* arrA = asArrayPtr();
        const auto* arrB = other.asArrayPtr();
        if (!arrA || !arrB || arrA->size() != arrB->size()) return false;
        for (size_t i = 0; i < arrA->size(); ++i)
            if (std::fabs((*arrA)[i] - (*arrB)[i]) > 0.0001f) return false;
        return true;
    }
    if (isObject()) {
        if (!other.isObject()) return false;
        const auto* objA = asObjectPtr();
        const auto* objB = other.asObjectPtr();
        if (!objA || !objB || objA->size() != objB->size()) return false;
        for (const auto& kv : *objA) {
            const auto& k = kv.first;
            const auto& va = kv.second;
            auto it = objB->find(k);
            if (it == objB->end() || !va || !it->second || !va->equals(*it->second)) return false;
        }
        return true;
    }
    return false;
}

// 智能转换
float Attribute::asFloat() const {
    if (isFloat()) {
        return std::get<float>(value_);
    }
    else if (isInt()) {
        return static_cast<float>(std::get<int>(value_));
    }
    else if (isString()) {
        try {
            return std::stof(std::get<std::string>(value_));
        }
        catch (...) {
            throw TypeError("Cannot convert string to float");
        }
    }
    throw TypeError("Cannot convert to float");
}

int Attribute::asInt() const {
    if (isInt()) {
        return std::get<int>(value_);
    }
    else if (isFloat()) {
        return static_cast<int>(std::get<float>(value_));
    }
    else if (isString()) {
        try {
            return std::stoi(std::get<std::string>(value_));
        }
        catch (...) {
            throw TypeError("Cannot convert string to int");
        }
    }
    throw TypeError("Cannot convert to int");
}

// 指针访问
const Attribute::ObjectType* Attribute::asObjectPtr() const {
    return std::get_if<ObjectType>(&value_);
}
const std::string* Attribute::asStringPtr() const {
    return std::get_if<std::string>(&value_);
}
const std::vector<float>* Attribute::asArrayPtr() const {
    return std::get_if<std::vector<float>>(&value_);
}

// 工具
Attribute Attribute::MakeSize(float width, float height) {
    return Attribute(std::vector<float>{width, height});
}