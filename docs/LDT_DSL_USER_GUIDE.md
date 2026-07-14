# LDT DSL 用户指南

> LDT（Layout Definition Template）是一种声明式 UI 描述语言，用于定义界面结构、样式和布局。

> 📖 中文 | [English](LDT_DSL_USER_GUIDE_EN.md)

---

## 1. 概述

一个 `.ldt` 文件从上到下包含三部分：

- **@style 块**：定义界面元素的视觉样式（颜色、字体、边框、间距等）
- **@layout 块**：定义界面元素的布局方式（排列方向、对齐、弹性伸缩等）
- **节点树**：定义界面的控件层级结构（嵌套关系）

三者的关系是：节点树描述「有哪些控件，谁包含谁」，@style 描述「控件长什么样」，@layout 描述「控件如何排列」。

---

## 2. 文件结构

一个完整的 `.ldt` 文件结构如下：

```ldt
@style {
    /* 样式规则 */
}

@layout {
    /* 布局规则 */
}

/* 节点树 */
```

`@style` 和 `@layout` 必须在节点树之前。如果不需要样式或布局，对应的块可以省略。

---

## 3. 选择器

选择器用于在 `@style` 和 `@layout` 块中指定规则应用于哪些节点。支持四种选择器类型：

### 3.1 类型选择器

直接使用控件类型名进行匹配：

```ldt
panel { ... }    /* 匹配所有 panel 控件 */
text { ... }     /* 匹配所有 text 控件 */
button { ... }   /* 匹配所有 button 控件 */
```

### 3.2 类选择器

以 `.` 开头，匹配节点的 `class` 属性：

```ldt
.header { ... }       /* 匹配 class 包含 "header" 的节点 */
.active { ... }       /* 匹配 class 包含 "active" 的节点 */
```

一个节点可以有多个 class（空格分隔），每个 class 都可以被独立匹配。

### 3.3 ID 选择器

以 `#` 开头，匹配节点的 `id` 属性：

```ldt
#root { ... }         /* 匹配 id 为 "root" 的节点 */
#submit-btn { ... }   /* 匹配 id 为 "submit-btn" 的节点 */
```

### 3.4 伪状态选择器

以 `:` 开头，匹配处于特定交互状态的节点：

| 伪状态 | 触发条件 |
|--------|---------|
| `:hover` | 鼠标悬停 |
| `:active` | 按下/激活状态 |
| `:focus` | 获得输入焦点 |
| `:disabled` | 禁用状态 |

```ldt
button:hover { background-color: #0056b3; }
input:focus { border-color: #007aff; }
```

### 3.5 组合使用

选择器可以组合多个条件：

```ldt
panel.login-panel { ... }      /* 类型 + 类 */
button.primary:hover { ... }   /* 类型 + 类 + 伪状态 */
```

---

## 4. 节点定义

### 4.1 基本语法

```
控件类型                         // 最简形式
控件类型:id                      // 指定标识符
控件类型(属性=值, ...)           // 设置属性
控件类型:id(属性=值, ...)        // 完整形式
```

**标识符（id）** 用 `:` 与类型分隔，如 `button:submitBtn`。id 在同一文件内应唯一，用于在代码中查找控件。

### 4.2 属性设置

属性在括号内以 `key=value` 形式给出，逗号分隔：

```ldt
text(value="Hello World", class="title")
input(placeholder="请输入", type="password")
panel(width=300, height=200, class="card")
```

**属性值支持以下类型：**

| 类型 | 写法 | 示例 |
|------|------|------|
| 整数 | 直接写数字 | `width=300` |
| 小数 | 带小数点 | `opacity=0.5` |
| 带单位的数值 | 数字+单位后缀 | `width=100px`, `width=50%` |
| 字符串 | 双引号包裹 | `value="Hello"` |
| 布尔值 | `true` 或 `false` | `visible=false` |
| 数组 | 方括号 | `items=[1,2,3]` |

字符串支持转义字符：`\n`（换行）、`\r`（回车）、`\t`（制表符）、`\"`（双引号）、`\\`（反斜杠）。

**特殊属性 `class`**：class 属性支持空格分隔多个类名：
```ldt
panel(class="card active highlight")
```

### 4.3 子节点

使用花括号 `{}` 包含子节点，子节点之间用逗号分隔：

```ldt
panel(class="container") {
    text(value="标题", class="title"),
    panel(class="body") {
        input(placeholder="输入内容"),
        button() { text(value="提交") }
    }
}
```

---

## 5. 单位系统

LDT DSL 支持以下五种单位：

| 单位 | 说明 | 适用场景 |
|------|------|---------|
| `px` | 像素 | 精确尺寸控制 |
| `dp` | 密度无关像素 | 跨屏幕密度一致的尺寸 |
| `sp` | 缩放无关像素 | 字体大小（可随系统缩放） |
| `%` | 百分比 | 相对父容器尺寸 |
| `auto` | 自动计算 | 根据内容或父容器自动决定 |

数值不写单位时默认视为 `px`（像素）。例如 `width=100` 等价于 `width=100px`。

**颜色值**使用十六进制格式：`#RRGGBB`（如 `#ff0000` 为红色）或 `#RGB`（如 `#f00` 也为红色）。支持透明度通道的格式为 `#RRGGBBAA`。

---

## 6. 样式属性

样式属性在 `@style` 块中使用，也可以内联在节点的括号属性中。

### 6.1 颜色

| 属性 | 说明 | 示例 |
|------|------|------|
| `background-color` | 背景颜色 | `background-color: #f0f0f0` |
| `color` | 文字颜色 | `color: #333333` |
| `border-color` | 边框颜色 | `border-color: #cccccc` |

### 6.2 边框

| 属性 | 说明 | 示例 |
|------|------|------|
| `border` | 边框简写（宽度+颜色） | `border: 1px #eeeeee` |
| `border-width` | 四边等宽设置 | `border-width: 2px` |
| `border-width-top` | 上边框宽度 | `border-width-top: 1px` |
| `border-width-right` | 右边框宽度 | `border-width-right: 2px` |
| `border-width-bottom` | 下边框宽度 | `border-width-bottom: 1px` |
| `border-width-left` | 左边框宽度 | `border-width-left: 2px` |
| `border-radius` | 圆角半径 | `border-radius: 8px` |

### 6.3 文字排版

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `font-size` | 字号 | 数值+单位 | `font-size: 16px` |
| `font-family` | 字体名称 | 字符串 | `font-family: "Arial"` |
| `font-weight` | 字重 | `normal`, `bold` | `font-weight: bold` |
| `line-height` | 行高 | 数值+单位 | `line-height: 24px` |
| `text-align` | 文字对齐 | `left`, `center`, `right`, `justify` | `text-align: center` |

以下属性支持**继承**：当子节点不设置时，自动使用父节点的值。这些属性包括 `color`、`font-size`、`font-family`、`font-weight`、`line-height`、`text-align`。

### 6.4 盒模型（尺寸与间距）

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `width` | 宽度 | 数值+单位/`auto` | `width: 300px` |
| `height` | 高度 | 数值+单位/`auto` | `height: 100%` |
| `padding` | 四边内边距 | 数值 | `padding: 16px` |
| `padding-top` | 上内边距 | 数值 | `padding-top: 8px` |
| `padding-right` | 右内边距 | 数值 | `padding-right: 12px` |
| `padding-bottom` | 下内边距 | 数值 | `padding-bottom: 8px` |
| `padding-left` | 左内边距 | 数值 | `padding-left: 12px` |
| `margin` | 四边外边距 | 数值/`auto` | `margin: 10px` |
| `margin-top` | 上外边距 | 数值/`auto` | `margin-top: auto` |
| `margin-right` | 右外边距 | 数值/`auto` | `margin-right: 20px` |
| `margin-bottom` | 下外边距 | 数值/`auto` | `margin-bottom: auto` |
| `margin-left` | 左外边距 | 数值/`auto` | `margin-left: 20px` |
| `box-sizing` | 尺寸计算模式 | `content-box`, `border-box` | `box-sizing: border-box` |
| `overflow` | 溢出处理 | `visible`, `hidden`, `scroll`, `auto` | `overflow: scroll` |

`margin` 支持 `auto` 值，可用于水平居中（左右均设为 `auto`）。

`box-sizing: border-box` 表示 `width`/`height` 包含 padding 和 border；`content-box`（默认）表示仅包含内容区。

`overflow: scroll` 或 `auto` 会在内容超出容器时显示滚动条。

### 6.5 视觉效果

| 属性 | 说明 | 示例 |
|------|------|------|
| `opacity` | 不透明度（0.0=全透明，1.0=不透明） | `opacity: 0.8` |
| `visible` | 是否可见 | `visible: false` |
| `box-shadow` | 阴影效果 | `box-shadow: "0 2px 8px #00000040"` |
| `background-image` | 背景图片路径 | `background-image: "images/bg.png"` |

### 6.6 叠加层与定位

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `overlay` | 是否在叠加层渲染（脱离正常文档流） | `true`, `false` | `overlay: true` |
| `anchor` | 叠加层锚点位置 | `none`, `top-left`, `center`, `top-right`, `bottom-left`, `bottom-right` | `anchor: center` |

`overlay: true` 使节点脱离正常的布局流，显示在其他内容之上。配合 `anchor` 可以将其定位到父容器的特定位置。

---

## 7. 布局属性

布局属性在 `@layout` 块中使用，也可以内联在节点的括号属性中。

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `display` | 布局模式 | `block`, `inline`, `flex`, `grid`, `none` | `display: flex` |
| `position` | 定位方式 | `static`, `relative`, `absolute`, `fixed` | `position: absolute` |

### 7.1 Flex 布局属性

当 `display` 设为 `flex` 时，以下属性控制弹性布局行为：

| 属性 | 说明 | 可选值 | 默认值 |
|------|------|--------|--------|
| `flex-direction` | 主轴方向 | `row`, `column`, `row-reverse`, `column-reverse` | `row` |
| `align-items` | 交叉轴对齐 | `stretch`, `flex-start`, `flex-end`, `center`, `baseline` | `stretch` |
| `justify-content` | 主轴对齐 | `flex-start`, `flex-end`, `center`, `space-between`, `space-around`, `space-evenly` | `flex-start` |
| `flex-wrap` | 是否换行 | `nowrap`, `wrap`, `wrap-reverse` | `nowrap` |
| `flex-grow` | 弹性增长比例 | 数值 | `0` |
| `flex-shrink` | 弹性收缩比例 | 数值 | `1` |
| `gap` | 子元素间距 | 数值+单位 | `0` |

### 7.2 Grid 布局属性

当 `display` 设为 `grid` 时：

| 属性 | 说明 | 示例 |
|------|------|------|
| `grid-template-columns` | 列模板 | `grid-template-columns: "1fr 2fr 1fr"` |
| `grid-template-rows` | 行模板 | `grid-template-rows: "auto 1fr auto"` |

---

## 8. 布局模型

### 8.1 Block 布局（默认）

`display: block` 是默认的布局模式。特性：

- 子元素**垂直堆叠**，一个接一个向下排列
- 子元素宽度默认**撑满父容器**的内容宽度
- 高度由子元素内容决定

适用于页面整体结构、卡片列表等自上而下的排列场景。

### 8.2 Flex 布局

`display: flex` 启用弹性布局。**注意**：LDT 的 Flex 实现是一个精选特性子集，包含最常用的 Flex 功能，但并非完整的 CSS Flexbox 标准实现。

#### 已支持的特性

**方向控制：**
- 四种主轴方向：`row`（水平从左到右）、`column`（垂直从上到下）、`row-reverse`、`column-reverse`
- 反向布局正确渲染

**换行：**
- `nowrap`（默认）：单行排列，子元素可能溢出容器
- `wrap`：超出容器宽度/高度时自动换到下一行/列

**主轴对齐（justify-content）— 全部 6 种模式：**
- `flex-start` — 起始端对齐
- `flex-end` — 末尾端对齐
- `center` — 居中对齐
- `space-between` — 两端对齐，中间均匀分布
- `space-around` — 每个子元素两侧有相等间距
- `space-evenly` — 所有间距（含首尾）完全相等

**交叉轴对齐（align-items）— 4 种模式：**
- `stretch`（默认）— 自动拉伸填满行高/行宽，仅对未设固定尺寸的子元素生效
- `flex-start` — 起始端对齐
- `flex-end` — 末尾端对齐
- `center` — 居中对齐

**弹性伸缩：**
- `flex-grow`：弹性增长比例。容器有剩余空间时，按 grow 值比例分配给子元素。默认 0（不增长）
- `flex-shrink`：弹性收缩比例。容器空间不足时，按「当前尺寸 × shrink」加权收缩。默认 1（允许收缩）
- 收缩保护机制：列方向下未设固定高度的子元素不会收缩到内容最小高度以下，防止文字被截断
- 伸缩发生后自动重新测量受影响的子元素及其后代

**间距：**
- `gap`：统一设置子元素之间的间距

#### 不支持的特性

以下 Flex 特性**暂未实现**：

| 不支持的特性 | 说明 |
|-------------|------|
| `align-items: baseline` | 无法按文字基线对齐（枚举值存在但实际无效果） |
| `align-self` | 无法对单个子元素单独设置交叉轴对齐方式 |
| `flex-basis` | 无独立的初始主轴尺寸属性，子元素尺寸仅由 `width`/`height` 决定 |
| `flex` 简写属性 | 不能写 `flex: 1`，需要分别写 `flex-grow` 和 `flex-shrink` |
| `order` | 无法改变子元素的视觉排列顺序，顺序由源码位置决定 |
| `align-content` | 多行时行与行之间的交叉轴分布不可控，仅简单堆叠 |
| `min-width / max-width` | 最小/最大宽度约束未暴露为 DSL 属性 |
| `min-height / max-height` | 最小/最大高度约束未暴露为 DSL 属性 |

适用于工具栏、导航栏、卡片网格等需要灵活对齐的场景。

### 8.3 Inline 布局

`display: inline` 启用行内布局。特性：

- 子元素**水平排列**，类似文字流
- 超出容器宽度时**自动换行**
- 每个子元素保持自身固有宽度

适用于标签组、文字+图标混合排列等场景。

### 8.4 盒模型

每个控件都是一个矩形盒子，从外到内分为四层：

```
┌─────────────────────────────┐
│         margin              │  ← 外边距（与其他控件的间距）
│  ┌───────────────────────┐  │
│  │       border          │  │  ← 边框
│  │  ┌─────────────────┐  │  │
│  │  │    padding      │  │  │  ← 内边距（边框与内容之间的留白）
│  │  │  ┌───────────┐  │  │  │
│  │  │  │  content  │  │  │  │  ← 内容区（文字、图片等实际内容）
│  │  │  └───────────┘  │  │  │
│  │  └─────────────────┘  │  │
│  └───────────────────────┘  │
└─────────────────────────────┘
```

- `box-sizing: content-box`（默认）：`width`/`height` 只控制 content 区域
- `box-sizing: border-box`：`width`/`height` 包含 padding 和 border，更直观

---

## 9. 完整示例

### 9.1 简单登录面板

```ldt
@style {
    .page {
        width: 100%;
        height: 100%;
        background-color: #f5f5f5;
    }
    .card {
        width: 320px;
        margin: auto;
        padding: 40px 32px;
        background-color: #ffffff;
        border-radius: 12px;
        box-shadow: "0 2px 12px #00000020";
    }
    .title {
        font-size: 24px;
        font-weight: bold;
        color: #333333;
        text-align: center;
        margin-bottom: 32px;
    }
    .input-field {
        width: 100%;
        padding: 12px;
        margin-bottom: 16px;
        border: 1px #dddddd;
        border-radius: 8px;
        font-size: 14px;
    }
    .login-btn {
        width: 100%;
        padding: 14px;
        background-color: #007aff;
        color: #ffffff;
        font-size: 16px;
        font-weight: bold;
        border-radius: 8px;
    }
}

@layout {
    .page {
        display: flex;
        align-items: center;
        justify-content: center;
    }
    .card {
        display: flex;
        flex-direction: column;
        align-items: stretch;
    }
}

panel(class="page") {
    panel(class="card") {
        text(class="title", value="账号登录"),
        input:username(class="input-field", placeholder="请输入用户名"),
        input:password(class="input-field", placeholder="请输入密码", type="password"),
        button:loginBtn(class="login-btn") { text(value="登 录") }
    }
}
```

### 9.2 横向商品列表

```ldt
@style {
    .list-container {
        width: 100%;
        padding: 16px;
        background-color: #ffffff;
    }
    .product-card {
        width: 120px;
        padding: 12px;
        background-color: #f8f9fa;
        border-radius: 8px;
    }
    .product-image {
        width: 100px;
        height: 100px;
        background-color: #e9ecef;
        border-radius: 6px;
    }
    .product-name {
        font-size: 13px;
        color: #333333;
        margin-top: 8px;
    }
    .product-price {
        font-size: 15px;
        font-weight: bold;
        color: #ff3b30;
        margin-top: 4px;
    }
}

@layout {
    .list-container {
        display: flex;
        flex-direction: row;
        gap: 12px;
        overflow: scroll;
    }
    .product-card {
        display: flex;
        flex-direction: column;
        align-items: center;
    }
}

panel(class="list-container") {
    panel(class="product-card") {
        image(class="product-image", src="p1.png"),
        text(class="product-name", value="商品名称"),
        text(class="product-price", value="¥99")
    },
    panel(class="product-card") {
        image(class="product-image", src="p2.png"),
        text(class="product-name", value="商品名称"),
        text(class="product-price", value="¥199")
    },
    panel(class="product-card") {
        image(class="product-image", src="p3.png"),
        text(class="product-name", value="商品名称"),
        text(class="product-price", value="¥299")
    }
}
```

### 9.3 带滚动区域的页面框架

```ldt
@style {
    * {
        box-sizing: border-box;
    }
    .app {
        width: 100%;
        height: 100%;
        background-color: #f2f2f7;
    }
    .navbar {
        width: 100%;
        height: 56px;
        padding: 0 16px;
        background-color: #ffffff;
        border-bottom-width: 1px;
        border-color: #e5e5ea;
    }
    .navbar-title {
        font-size: 18px;
        font-weight: bold;
        color: #000000;
    }
    .content {
        width: 100%;
        padding: 16px;
        overflow: auto;
    }
    .tabbar {
        width: 100%;
        height: 50px;
        background-color: #ffffff;
        border-top-width: 1px;
        border-color: #e5e5ea;
    }
    .tab-item {
        font-size: 11px;
        color: #8e8e93;
    }
    .tab-item.active {
        color: #007aff;
    }
}

@layout {
    .app {
        display: flex;
        flex-direction: column;
    }
    .navbar {
        display: flex;
        flex-direction: row;
        align-items: center;
    }
    .content {
        flex-grow: 1;
        flex-shrink: 1;
    }
    .tabbar {
        display: flex;
        flex-direction: row;
        justify-content: space-around;
        align-items: center;
    }
    .tab-item {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
    }
}

panel(class="app") {
    panel(class="navbar") {
        text(class="navbar-title", value="应用标题")
    },
    panel(class="content") {
        /* 可滚动的内容区域 */
        text(value="这里是可滚动的内容...")
    },
    panel(class="tabbar") {
        panel(class="tab-item active") { text(value="首页") },
        panel(class="tab-item") { text(value="发现") },
        panel(class="tab-item") { text(value="消息") },
        panel(class="tab-item") { text(value="我的") }
    }
}
```

---

## 10. 常用控件类型

| 类型 | 说明 | 常用属性 |
|------|------|---------|
| `panel` | 容器/面板 | 通用样式和布局属性 |
| `text` | 文本标签 | `value`（文本内容） |
| `button` | 按钮 | 通过子 `text` 节点设置文字 |
| `input` | 输入框 | `placeholder`（占位文字）, `type`（类型） |
| `image` | 图片 | `src`（图片路径） |
| `scrollbar` | 滚动条 | 由引擎自动管理，一般不需要手动创建 |

---

## 附录：可继承属性

以下样式属性在子节点不设置时会自动从父节点继承：

- `color` — 文字颜色
- `font-size` — 字号
- `font-family` — 字体
- `font-weight` — 字重
- `line-height` — 行高
- `text-align` — 文字对齐
- `visible` — 可见性

这意味着如果你在父容器上设置了 `font-size: 16px`，所有子节点的文字默认都是 16px，除非子节点显式覆盖。
