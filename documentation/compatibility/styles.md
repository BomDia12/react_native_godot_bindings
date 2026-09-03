# Style compatibility

This matrix covers cross-platform style keys from React Native 0.87.1 and distinguishes
implemented subsets from behavior covered by automated tests. Partially supported rows may
rely on source evidence when test evidence is `none`. Style keys that React Native scopes
to a single platform are out of scope and are not listed: `borderCurve`, `direction`,
`elevation`, `fontVariant`, `includeFontPadding`, `shadowColor`, `shadowOffset`,
`shadowOpacity`, `shadowRadius`, `textAlignVertical`, `verticalAlign`, and
`writingDirection`.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| STYLE-FLEX-DIRECTION | `flexDirection` | partially supported | Maps the four public directions to Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX-WRAP | `flexWrap` | partially supported | Maps the public wrap values to Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-JUSTIFY | `justifyContent` | partially supported | Maps public justify values to Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ALIGN-ITEMS | `alignItems` | partially supported | Maps public alignment values to Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ALIGN-SELF | `alignSelf` | partially supported | Maps public alignment values to Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ALIGN-CONTENT | `alignContent` | partially supported | Maps start, end, center, and stretch to Godot's Yoga layout; the three space-distribution values and automated coverage are absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX | `flex` | partially supported | Numeric flex fills the baseline root; broader shorthand semantics are not covered. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | [LAYOUT-SMOKE](test-coverage.md) |
| STYLE-FLEX-GROW | `flexGrow` | partially supported | Numeric values reach Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX-SHRINK | `flexShrink` | partially supported | Numeric values reach Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-FLEX-BASIS | `flexBasis` | partially supported | Points, percent, and auto values reach Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-BOX-SIZING | `boxSizing` | pending | Public style key; no Godot behavior or automated coverage. | none | none |
| STYLE-ASPECT | `aspectRatio` | partially supported | Numeric values reach Godot's Yoga layout; string forms are not handled and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-WIDTH | `width` | partially supported | Points, percent, and auto values reach Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-HEIGHT | `height` | partially supported | Points, percent, and auto values reach Godot's Yoga layout; no automated behavior coverage. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MIN-WIDTH | `minWidth` | partially supported | Points and percent values reach Godot's Yoga layout; auto is not handled and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MIN-HEIGHT | `minHeight` | partially supported | Points and percent values reach Godot's Yoga layout; auto is not handled and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MAX-WIDTH | `maxWidth` | partially supported | Points and percent values reach Godot's Yoga layout; auto is not handled and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MAX-HEIGHT | `maxHeight` | partially supported | Points and percent values reach Godot's Yoga layout; auto is not handled and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-MARGIN | `margin*` | partially supported | Physical and axis shorthands accept points and percent values in Godot's Yoga layout; logical edges are absent and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-LOGICAL-MARGIN | `marginBlock* / marginInline* / marginStart / marginEnd` | pending | Public logical margin keys have no Godot behavior or automated coverage. | none | none |
| STYLE-PADDING | `padding*` | partially supported | Physical and axis shorthands accept points and percent values in Godot's Yoga layout; the smoke test covers uniform numeric padding and logical edges are absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | [LAYOUT-SMOKE](test-coverage.md) |
| STYLE-LOGICAL-PADDING | `paddingBlock* / paddingInline* / paddingStart / paddingEnd` | pending | Public logical padding keys have no Godot behavior or automated coverage. | none | none |
| STYLE-GAP | `gap` | partially supported | Numeric values reach Godot's Yoga layout; string forms and automated behavior coverage are absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-ROW-GAP | `rowGap` | partially supported | Numeric values reach Godot's Yoga layout; string forms and automated behavior coverage are absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-COLUMN-GAP | `columnGap` | partially supported | Numeric values reach Godot's Yoga layout; string forms and automated behavior coverage are absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-DISPLAY | `display` | partially supported | Maps `none` and other values to Godot's Yoga display modes; `contents` is not distinct and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-BACKFACE | `backfaceVisibility` | pending | Public view style key has no Godot behavior or automated coverage. | none | none |
| STYLE-POSITION | `position` | partially supported | Maps `absolute` and other values to Godot's Yoga position modes; `static` is not distinct and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-OFFSETS | `top/right/bottom/left` | partially supported | Numeric physical offsets reach Godot's Yoga layout; percent and logical edges are absent and automated coverage is absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp) | none |
| STYLE-INSET | `inset*` | pending | Public inset keys have no Godot behavior or automated coverage. | none | none |
| STYLE-BORDER-WIDTH | `borderWidth/border*Width` | partially supported | Numeric physical widths affect Godot Yoga layout and Panel painting; logical edges and automated coverage are absent. | [layout](../../modules/react_native_bindings/fabric/rn_layout.cpp), [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-BACKGROUND | `backgroundColor` | partially supported | Android processed numeric colors paint a panel; dynamic and platform colors are absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | [STYLE-UNIT](test-coverage.md), [BASELINE-SMOKE](test-coverage.md) |
| STYLE-BORDER-COLOR | `borderColor` | partially supported | One uniform numeric ARGB color reaches Godot Panel painting; per-edge colors and automated coverage are absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-BORDER-COLOR-EDGES | `borderTop/Bottom/Left/Right/Start/End/Block*Color` | pending | Public per-edge and logical border colors have no Godot behavior or automated coverage. | none | none |
| STYLE-BORDER-RADIUS | `borderRadius/border*Radius` | partially supported | Uniform and physical numeric corner radii reach Godot Panel painting; logical and string forms and automated coverage are absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-OPACITY | `opacity` | partially supported | Numeric opacity modulates the mounted subtree; animation is absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | [STYLE-UNIT](test-coverage.md), [BASELINE-SMOKE](test-coverage.md) |
| STYLE-OVERFLOW | `overflow` | partially supported | `hidden` enables Godot clipping; `visible` and `scroll` are not distinct and automated coverage is absent. | [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | none |
| STYLE-BORDER-STYLE | `borderStyle` | pending | Public border style key has no Godot behavior or automated coverage. | none | none |
| STYLE-POINTER-EVENTS | `pointerEvents` | partially supported | `auto`, `none`, `box-only`, and `box-none` control Godot hit testing; other interaction semantics are not covered. | [input router](../../modules/react_native_bindings/input/rn_input_router.cpp) | [INTERACTION-UNIT](test-coverage.md), [PRESSABLE-SMOKE](test-coverage.md) |
| STYLE-CURSOR | `cursor` | pending | Public cursor key has no Godot behavior or automated coverage. | none | none |
| STYLE-OUTLINE | `outlineColor/outlineOffset/outlineStyle/outlineWidth` | pending | Public outline keys have no Godot behavior or automated coverage. | none | none |
| STYLE-BOX-SHADOW | `boxShadow` | pending | Public box-shadow key has no Godot behavior or automated coverage. | none | none |
| STYLE-FILTER | `filter` | pending | Public filter key has no Godot behavior or automated coverage. | none | none |
| STYLE-BLEND | `mixBlendMode` | pending | Public blend-mode key has no Godot behavior or automated coverage. | none | none |
| STYLE-EXPERIMENTAL-BACKGROUND | `experimental_background*` | pending | Public experimental background keys have no Godot behavior or automated coverage. | none | none |
| STYLE-ISOLATION | `isolation` | pending | Public isolation key has no Godot behavior or automated coverage. | none | none |
| STYLE-FONT-SIZE | `fontSize` | partially supported | Numeric size overrides the fallback font; broader typography is absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| STYLE-COLOR | `color` | partially supported | Android processed numeric text color is supported; dynamic colors are absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| STYLE-Z-INDEX | `zIndex` | pending | Public stacking key has no Godot behavior or automated coverage. | none | none |
| STYLE-TRANSFORM | `transform/transformOrigin` | pending | Public transforms and transform origins have no Godot behavior or automated coverage. | none | none |
| STYLE-DEPRECATED-TRANSFORMS | `transformMatrix/rotation/scaleX/scaleY/translateX/translateY` | pending | Deprecated public transform keys have no Godot behavior or automated coverage. | none | none |
| STYLE-LOGICAL-EDGES | `start/end offsets` | pending | Public logical position offsets have no Godot behavior or automated coverage. | none | none |
| STYLE-TEXT-TYPOGRAPHY | `fontFamily/fontStyle/fontWeight` | pending | Public typography keys have no Godot behavior or automated coverage. | none | none |
| STYLE-TEXT-METRICS | `textShadow*/letterSpacing/lineHeight` | pending | Public text metric keys have no Godot behavior or automated coverage. | none | none |
| STYLE-TEXT-ALIGNMENT | `textAlign` | pending | The public text alignment key has no Godot behavior or automated coverage. | none | none |
| STYLE-TEXT-DECORATION | `textDecoration*` | pending | Public text decoration keys have no Godot behavior or automated coverage. | none | none |
| STYLE-TEXT-OTHER | `textTransform/userSelect` | pending | Public text keys have no Godot behavior or automated coverage. | none | none |
| STYLE-IMAGE | `resizeMode/objectFit/tintColor/overlayColor` | pending | Public image style keys have no Godot behavior or automated coverage. | none | none |
