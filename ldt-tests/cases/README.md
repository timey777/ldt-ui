# LDT Test Registry

每个测试只测一个核心行为。新增测试前先查此表。

## 两层测试架构

```
ldt_test_runner
  ├─ Unit Tests  (cases/unit/*.cpp)     — C++ TEST_CASE 宏，保护底层算法
  └─ Golden Tests (cases/golden/*/)     — case.ldt + expected.json，保护用户行为
```

> **Golden 测试**：运行 LDT 管线后提取属性值，与 expected.json 比较。
> 新增 golden 测试只需建目录放 `.ldt` + `.json`，无需写 C++。

---

## Unit Tests — C++ 行为测试

### layout/ — 布局行为

| 测试 | 验证内容 |
|------|---------|
| `layout/flex_basic` | flex-direction:row 子节点水平排列 |
| `layout/flex_shrink` | flex-shrink 按比例收缩 |

### text/ — 文本测量行为

| 测试 | 验证内容 |
|------|---------|
| `text/measure` | 文本有非零宽高 |

### ui/ — 用户交互行为

| 测试 | 验证内容 |
|------|---------|
| `ui/input_multiline_wrap` | 多行输入框文本根据宽度换行（height > 单行） |

---

## Golden Tests — LDT 场景测试

### input/

| 测试 | 验证内容 |
|------|---------|
| `golden/input/multiline_wrap` | 多行输入框 computedHeight > 25 |

---

## 如何新增测试

### 新增 Unit Test
在 `cases/unit/` 下建 `.cpp` 文件：
```cpp
#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

TEST_CASE("category/name") {
    TestEnv env;
    auto* root = env.load(R"( ... LDT DSL ... )");
    EXPECT_GT(root->layout.computedHeight, 0);
}
```

### 新增 Golden Test
在 `cases/golden/` 下建目录：
```
golden/category/name/
  ├ case.ldt          # LDT 描述
  └ expected.json     # 期望结果
```
```json
{
    "selectors": {
        "#myId": {
            "computedWidth": 100,
            "computedHeight": { "gt": 25 }
        }
    }
}
```
支持运算符：`"gt"`, `"lt"`, `"eq"`，或直接写数值（±0.5 容差）。
