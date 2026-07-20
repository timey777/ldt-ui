# LDT DSL Syntax Reference (AI Edition)

> Compact version for AI context injection. Full property set, syntax rules, and key differences from standard CSS.

> 📖 [中文版](LDT_DSL_AI_REFERENCE.md) | English

---

## 1. Quick Start

An LDT DSL file consists of three kinds of blocks:

```ldt
@style    { /* Style rules */ }
@layout   { /* Layout rules */ }
// Node tree (widget hierarchy)
panel(id="root") {
    text(value="Hello")
}
```

`@style` and `@layout` blocks must appear before the node tree.

---

## 2. Selector Quick Reference

| Syntax | Matches | Example |
|--------|---------|---------|
| `type` | Widget type name | `panel`, `text`, `button` |
| `.class` | class attribute value | `.header`, `.active` |
| `#id` | id attribute value | `#root` |
| `:pseudo` | UI state | `:hover`, `:active`, `:focus`, `:disabled` |
| Combined | Concatenated in order | `panel.login-panel:hover` |

**Limitation**: Descendant/child combinators are not supported (no `panel .title` or `panel > .title`). A selector matches only a single node.

---

## 3. Node Syntax

```
type                       // Simplest
type:id                    // With id
type(attr=val, ...)        // With attributes
type:id(attr=val, ...)     // Full form
```

**Attribute list syntax** (inside parentheses, comma-separated):
```
key=value                  // Single value
key="string"               // String
key=[item1, item2]         // Array
key.subkey=value           // ❌ Not supported (no nesting)
```

**Child node syntax** (inside curly braces, comma-separated):
```
type(attr) { child1, child2, child3(id="x") }
```

**Automatic value type inference:**

| Token Form | Inferred As |
|------------|-------------|
| `42` | int |
| `3.14`, `1e5` | float |
| `"hello"` | string |
| `true`, `false` | bool |
| `100px`, `50%`, `auto` | string (valued numbers with unit kept as string) |
| Other identifiers | string |

---

## 4. Unit System

| Unit | Meaning | Example |
|------|---------|---------|
| `px` | Pixels | `width: 100px` |
| `dp` | Density-independent pixels (equivalent to px) | `width: 50dp` |
| `sp` | Scale-independent pixels | `font-size: 14sp` |
| `%` | Percentage of parent | `width: 50%` |
| `auto` | Automatic calculation | `width: auto` |
| Bare number | Defaults to px | `width: 100` → 100px |

**Colors**: Only `#RRGGBB` or `#RGB` hex format is supported.

---

## 5. Style Properties — Complete Table

### 5.1 Color & Background

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `background-color` | Color | `#00000000` (transparent) | No | Background color |
| `color` | Color | `#000000` | **Yes** | Text color |
| `border-color` | Color | `#00000000` | No | Border color |
| `background-image` | string | `""` | No | Background image path |

### 5.2 Borders

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `border` | shorthand | — | No | Shorthand: `"1px #ff0000"` (width color; keywords like `solid` are ignored) |
| `border-width` | Edges | `0` | No | Uniform width for all four sides |
| `border-width-top` | float | `0` | No | Top border |
| `border-width-right` | float | `0` | No | Right border |
| `border-width-bottom` | float | `0` | No | Bottom border |
| `border-width-left` | float | `0` | No | Left border |
| `border-radius` | px | `0` | No | Corner radius |

### 5.3 Typography

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `font-size` | px | `14` | **Yes** | Font size |
| `font-family` | string | `""` | **Yes** | Font name |
| `font-weight` | keyword | `normal` | **Yes** | `normal` / `bold` |
| `line-height` | px | `0` | **Yes** | Line height; 0 = auto |
| `text-align` | keyword | `left` | **Yes** | `left` / `center` / `right` / `justify` |

### 5.4 Box Model

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `width` | Unit | `auto` | No | px/dp/sp/%/auto |
| `height` | Unit | `auto` | No | px/dp/sp/%/auto |
| `padding` | Edges | `0` | No | All four sides padding |
| `padding-top/right/bottom/left` | px | `0` | No | Per-side padding |
| `margin` | Edges | `0` | No | All four sides margin; supports `auto` |
| `margin-top/right/bottom/left` | px | `0` | No | Per-side margin; supports `auto` |
| `box-sizing` | keyword | `content-box` | No | `content-box` / `border-box` |
| `overflow` | keyword | `auto` | No | `visible` / `hidden` / `scroll` / `auto` |

### 5.5 Visual Effects

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `opacity` | float | `1.0` | No | Opacity (0.0 ~ 1.0) |
| `visible` | bool | `true` | **Yes** | Visibility |
| `box-shadow` | string | `""` | No | Shadow string |
| `overlay` | bool | `false` | No | Render in overlay layer (out of flow) |
| `anchor` | keyword | `none` | No | `none` / `top-left` / `center` / `top-right` / `bottom-left` / `bottom-right` |

---

## 6. Layout Properties — Complete Table

These can be used in `@layout` blocks or inlined in node attributes.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `display` | keyword | `block` | `block` / `inline` / `flex` / `grid` / `none` |
| `position` | keyword | `static` | `static` / `relative` / `absolute` / `fixed` |
| `flex-direction` | keyword | `row` | `row` / `column` / `row-reverse` / `column-reverse` |
| `align-items` | keyword | `stretch` | `stretch` / `flex-start` / `flex-end` / `center` / `baseline` |
| `justify-content` | keyword | `flex-start` | `flex-start` / `flex-end` / `center` / `space-between` / `space-around` / `space-evenly` |
| `flex-wrap` | keyword | `nowrap` | `nowrap` / `wrap` / `wrap-reverse` |
| `flex-grow` | float | `0` | Flex grow ratio |
| `flex-shrink` | float | `1` | Flex shrink ratio |
| `gap` | px | `0` | Child spacing |
| `grid-template-columns` | string | `""` | Grid column template |
| `grid-template-rows` | string | `""` | Grid row template |

---

## 7. Flex Implementation Details & Limitations

### 7.1 Implemented Features

| Feature | Status | Notes |
|---------|--------|-------|
| `flex-direction` | ✅ | row / column / row-reverse / column-reverse; reverse works correctly |
| `flex-wrap` | ✅ wrap/nowrap | Wrap works; wrap-reverse enum exists but row stacking order may be non-standard |
| `justify-content` | ✅ All 6 values | flex-start / flex-end / center / space-between / space-around / space-evenly |
| `flex-grow` | ✅ | Proportional distribution of remaining space; triggers child remeasure after allocation |
| `flex-shrink` | ✅ | Weighted shrink by size × shrink; triggers remeasure after shrink |
| `align-items: stretch` | ✅ | Auto-sized children stretched to row height/width |
| `align-items: center` | ✅ | Cross-axis centering |
| `align-items: flex-end` | ✅ | Cross-axis end alignment |
| `align-items: flex-start` | ✅ | Cross-axis start (default fallthrough) |
| `gap` | ✅ | Child spacing; participates in wrap and space allocation |
| Shrink floor protection | ✅ | Column-direction auto-height children have content-based minimum height during shrink; ignored when parent has overflow:scroll/hidden |
| Nested remeasure | ✅ | Auto-remeasure descendants after grow/shrink/stretch |

### 7.2 ⚠️ Not Implemented / Non-Standard Behavior

| Missing Feature | Impact | Workaround |
|----------------|--------|------------|
| **`align-items: baseline`** | Enum exists but no code path; falls back to flex-start | Fixed height + center approximation |
| **`align-self`** | Cannot override cross-axis alignment per child | Wrap in a container |
| **`flex-basis`** | No explicit initial main-axis size | Use width/height only |
| **`flex` shorthand** | Cannot use `flex: 1` | Write `flex-grow` + `flex-shrink` separately |
| **`order`** | Cannot change visual order | Reorder nodes in source |
| **`align-content`** | Multi-line cross-axis distribution uncontrollable | Rows simply stack |
| **`min-width/height`** | Not exposed as DSL properties | Internal clamp is fixed (0, FLT_MAX) |
| **`max-width/height`** | Same as above | Same as above |
| **`row-gap / column-gap`** | Only single `gap` | — |
| **Percentage gap** | gap is absolute length only | — |
| **`wrap-reverse`** | Enum exists but cross-axis may be non-standard | Prefer wrap |
| **Flex child `margin:auto`** | Does not absorb remaining space | Normal box-model behavior only |

---

## 8. Layout Algorithm Behavior

| Layout Mode | display value | Behavior |
|------------|---------------|----------|
| **Block** | `block` (default) | Vertically stacked. Children fill parent width by default; height is content-driven |
| **Flex** | `flex` | Flexbox layout (non-standard CSS subset). See §7. Does not support baseline/align-self/flex-basis/order/align-content |
| **Inline** | `inline` | Horizontal flow layout. Children wrap automatically, similar to text flow |
| **Grid** | `grid` | Grid layout (template string defined) |
| **Hidden** | `none` | Excluded from layout and rendering |

Box model (outside → inside): **margin → border → padding → content**

With `box-sizing: border-box`, `width`/`height` include padding and border.
With `box-sizing: content-box` (default), `width`/`height` refer only to the content area.

Containers with `overflow: scroll` or `auto` provide scrollbars and `scrollWidth`/`scrollHeight` when content overflows.

---

## 9. ⚠️ Key Differences from Standard CSS

| # | Scenario | ❌ Wrong (CSS habit) | ✅ Correct (LDT DSL) |
|---|----------|---------------------|---------------------|
| 1 | Descendant selector | `.panel .title` | Assign a class directly to the child node; combinators not supported |
| 2 | Child selector | `panel > text` | Same as above |
| 3 | Border shorthand | `border: 1px solid #ccc` | `border: 1px #cccccc` (solid keyword is ignored) |
| 4 | em/rem units | `font-size: 1.2em` | `font-size: 16px` or `font-size: 16sp` |
| 5 | vh/vw units | `height: 100vh` | `height: 100%` (parent must have a definite height) |
| 6 | Flex shorthand | `flex: 1` | `flex-grow: 1; flex-shrink: 1` |
| 7 | flex-basis | `flex-basis: 200px` | ❌ Not supported; use `width`/`height` instead |
| 8 | align-self | `align-self: center` | ❌ Not supported; wrap in a container or use align-items on parent |
| 9 | order | `order: 1` | ❌ Not supported; reorder nodes in source |
| 10 | align-content | `align-content: space-between` | ❌ Not supported; multi-line simply stacks |
| 11 | align-items: baseline | `align-items: baseline` | ❌ No effect, falls back to flex-start |
| 12 | position:absolute coordinates | `position: absolute; top: 0; left: 0` | `overlay: true; anchor: top-left` |
| 13 | visibility/display:none | `visibility: hidden` | `visible: false` |
| 14 | display:none | `display: none` | `visible: false` or `display: none` |
| 15 | rgba() colors | `rgba(0,0,0,0.5)` | `opacity: 0.5` + `background-color: #000000` |
| 16 | CSS specificity | Cascade by selector weight | Rules override in declaration order (last wins) |
| 17 | min/max-width/height | `min-width: 100px` | ❌ Not supported (internal clamp is 0~FLT_MAX) |
| 18 | Pseudo-elements | `::before`, `::after` | ❌ Not supported |
| 19 | @media queries | `@media (max-width: 768px)` | ❌ Not supported |
| 20 | transform/transition | `transform: rotate(45deg)` | ❌ Not supported |
| 21 | z-index | `z-index: 10` | No z-index; use `overlay: true` to elevate to overlay layer |
| 22 | Color functions | `rgb()`, `hsl()` | Only `#RRGGBB` or `#RGB` |
| 23 | border-style | `border-style: dashed` | ❌ Not supported (border is width + color only) |
| 24 | background shorthand | `background: url(...) center` | `background-image: "path"` (path only) |

---

## 10. Complete Examples

### 10.1 Login Panel

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
        text(class="title", value="User Login"),
        input(class="field", placeholder="Username"),
        input(class="field", placeholder="Password", type="password"),
        button(class="btn") { text(value="Login") }
    }
}
```

### 10.2 Flex Horizontal List

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

### 10.3 Scrollable Container

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

## 11. DSL vs HTML+CSS — Key Design Differences (AI Must-Read)

The following differences are by design in the DSL engine, unlike standard CSS behavior. Keep them in mind when generating DSL.

### 11.1 ⭐ Default `overflow` is `auto` (CSS default is `visible`)

In standard CSS, block elements default to `overflow: visible` (content overflows without clipping).
**LDT DSL defaults to `overflow: auto`** — if content exceeds the container's width/height, a scrollbar appears automatically.

```ldt
/* No overflow set → defaults to auto */
.panel { height: 100px; }  
/* If children exceed 100px → vertical scrollbar appears */
```

> To clip without scrollbar: explicitly write `overflow: hidden`
> To allow free overflow: explicitly write `overflow: visible`

### 11.2 ⭐ `height: 100%` + `padding` requires `box-sizing: border-box`

This is a **core design difference** between the DSL box model and standard CSS. Here's why:

**Standard CSS content-box model:**
```
height: 100%   → content height = 100% of parent's content height
+ padding      → border-box height = 100% + padding (overflows parent)
```

In standard CSS, a child with `height: 100%` and padding **overflows its parent**. But because CSS defaults to `overflow: visible`, the overflow is merely visual — it does NOT trigger a scrollbar.

**In LDT DSL:**
- `overflow` defaults to `auto` → as soon as the child overflows the parent, **a scrollbar appears immediately**
- Therefore `height: 100%` + `padding` almost always produces an unwanted scrollbar on the parent

```ldt
/* ❌ content-box default: border-box = 100% + 24px×2 → overflows parent, triggers scrollbar */
.card { height: 100%; padding: 24px; }

/* ✅ border-box: height includes padding, border-box exactly = 100%, no overflow */
.card { height: 100%; padding: 24px; box-sizing: border-box; }
```

**Rule**: Whenever you use `height: 100%` (or `width: 100%`) together with `padding`, **always add `box-sizing: border-box`**, otherwise 100% + padding will inevitably overflow the parent.

### 11.3 Fixed-size containers must account for content

Because `overflow` defaults to `auto`, if you set a fixed `height` or `width` on a container whose children exceed that size, a **scrollbar will appear automatically**. This is by design, not a bug.

```ldt
/* If content is ~35px tall, fixing height to 30px → scrollbar appears */
.progress-area { height: 30px; }

/* Remove fixed height or use auto to let content determine the size */
.progress-area { }
```

### 11.4 `background-image` only supports local file paths

HTTP URLs are not supported. Images are loaded from the local filesystem.

```ldt
/* ❌ Not supported */
background-image: "https://example.com/image.jpg";

/* ✅ Local path only */
background-image: "images/cover.png";
```

### 11.5 CJK fonts do not include emoji glyphs

The built-in font `AlibabaPuHuiTi` does not contain emoji. Using emoji characters (🔀 ⏮ ⏸ etc.) will render as tofu boxes `□`. Use English text instead.

```ldt
/* ❌ Not rendered */    text(value="🔀")
/* ✅ Works */            text(value="RDM")
```

### 11.6 Nested containers: calculate content-box height layer by layer

```
Root (800×600)
  └── Container (100% × 100%)        = 800×600
       └── Card (height:100%, box-sizing:border-box) = 360×600
            └── content-box = 600 - 32padding = 568px
                 └── children total ~572px → overflow 4px → scrollbar appears
```

When generating multi-layer nested DSL, calculate layer by layer whether the content-box can accommodate the sum of its children.

---

## Appendix: Inherited Properties

The following properties are automatically inherited from the parent node (if not explicitly set on the child, the parent's value is used):

`color`, `font-size`, `font-family`, `font-weight`, `line-height`, `text-align`, `visible`
