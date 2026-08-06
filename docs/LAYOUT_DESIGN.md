# LDT 确定性布局设计

LDT 布局面向可预测的应用界面，不复刻 CSS 的固有最小尺寸和完整 Flexbox 算法。

## 尺寸模型

每个节点包含三类相关尺寸：

1. 期望尺寸：显式 `width`/`height`；值为 `auto` 时使用内容测量尺寸。
2. 最终尺寸：期望尺寸经过 min/max 约束和 flex 空间分配后的结果。
3. 滚动尺寸：内容超过最终尺寸时，内容实际占用的范围。

内容可以增大期望尺寸或滚动尺寸，但不能反向扩大 flex 已经分配的最终尺寸。

## 默认值

| 属性 | 默认值 | 含义 |
|---|---:|---|
| `min-width`, `min-height` | `0` | 不存在隐式内容最小尺寸 |
| `max-width`, `max-height` | 不限制 | 没有最大尺寸约束 |
| `flex-grow` | `0` | 不参与剩余空间分配 |
| `flex-shrink` | `0` | 不参与空间收缩 |

`width` 和 `height` 是期望尺寸，不是最小尺寸。只有显式设置 `flex-shrink` 后，元素才允许小于其期望尺寸。

## 盒模型

LDT 只使用一种尺寸模型：`width`、`height`、`min-*` 和 `max-*` 都表示 border box。Padding 和 border 包含在声明尺寸内，margin 位于声明尺寸之外。

例如，`width: 100px; padding: 10px; border: 2px` 会产生 100px 的 border box 和 76px 的 content box。

## Min/Max 约束

Min/max 是显式硬边界：

```text
基础尺寸 = clamp(期望尺寸, 最小尺寸, 最大尺寸)
```

不存在隐式 `min-content` 或 `min-height:auto` 行为。显式最小尺寸大于显式最大尺寸时，最小尺寸优先。

百分比仅在父节点对应轴尺寸确定时，才相对父节点 content box 解析；否则按 `auto` 处理。

## Flex 空间分配

Grow 和 shrink 相互独立，并且只作用于主轴。

剩余空间为正时，符合条件的元素按 `flex-grow` 值直接分配空间。元素达到最大尺寸后冻结，剩余空间继续分配给其他符合条件的元素。

剩余空间为负时，只有 `flex-shrink > 0` 的元素参与收缩。缺少的空间直接按 shrink 值分配，不使用 `尺寸 × shrink`。元素达到最小尺寸后冻结，剩余收缩量继续分配。

没有可收缩元素时，未解决的空间成为容器 overflow。默认 `flex-shrink: 0` 的元素不会被隐式压缩。

## 滚动容器

需要占用剩余空间的弹性滚动区域必须显式允许收缩：

```ldt
@layout {
    .app { display: flex; flex-direction: column; }
    .content { flex-grow: 1; flex-shrink: 1; }
}
```

内容区域取得剩余的最终高度。超出的后代增加其 `scrollHeight`，不能扩大已经分配的最终高度。

这样可以保持工具栏和状态栏尺寸稳定，并让指定的内容区域负责处理 overflow。
