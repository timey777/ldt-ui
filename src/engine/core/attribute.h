#pragma once

#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <utility>

class Attribute {
public:
	// 嵌套对象类型：使用 shared_ptr 避免无限递归类型
	using ObjectType = std::unordered_map<std::string, std::shared_ptr<Attribute>>;

	// 支持的基础类型
	using ValueType = std::variant<
		std::monostate,      // 空状态
		bool,                // 布尔值
		int,                 // 整数
		float,               // 单精度浮点
		std::string,         // 字符串
		std::vector<float>,  // 浮点数组
		ObjectType           // 嵌套对象（字符串->shared_ptr<Attribute>）
	>;

	// 构造方法
	Attribute();
	explicit Attribute(bool b);
	explicit Attribute(int i);
	explicit Attribute(float f);
	explicit Attribute(const char* s);
	explicit Attribute(std::string s);
	explicit Attribute(std::vector<float> arr);
	explicit Attribute(ObjectType obj);

	// 便捷：从简单的 map<string,Attribute> 构造（会复制为 shared_ptr）
	explicit Attribute(const std::unordered_map<std::string, Attribute>& obj);


	// 类型检查
	bool isNull() const;
	bool isBool() const;
	bool isInt() const;
	bool isFloat() const;
	bool isString() const;
	bool isArray() const;
	bool isObject() const;

	// 深度比较两个 Attribute 是否相等（类型和值）
	bool equals(const Attribute& other) const;

	// 安全取值方法（抛出 TypeError 当类型不匹配）
	template<typename T>
	T as() const {
		try {
			return std::get<T>(value_);
		}
		catch (const std::bad_variant_access&) {
			throw TypeError("Invalid type conversion");
		}
	}

	// 智能数值转换：自动处理 int->float 转换，以及 string->float 转换
	float asFloat() const;
	int asInt() const;

	// 若需要按引用方式访问（避免复制），可使用下面的 helper（返回指针或 nullptr）
	const ObjectType* asObjectPtr() const;
	const std::string* asStringPtr() const;
	const std::vector<float>* asArrayPtr() const;

	// 便捷方法
	static Attribute MakeSize(float width, float height);

private:
	ValueType value_;

public:
	// 类型错误异常
	class TypeError : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};
};
