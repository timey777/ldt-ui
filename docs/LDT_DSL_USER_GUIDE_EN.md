# LDT DSL User Guide

> LDT (Layout Definition Template) is a declarative UI description language for defining interface structure, styling, and layout.

> 📖 [中文版](LDT_DSL_USER_GUIDE.md) | English

---

## 1. Overview

An `.ldt` file consists of three sections from top to bottom:

- **@style block**: Defines visual styles for UI elements (color, font, border, spacing, etc.)
- **@layout block**: Defines layout behavior for UI elements (direction, alignment, flex, etc.)
- **Node tree**: Defines the widget hierarchy (nesting relationships)

The relationship: the node tree describes **what widgets exist and who contains whom**, @style describes **what widgets look like**, and @layout describes **how widgets are arranged**.

---

## 2. File Structure

A complete `.ldt` file structure:

```ldt
@style {
    /* Style rules */
}

@layout {
    /* Layout rules */
}

/* Node tree */
```

`@style` and `@layout` must appear before the node tree. If no styles or layouts are needed, the corresponding blocks can be omitted.

---

## 3. Selectors

Selectors specify which nodes a rule in `@style` or `@layout` applies to. Four selector types are supported:

### 3.1 Type Selector

Matches by widget type name:

```ldt
panel { ... }    /* Matches all panel widgets */
text { ... }     /* Matches all text widgets */
button { ... }   /* Matches all button widgets */
```

### 3.2 Class Selector

Begins with `.`, matches the node's `class` attribute:

```ldt
.header { ... }       /* Matches nodes whose class includes "header" */
.active { ... }       /* Matches nodes whose class includes "active" */
```

A node can have multiple classes (space-separated), each independently matchable.

### 3.3 ID Selector

Begins with `#`, matches the node's `id` attribute:

```ldt
#root { ... }         /* Matches the node with id "root" */
#submit-btn { ... }   /* Matches the node with id "submit-btn" */
```

### 3.4 Pseudo-State Selector

Begins with `:`, matches nodes in specific interaction states:

| Pseudo-state | Trigger Condition |
|--------------|-------------------|
| `:hover` | Mouse hover |
| `:active` | Pressed / active state |
| `:focus` | Input focus |
| `:disabled` | Disabled state |

```ldt
button:hover { background-color: #0056b3; }
input:focus { border-color: #007aff; }
```

### 3.5 Combining Selectors

Selectors can combine multiple conditions:

```ldt
panel.login-panel { ... }      /* Type + Class */
button.primary:hover { ... }   /* Type + Class + Pseudo-state */
```

---

## 4. Node Definition

### 4.1 Basic Syntax

```
widgetType                         // Simplest form
widgetType:id                      // With identifier
widgetType(attr=value, ...)        // With attributes
widgetType:id(attr=value, ...)     // Full form
```

**Identifier (id)** is separated from type by `:`, e.g. `button:submitBtn`. IDs should be unique within a file, used for looking up widgets in code.

### 4.2 Attribute Settings

Attributes are given in parentheses as `key=value`, comma-separated:

```ldt
text(value="Hello World", class="title")
input(placeholder="Enter text", type="password")
panel(width=300, height=200, class="card")
```

**Supported attribute value types:**

| Type | Syntax | Example |
|------|--------|---------|
| Integer | Plain number | `width=300` |
| Float | With decimal point | `opacity=0.5` |
| Unit value | Number + unit suffix | `width=100px`, `width=50%` |
| String | Double-quoted | `value="Hello"` |
| Boolean | `true` or `false` | `visible=false` |
| Array | Square brackets | `items=[1,2,3]` |

Strings support escape characters: `\n` (newline), `\r` (carriage return), `\t` (tab), `\"` (double quote), `\\` (backslash).

**Special attribute `class`**: The class attribute supports space-separated multiple class names:
```ldt
panel(class="card active highlight")
```

### 4.3 Child Nodes

Use curly braces `{}` to contain child nodes, separated by commas:

```ldt
panel(class="container") {
    text(value="Title", class="title"),
    panel(class="body") {
        input(placeholder="Enter content"),
        button() { text(value="Submit") }
    }
}
```

---

## 5. Unit System

LDT DSL supports five unit types:

| Unit | Description | Use Case |
|------|-------------|----------|
| `px` | Pixels | Precise size control |
| `dp` | Density-independent pixels | Consistent sizing across screen densities |
| `sp` | Scale-independent pixels | Font sizes (scales with system settings) |
| `%` | Percentage | Relative to parent container size |
| `auto` | Automatic | Auto-calculated based on content or parent |

Numeric values without a unit default to `px` (pixels). For example, `width=100` is equivalent to `width=100px`.

**Color values** use hexadecimal format: `#RRGGBB` (e.g. `#ff0000` for red) or `#RGB` (e.g. `#f00` also red). Alpha channel is supported as `#RRGGBBAA`.

---

## 6. Style Properties

Style properties are used in the `@style` block and can also be inlined in node attribute parentheses.

### 6.1 Colors

| Property | Description | Example |
|----------|-------------|---------|
| `background-color` | Background color | `background-color: #f0f0f0` |
| `color` | Text color | `color: #333333` |
| `border-color` | Border color | `border-color: #cccccc` |

### 6.2 Borders

| Property | Description | Example |
|----------|-------------|---------|
| `border` | Border shorthand (width + color) | `border: 1px #eeeeee` |
| `border-width` | Uniform border width | `border-width: 2px` |
| `border-width-top` | Top border width | `border-width-top: 1px` |
| `border-width-right` | Right border width | `border-width-right: 2px` |
| `border-width-bottom` | Bottom border width | `border-width-bottom: 1px` |
| `border-width-left` | Left border width | `border-width-left: 2px` |
| `border-radius` | Border radius | `border-radius: 8px` |

### 6.3 Typography

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `font-size` | Font size | Value + unit | `font-size: 16px` |
| `font-family` | Font name | String | `font-family: "Arial"` |
| `font-weight` | Font weight | `normal`, `bold` | `font-weight: bold` |
| `line-height` | Line height | Value + unit | `line-height: 24px` |
| `text-align` | Text alignment | `left`, `center`, `right`, `justify` | `text-align: center` |

The following properties support **inheritance**: when a child node does not set them, it automatically uses the parent's value. These include `color`, `font-size`, `font-family`, `font-weight`, `line-height`, and `text-align`.

### 6.4 Box Model (Size & Spacing)

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `width` | Width | Value + unit / `auto` | `width: 300px` |
| `height` | Height | Value + unit / `auto` | `height: 100%` |
| `padding` | Uniform padding | Value | `padding: 16px` |
| `padding-top` | Top padding | Value | `padding-top: 8px` |
| `padding-right` | Right padding | Value | `padding-right: 12px` |
| `padding-bottom` | Bottom padding | Value | `padding-bottom: 8px` |
| `padding-left` | Left padding | Value | `padding-left: 12px` |
| `margin` | Uniform margin | Value / `auto` | `margin: 10px` |
| `margin-top` | Top margin | Value / `auto` | `margin-top: auto` |
| `margin-right` | Right margin | Value / `auto` | `margin-right: 20px` |
| `margin-bottom` | Bottom margin | Value / `auto` | `margin-bottom: auto` |
| `margin-left` | Left margin | Value / `auto` | `margin-left: 20px` |
| `box-sizing` | Box sizing mode | `content-box`, `border-box` | `box-sizing: border-box` |
| `overflow` | Overflow handling | `visible`, `hidden`, `scroll`, `auto` | `overflow: scroll` |

`margin` supports `auto` for horizontal centering (set both left and right to `auto`).

`box-sizing: border-box` means `width`/`height` include padding and border; `content-box` (default) means content area only.

`overflow: scroll` or `auto` shows scrollbars when content exceeds the container.

### 6.5 Visual Effects

| Property | Description | Example |
|----------|-------------|---------|
| `opacity` | Opacity (0.0 = fully transparent, 1.0 = opaque) | `opacity: 0.8` |
| `visible` | Visibility | `visible: false` |
| `box-shadow` | Shadow effect | `box-shadow: "0 2px 8px #00000040"` |
| `background-image` | Background image path | `background-image: "images/bg.png"` |

### 6.6 Overlay & Anchoring

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `overlay` | Render in overlay layer (out of normal flow) | `true`, `false` | `overlay: true` |
| `anchor` | Overlay anchor position | `none`, `top-left`, `center`, `top-right`, `bottom-left`, `bottom-right` | `anchor: center` |

`overlay: true` takes the node out of normal layout flow, displaying it above other content. Combine with `anchor` to position it at a specific location within the parent.

---

## 7. Layout Properties

Layout properties are used in the `@layout` block and can also be inlined in node attribute parentheses.

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `display` | Layout mode | `block`, `inline`, `flex`, `grid`, `none` | `display: flex` |
| `position` | Positioning | `static`, `relative`, `absolute`, `fixed` | `position: absolute` |

### 7.1 Flex Layout Properties

When `display` is set to `flex`, the following properties control flex layout behavior:

| Property | Description | Accepted Values | Default |
|----------|-------------|-----------------|---------|
| `flex-direction` | Main axis direction | `row`, `column`, `row-reverse`, `column-reverse` | `row` |
| `align-items` | Cross-axis alignment | `stretch`, `flex-start`, `flex-end`, `center`, `baseline` | `stretch` |
| `justify-content` | Main-axis alignment | `flex-start`, `flex-end`, `center`, `space-between`, `space-around`, `space-evenly` | `flex-start` |
| `flex-wrap` | Wrapping | `nowrap`, `wrap`, `wrap-reverse` | `nowrap` |
| `flex-grow` | Flex grow ratio | Number | `0` |
| `flex-shrink` | Flex shrink ratio | Number | `1` |
| `gap` | Child spacing | Value + unit | `0` |

### 7.2 Grid Layout Properties

When `display` is set to `grid`:

| Property | Description | Example |
|----------|-------------|---------|
| `grid-template-columns` | Column template | `grid-template-columns: "1fr 2fr 1fr"` |
| `grid-template-rows` | Row template | `grid-template-rows: "auto 1fr auto"` |

---

## 8. Layout Model

### 8.1 Block Layout (Default)

`display: block` is the default layout mode. Characteristics:

- Child elements **stack vertically**, one after another
- Child width defaults to **fill the parent container's** content width
- Height is determined by child content

Suitable for page structure, card lists, and other top-to-bottom arrangements.

### 8.2 Flex Layout

`display: flex` enables flex layout. **Note**: LDT's Flex implementation is a curated subset of features, including the most commonly used Flex capabilities but not the full CSS Flexbox standard.

#### Supported Features

**Direction Control:**
- Four main axis directions: `row` (left to right), `column` (top to bottom), `row-reverse`, `column-reverse`
- Reverse layouts render correctly

**Wrapping:**
- `nowrap` (default): Single line, children may overflow
- `wrap`: Auto-wrap to next line/column when exceeding container size

**Main-Axis Alignment (justify-content) — all 6 modes:**
- `flex-start` — Align to start
- `flex-end` — Align to end
- `center` — Center alignment
- `space-between` — Space between, evenly distributed
- `space-around` — Equal spacing around each child
- `space-evenly` — All spacing (including ends) completely equal

**Cross-Axis Alignment (align-items) — 4 modes:**
- `stretch` (default) — Auto-stretch to fill row height/width, only for children without fixed size
- `flex-start` — Align to start
- `flex-end` — Align to end
- `center` — Center alignment

**Flex Grow & Shrink:**
- `flex-grow`: Flex grow ratio. When container has extra space, distributes proportionally. Default 0 (no grow)
- `flex-shrink`: Flex shrink ratio. When container lacks space, shrinks weighted by `current size × shrink`. Default 1 (allow shrink)
- Shrink protection: In column direction, children without fixed height won't shrink below content minimum height
- After grow/shrink, affected children and descendants are automatically re-measured

**Spacing:**
- `gap`: Uniform spacing between children

#### Unsupported Features

The following Flex features are **not yet implemented**:

| Unsupported Feature | Notes |
|--------------------|-------|
| `align-items: baseline` | Cannot align by text baseline (enum exists but has no effect) |
| `align-self` | Cannot override cross-axis alignment for individual children |
| `flex-basis` | No explicit initial main-axis size; child size determined by `width`/`height` only |
| `flex` shorthand | Cannot write `flex: 1`; use separate `flex-grow` and `flex-shrink` |
| `order` | Cannot change visual order of children; order follows source position |
| `align-content` | Multi-line cross-axis distribution of rows is uncontrollable; simple stacking only |
| `min-width / max-width` | Min/max width constraints not exposed as DSL properties |
| `min-height / max-height` | Min/max height constraints not exposed as DSL properties |

### 8.3 Inline Layout

`display: inline` enables inline layout. Characteristics:

- Child elements **arrange horizontally**, like text flow
- **Auto-wrap** when exceeding container width
- Each child maintains its intrinsic width

Suitable for tag groups, text + icon mixed arrangements, etc.

### 8.4 Box Model

Each widget is a rectangular box with four layers from outside to inside:

```
┌─────────────────────────────┐
│         margin              │  ← Margin (spacing from other widgets)
│  ┌───────────────────────┐  │
│  │       border          │  │  ← Border
│  │  ┌─────────────────┐  │  │
│  │  │    padding      │  │  │  ← Padding (space between border and content)
│  │  │  ┌───────────┐  │  │  │
│  │  │  │  content  │  │  │  │  ← Content area (text, images, etc.)
│  │  │  └───────────┘  │  │  │
│  │  └─────────────────┘  │  │
│  └───────────────────────┘  │
└─────────────────────────────┘
```

- `box-sizing: content-box` (default): `width`/`height` controls only the content area
- `box-sizing: border-box`: `width`/`height` includes padding and border (more intuitive)

---

## 9. Complete Examples

### 9.1 Simple Login Panel

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
        text(class="title", value="Account Login"),
        input:username(class="input-field", placeholder="Enter username"),
        input:password(class="input-field", placeholder="Enter password", type="password"),
        button:loginBtn(class="login-btn") { text(value="Login") }
    }
}
```

### 9.2 Horizontal Product List

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
        text(class="product-name", value="Product Name"),
        text(class="product-price", value="$99")
    },
    panel(class="product-card") {
        image(class="product-image", src="p2.png"),
        text(class="product-name", value="Product Name"),
        text(class="product-price", value="$199")
    },
    panel(class="product-card") {
        image(class="product-image", src="p3.png"),
        text(class="product-name", value="Product Name"),
        text(class="product-price", value="$299")
    }
}
```

### 9.3 Page Layout with Scrollable Content

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
        text(class="navbar-title", value="App Title")
    },
    panel(class="content") {
        /* Scrollable content area */
        text(value="Scrollable content goes here...")
    },
    panel(class="tabbar") {
        panel(class="tab-item active") { text(value="Home") },
        panel(class="tab-item") { text(value="Discover") },
        panel(class="tab-item") { text(value="Messages") },
        panel(class="tab-item") { text(value="Profile") }
    }
}
```

---

## 10. Common Widget Types

| Type | Description | Common Attributes |
|------|-------------|-------------------|
| `panel` | Container / panel | Generic style and layout attributes |
| `text` | Text label | `value` (text content) |
| `button` | Button | Set label via child `text` node |
| `input` | Input field | `placeholder` (placeholder text), `type` (input type) |
