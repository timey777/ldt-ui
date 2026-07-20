# LDT DSL 语法参考（AI 版）

> 紧凑版，用于注入 AI 上下文。属性全集、语法规则、与标准 CSS 的关键差异。

> 📖 中文 | [English](LDT_DSL_AI_REFERENCE_EN.md)

---

## 1. 快速开始

LDT DSL 文件由三种块组成：

```ldt
@style    { /* 样式规则 */ }
@layout   { /* 布局规则 */ }
// 节点树（控件层级）
panel(id="root") {
    text(value="Hello")
}
```

`@style` 和 `@layout` 块必须出现在节点树之前。

---

## 2. 选择器速查

| 语法 | 匹配 | 示例 |
|------|------|------|
| `type` | 控件类型名 | `panel`, `text`, `button` |
| `.class` | class 属性值 | `.header`, `.active` |
| `#id` | id 属性值 | `#root` |
| `:pseudo` | UI 状态 | `:hover`, `:active`, `:focus`, `:disabled` |
| 组合 | 顺序连接 | `panel.login-panel:hover` |

**限制**：不支持后代/子代组合器（不能写 `panel .title` 或 `panel > .title`）。一个选择器只匹配单个节点。

---

## 3. 节点语法

```
type                       // 最简
type:id                    // 带 id
type(attr=val, ...)        // 带属性
type:id(attr=val, ...)     // 完整形式
```

**属性列表语法**（括号内，逗号分隔）：
```
key=value                  // 单值
key="string"               // 字符串
key=[item1, item2]         // 数组
key.subkey=value           // ❌ 不支持嵌套
```

**子节点语法**（花括号内，逗号分隔）：
```
type(attr) { child1, child2, child3(id="x") }
```

**属性值自动类型推断**：

| Token 形式 | 推断为 |
|-----------|--------|
| `42` | int |
| `3.14`, `1e5` | float |
| `"hello"` | string |
| `true`, `false` | bool |
| `100px`, `50%`, `auto` | string（带单位的数字保留为字符串） |
| 其他标识符 | string |

---

## 4. 单位体系

| 单位 | 含义 | 示例 |
|------|------|------|
| `px` | 像素 | `width: 100px` |
| `dp` | 密度无关像素（与 px 等价） | `width: 50dp` |
| `sp` | 缩放无关像素 | `font-size: 14sp` |
| `%` | 父容器百分比 | `width: 50%` |
| `auto` | 自动计算 | `width: auto` |
| 纯数字 | 默认视为 px | `width: 100` → 100px |

**颜色**：仅支持 `#RRGGBB` 或 `#RGB` 十六进制格式。

---

## 5. 样式属性全表

### 5.1 颜色与背景

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `background-color` | Color | `#00000000` (透明) | 否 | 背景色 |
| `color` | Color | `#000000` | **是** | 文字颜色 |
| `border-color` | Color | `#00000000` | 否 | 边框颜色 |
| `background-image` | string | `""` | 否 | 背景图片路径 |

### 5.2 边框

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `border` | shorthand | — | 否 | 简写：`"1px #ff0000"`（width color，忽略 solid 等关键字） |
| `border-width` | Edges | `0` | 否 | 四边统一宽度 |
| `border-width-top` | float | `0` | 否 | 上边框 |
| `border-width-right` | float | `0` | 否 | 右边框 |
| `border-width-bottom` | float | `0` | 否 | 下边框 |
| `border-width-left` | float | `0` | 否 | 左边框 |
| `border-radius` | px | `0` | 否 | 圆角 |

### 5.3 文字

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `font-size` | px | `14` | **是** | 字号 |
| `font-family` | string | `""` | **是** | 字体名 |
| `font-weight` | keyword | `normal` | **是** | `normal` / `bold` |
| `line-height` | px | `0` | **是** | 行高，0=自动 |
| `text-align` | keyword | `left` | **是** | `left` / `center` / `right` / `justify` |

### 5.4 盒模型

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `width` | Unit | `auto` | 否 | px/dp/sp/%/auto |
| `height` | Unit | `auto` | 否 | px/dp/sp/%/auto |
| `padding` | Edges | `0` | 否 | 四边内边距 |
| `padding-top/right/bottom/left` | px | `0` | 否 | 单边内边距 |
| `margin` | Edges | `0` | 否 | 四边外边距，支持 `auto` |
| `margin-top/right/bottom/left` | px | `0` | 否 | 单边外边距，支持 `auto` |
| `box-sizing` | keyword | `content-box` | 否 | `content-box` / `border-box` |
| `overflow` | keyword | `auto` | 否 | `visible` / `hidden` / `scroll` / `auto` |

### 5.5 视觉效果

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `opacity` | float | `1.0` | 否 | 不透明度 (0.0~1.0) |
| `visible` | bool | `true` | **是** | 可见性 |
| `box-shadow` | string | `""` | 否 | 阴影字符串 |
| `overlay` | bool | `false` | 否 | 是否在叠加层渲染（脱离文档流） |
| `anchor` | keyword | `none` | 否 | `none` / `top-left` / `center` / `top-right` / `bottom-left` / `bottom-right` |

---

## 6. 布局属性全表

这些属性既可在 `@layout` 块中使用，也可内联在节点属性中。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `display` | keyword | `block` | `block` / `inline` / `flex` / `grid` / `none` |
| `position` | keyword | `static` | `static` / `relative` / `absolute` / `fixed` |
| `flex-direction` | keyword | `row` | `row` / `column` / `row-reverse` / `column-reverse` |
| `align-items` | keyword | `stretch` | `stretch` / `flex-start` / `flex-end` / `center` / `baseline` |
| `justify-content` | keyword | `flex-start` | `flex-start` / `flex-end` / `center` / `space-between` / `space-around` / `space-evenly` |
| `flex-wrap` | keyword | `nowrap` | `nowrap` / `wrap` / `wrap-reverse` |
| `flex-grow` | float | `0` | 弹性增长比例 |
| `flex-shrink` | float | `1` | 弹性收缩比例 |
| `gap` | px | `0` | 子元素间距 |
| `grid-template-columns` | string | `""` | 网格列模板 |
| `grid-template-rows` | string | `""` | 网格行模板 |

---

## 7. Flex 实现细节与限制

### 7.1 已实现的特性

| 特性 | 状态 | 说明 |
|------|------|------|
| `flex-direction` | ✅ | row / column / row-reverse / column-reverse，反向正确 |
| `flex-wrap` | ✅ wrap/nowrap | wrap 换行正常；wrap-reverse 枚举存在但行堆叠顺序可能非标准 |
| `justify-content` | ✅ 全部 6 值 | flex-start / flex-end / center / space-between / space-around / space-evenly |
| `flex-grow` | ✅ | 按比例分配剩余空间，分配后触发子元素 remeasure |
| `flex-shrink` | ✅ | 按尺寸×shrink 加权收缩，收缩后触发 remeasure |
| `align-items: stretch` | ✅ | auto 尺寸子元素拉伸到行高/行宽 |
| `align-items: center` | ✅ | 交叉轴居中 |
| `align-items: flex-end` | ✅ | 交叉轴末尾对齐 |
| `align-items: flex-start` | ✅ | 交叉轴起始（默认 fallthrough） |
| `gap` | ✅ | 子元素间距，参与换行和空间分配 |
| 收缩下限保护 | ✅ | 列方向 auto-height 子元素 shrink 时有 content-based 最小高度；父容器 overflow:scroll/hidden 时忽略保护 |
| 嵌套 remeasure | ✅ | grow/shrink/stretch 后自动重新测量后代 |

### 7.2 ⚠️ 未实现 / 非标准行为

| 缺失特性 | 影响 | 替代方案 |
|----------|------|---------|
| **`align-items: baseline`** | 枚举存在但无代码路径，退化为 flex-start | 固定高度 + center 近似 |
| **`align-self`** | 无法单元素覆盖交叉轴对齐 | 包一层容器 |
| **`flex-basis`** | 无显式初始主轴尺寸 | 仅用 width/height 控制 |
| **`flex` 简写** | 不能 `flex: 1` | 分别写 `flex-grow` + `flex-shrink` |
| **`order`** | 无法改变视觉顺序 | 调整源码节点顺序 |
| **`align-content`** | 多行时交叉轴分布不可控 | 行仅简单堆叠 |
| **`min-width/height`** | 未暴露 DSL 属性 | 内部 clamp 固定 (0, FLT_MAX) |
| **`max-width/height`** | 同上 | 同上 |
| **`row-gap / column-gap`** | 仅单一 `gap` | — |
| **百分比 gap** | gap 仅绝对长度 | — |
| **`wrap-reverse`** | 枚举存在但交叉轴可能非标准 | 优先用 wrap |
| **flex 子元素 `margin:auto`** | 不吸收剩余空间 | 仅普通盒模型行为 |

---

## 8. 布局算法行为

| 布局模式 | display 值 | 行为 |
|---------|-----------|------|
| **Block** | `block`（默认） | 垂直堆叠。子元素宽度默认撑满父容器，高度由内容决定 |
| **Flex** | `flex` | 弹性布局（非标准 CSS Flexbox 子集）。详见第 7 节。不支持 baseline/align-self/flex-basis/order/align-content |
| **Inline** | `inline` | 水平流式排列。子元素自动换行，类似文字流 |
| **Grid** | `grid` | 网格布局（模板字符串定义） |
| **隐藏** | `none` | 不参与布局和渲染 |

盒模型（由外向内）：**margin → border → padding → content**

`box-sizing: border-box` 时，`width`/`height` 包含 padding 和 border。
`box-sizing: content-box`（默认）时，`width`/`height` 仅指 content 区域。

`overflow: scroll` 或 `auto` 的容器在内容溢出时提供滚动条和 `scrollWidth`/`scrollHeight`。

---

## 9. ⚠️ 与标准 CSS 的关键差异

| # | 场景 | ❌ 错误（CSS 习惯） | ✅ 正确（LDT DSL） |
|---|------|---------------------|---------------------|
| 1 | 后代选择器 | `.panel .title` | 为子节点单独设置 class，直接匹配 |
| 2 | 子代选择器 | `panel > text` | 同上，不支持组合器 |
| 3 | border 简写 | `border: 1px solid #ccc` | `border: 1px #cccccc`（solid 关键字被忽略） |
| 4 | em/rem 单位 | `font-size: 1.2em` | `font-size: 16px` 或 `font-size: 16sp` |
| 5 | vh/vw 单位 | `height: 100vh` | `height: 100%`（需父容器有明确高度） |
| 6 | flex 简写 | `flex: 1` | `flex-grow: 1; flex-shrink: 1` |
| 7 | flex-basis | `flex-basis: 200px` | ❌ 不支持；用 `width`/`height` 代替 |
| 8 | align-self | `align-self: center` | ❌ 不支持；包一层容器或统一用 align-items |
| 9 | order | `order: 1` | ❌ 不支持；调整节点源码顺序 |
| 10 | align-content | `align-content: space-between` | ❌ 不支持；多行仅简单堆叠 |
| 11 | align-items: baseline | `align-items: baseline` | ❌ 无效果，退化为 flex-start |
| 12 | position:absolute 坐标 | `position: absolute; top: 0; left: 0` | `overlay: true; anchor: top-left` |
| 13 | visibility/display:none | `visibility: hidden` | `visible: false` |
| 14 | display:none | `display: none` | `visible: false` 或 `display: none` |
| 15 | rgba() 颜色 | `rgba(0,0,0,0.5)` | `opacity: 0.5` + `background-color: #000000` |
| 16 | CSS 特异性计算 | 依赖选择器权重 | 规则按出现顺序覆盖（后面覆盖前面） |
| 17 | min/max-width/height | `min-width: 100px` | ❌ 不支持（内部固定 clamp 0~FLT_MAX） |
| 18 | 伪元素 | `::before`, `::after` | ❌ 不支持 |
| 19 | @media 查询 | `@media (max-width: 768px)` | ❌ 不支持 |
| 20 | transform/transition | `transform: rotate(45deg)` | ❌ 不支持 |
| 21 | z-index | `z-index: 10` | 无 z-index；用 `overlay: true` 提升到叠加层 |
| 22 | 颜色函数 | `rgb()`, `hsl()` | 仅 `#RRGGBB` 或 `#RGB` |
| 23 | border-style | `border-style: dashed` | ❌ 不支持（border 仅宽度+颜色） |
| 24 | background 简写 | `background: url(...) center` | `background-image: "path"`（仅图片路径） |

---

## 10. 完整示例

### 10.1 登录面板

```ldt
@style {
    .container {
        width: 100%;
        height: 100%;
        background-color: #f8f8f8;
    }
    .card {
        width: 320px;
        margin: auto;
        padding: 32px;
        background-color: #ffffff;
        border-radius: 12px;
    }
    .title {
        font-size: 22px;
        color: #333333;
        margin-bottom: 24px;
    }
    .field {
        width: 100%;
        padding: 10px;
        margin-bottom: 16px;
        border: 1px #cccccc;
        border-radius: 6px;
    }
    .btn {
        width: 100%;
        padding: 12px;
        background-color: #007bff;
        color: #ffffff;
        font-size: 16px;
        border-radius: 6px;
    }
}

@layout {
    .card {
        display: flex;
        flex-direction: column;
        align-items: stretch;
        justify-content: center;
    }
}

panel(class="container") {
    panel(class="card") {
        text(class="title", value="用户登录"),
        input(class="field", placeholder="用户名"),
        input(class="field", placeholder="密码", type="password"),
        button(class="btn") { text(value="登录") }
    }
}
```

### 10.2 Flex 横向列表

```ldt
@style {
    .row { width: 100%; padding: 16px; }
    .item {
        width: 80px; height: 80px;
        background-color: #e9ecef;
        border-radius: 8px;
    }
    .item-label { font-size: 12px; color: #666666; margin-top: 4px; }
}

@layout {
    .row {
        display: flex;
        flex-direction: row;
        gap: 12px;
        align-items: center;
    }
    .item {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
    }
}

panel(class="row") {
    panel(class="item") { text(class="item-label", value="A") },
    panel(class="item") { text(class="item-label", value="B") },
    panel(class="item") { text(class="item-label", value="C") },
    panel(class="item") { text(class="item-label", value="D") }
}
```

### 10.3 滚动容器

```ldt
@style {
    .scroll-box {
        width: 300px; height: 200px;
        background-color: #ffffff;
        border: 1px #ced4da;
        border-radius: 4px;
        padding: 10px;
        overflow: scroll;
    }
    .row-item {
        width: 100%; height: 40px;
        margin-bottom: 8px;
        background-color: #e9ecef;
        border-radius: 4px;
        padding: 8px;
    }
}

panel(class="scroll-box") {
    panel(class="row-item") { text(value="Item 1") },
    panel(class="row-item") { text(value="Item 2") },
    panel(class="row-item") { text(value="Item 3") },
    panel(class="row-item") { text(value="Item 4") },
    panel(class="row-item") { text(value="Item 5") },
    panel(class="row-item") { text(value="Item 6") }
}
```

---

## 附录：DSL 与 HTML+CSS 的关键差异（AI 生成必读）

以下差异是 DSL 设计层面的选择，与标准 CSS 行为不同，生成时必须注意。

### 1. ⭐ `overflow` 默认值为 `auto`（而非 CSS 的 `visible`）

标准 CSS 中块级元素默认 `overflow: visible`（内容溢出不会被裁剪）。
**LDT DSL 中默认 `overflow: auto`**，内容超出容器高度/宽度时自动出现滚动条。

```ldt
/* 未设置 overflow → 默认 auto */
.panel { height: 100px; }  
/* 内部子元素总高 > 100px 时 → 自动出现纵向滚动条 */
```

> 需要裁剪不显示滚动条：显式写 `overflow: hidden`
> 需要内容自由溢出：显式写 `overflow: visible`

### 2. ⭐ `height: 100%` + `padding` 必须配 `box-sizing: border-box`

这是 DSL 盒模型与标准 CSS 的**核心差异**。原因如下：

**CSS 标准盒模型（`content-box`）**：
```
height: 100%   → content 高度 = 父容器 content 高度的 100%
+ padding      → border-box 高度 = 100% + padding（超出父容器）
```

在标准 CSS 中，`height: 100%` 的子元素如果有 padding，会**撑破父容器**。但 Web 浏览器因 `overflow: visible` 默认行为，内容溢出通常仅视觉覆盖、不触发滚动条。

**在 LDT DSL 中**：
- `overflow` 默认 `auto` → 子元素一旦撑破父容器，**立即触发滚动条**
- 因此 `height: 100%` + `padding` 的组合几乎必然导致父容器出现滚动条

```ldt
/* ❌ 默认 content-box：border-box = 100% + 24px×2 → 撑破父容器，触发滚动条 */
.card { height: 100%; padding: 24px; }

/* ✅ border-box：height 包含 padding，border-box 精确 = 100%，不溢出 */
.card { height: 100%; padding: 24px; box-sizing: border-box; }
```

**结论**：在 DSL 中，只要用 `height: 100%`（或 `width: 100%`）的同时设了 `padding`，**一律加 `box-sizing: border-box`**，否则 100% + padding 必然溢出父级。

### 3. 固定尺寸容器需留意内容是否超出

因默认 `overflow: auto`，给容器写固定 `height` 或 `width` 时，若内部子元素总和超出该尺寸，会**自动出现滚动条**。不是 Bug，是设计行为。

```ldt
/* 若内容总高约 35px，固定 30px → 滚动条出现 */
.progress-area { height: 30px; }

/* 去掉固定高度或用 auto 让内容自然撑开 */
.progress-area { }
```

### 4. `background-image` 仅支持本地文件路径

不支持 HTTP URL。图片引擎通过本地文件系统加载。

```ldt
/* ❌ 不支持 */
background-image: "https://example.com/image.jpg";

/* ✅ 仅支持本地路径 */
background-image: "images/cover.png";
```

### 5. 中文字体不含 emoji 字形

内置字体 `AlibabaPuHuiTi` 不含 emoji。使用 emoji（🔀 ⏮ ⏸ 等）会显示为方块 `□`。用英文文字代替。

```ldt
/* ❌ 不显示 */    text(value="🔀")
/* ✅ 可显示 */    text(value="RDM")
```

### 6. 多层嵌套时逐层核算 content-box 高度

```
Root (800×600)
  └── Container (100% × 100%)        = 800×600
       └── Card (height:100%, box-sizing:border-box) = 360×600
            └── content-box = 600 - 32padding = 568px
                 └── 子元素总高约 572px → 超出 4px → 出现滚动条
```

生成多层嵌套 DSL 时，逐层计算 content-box 是否能容纳子元素总和。

---

## 附录：继承属性清单

以下属性会从父节点自动继承（子节点不设置时使用父节点值）：

`color`, `font-size`, `font-family`, `font-weight`, `line-height`, `text-align`, `visible`
