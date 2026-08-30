# Style compatibility

This matrix distinguishes parsed keys from behavior covered on the supported path.
Pending entries may have source support but cannot be claimed until automated coverage
defines their accepted values and limitations.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| STYLE-FLEX-DIRECTION | `flexDirection` | pending | Parsed by Yoga adapter; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX-WRAP | `flexWrap` | pending | Parsed by Yoga adapter; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-JUSTIFY | `justifyContent` | pending | Parsed by Yoga adapter; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ALIGN-ITEMS | `alignItems` | pending | Parsed by Yoga adapter; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ALIGN-SELF | `alignSelf` | pending | Parsed by Yoga adapter; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ALIGN-CONTENT | `alignContent` | pending | Parsed by Yoga adapter; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX | `flex` | partially supported | Numeric flex fills the baseline root; broader shorthand semantics are not covered. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | [LAYOUT-SMOKE](test-coverage.md) |
| STYLE-FLEX-GROW | `flexGrow` | pending | Parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX-SHRINK | `flexShrink` | pending | Parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX-BASIS | `flexBasis` | pending | Points, percent, and auto are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-BOX-SIZING | `boxSizing` | pending | Public style key; no Godot behavior or automated coverage. | none | none |
| STYLE-ASPECT | `aspectRatio` | pending | Parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-WIDTH | `width` | pending | Points, percent, and auto are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-HEIGHT | `height` | pending | Points, percent, and auto are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MIN-WIDTH | `minWidth` | pending | Points and percent are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MIN-HEIGHT | `minHeight` | pending | Points and percent are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MAX-WIDTH | `maxWidth` | pending | Points and percent are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MAX-HEIGHT | `maxHeight` | pending | Points and percent are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MARGIN | `margin*` | pending | Physical and axis shorthands parse points and percent; no logical edges or coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-LOGICAL-MARGIN | `marginBlock* / marginInline*` | pending | Public logical margin keys have no Godot behavior or automated coverage. | none | none |
| STYLE-PADDING | `padding*` | partially supported | Numeric padding affects the baseline tree; percent and per-edge variants lack coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | [LAYOUT-SMOKE](test-coverage.md) |
| STYLE-LOGICAL-PADDING | `paddingBlock* / paddingInline*` | pending | Public logical padding keys have no Godot behavior or automated coverage. | none | none |
| STYLE-GAP | `gap` | pending | Numeric value is parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ROW-GAP | `rowGap` | pending | Numeric value is parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-COLUMN-GAP | `columnGap` | pending | Numeric value is parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-DISPLAY | `display` | pending | Flex and none are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-POSITION | `position` | pending | Relative and absolute are parsed; no automated coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-OFFSETS | `top/right/bottom/left` | pending | Numeric physical offsets are parsed; no percent or logical edges. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-INSET | `inset*` | pending | Public inset keys have no Godot behavior or automated coverage. | none | none |
| STYLE-DIRECTION | `direction` | pending | Public direction key has no Godot RTL behavior or automated coverage. | none | none |
| STYLE-BORDER-WIDTH | `borderWidth/border*Width` | pending | Numeric physical widths affect layout and painting; no automated coverage. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-BACKGROUND | `backgroundColor` | partially supported | Android processed numeric colors paint a panel; dynamic and platform colors are absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | [STYLE-UNIT](test-coverage.md), [BASELINE-SMOKE](test-coverage.md) |
| STYLE-BORDER-COLOR | `borderColor` | pending | One uniform numeric color is parsed; no per-edge colors or coverage. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-BORDER-RADIUS | `borderRadius/border*Radius` | pending | Uniform and physical corner radii are parsed; no automated coverage. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-OPACITY | `opacity` | partially supported | Numeric opacity modulates the mounted subtree; animation is absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | [STYLE-UNIT](test-coverage.md), [BASELINE-SMOKE](test-coverage.md) |
| STYLE-OVERFLOW | `overflow` | pending | Hidden and visible map to clipping behavior; no automated coverage. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-FONT-SIZE | `fontSize` | partially supported | Numeric size overrides the fallback font; broader typography is absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| STYLE-COLOR | `color` | partially supported | Android processed numeric text color is supported; dynamic colors are absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| STYLE-Z-INDEX | `zIndex` | pending | Public stacking key has no Godot behavior or automated coverage. | none | none |
| STYLE-TRANSFORM | `transform` | pending | Not implemented. | none | none |
| STYLE-SHADOW | `shadow*` | pending | Not implemented. | none | none |
| STYLE-LOGICAL-EDGES | `start/end logical edges` | pending | Not implemented. | none | none |
| STYLE-BORDER-STYLE | `borderStyle` | pending | Not implemented. | none | none |
