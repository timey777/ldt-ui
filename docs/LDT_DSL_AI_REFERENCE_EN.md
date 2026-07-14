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
| `overflow` | keyword | `visible` | No | `visible` / `hidden` / `scroll` / `auto` |

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
