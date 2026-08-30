# Fabric host-method compatibility

The React Native 0.84.1 `FabricUIManager` surface is classified below.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| FABRIC-CREATE | `createNode` | partially supported | Creates shadow nodes for the static renderer; `instanceHandle` is not retained. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| FABRIC-CLONE | `cloneNode` | pending | Implemented in source but lacks automated contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-CLONE-CHILDREN | `cloneNodeWithNewChildren` | pending | Implemented in source but lacks automated contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-CLONE-PROPS | `cloneNodeWithNewProps` | pending | Implemented in source but lacks automated contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-CLONE-BOTH | `cloneNodeWithNewChildrenAndProps` | pending | Implemented in source but lacks automated contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-CHILD-SET | `createChildSet` | partially supported | Creates the root child set used by the baseline commit. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| FABRIC-APPEND | `appendChild` | partially supported | Builds hierarchical shadow children for the baseline tree. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| FABRIC-APPEND-SET | `appendChildToSet` | partially supported | Adds the baseline application root to its child set. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| FABRIC-COMPLETE | `completeRoot` | partially supported | Defers a full-tree reconstruction commit to Godot's scene thread. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| FABRIC-MEASURE | `measure` | pending | Warning stub. | none | none |
| FABRIC-MEASURE-WINDOW | `measureInWindow` | pending | Warning stub. | none | none |
| FABRIC-MEASURE-LAYOUT | `measureLayout` | pending | Warning stub. | none | none |
| FABRIC-LAYOUT-ANIMATION | `configureNextLayoutAnimation` | pending | Warning stub. | none | none |
| FABRIC-ACCESSIBILITY-EVENT | `sendAccessibilityEvent` | pending | Warning stub. | none | none |
| FABRIC-FIND-SHADOW | `findShadowNodeByTag_DEPRECATED` | pending | Warning stub. | none | none |
| FABRIC-SET-NATIVE-PROPS | `setNativeProps` | pending | Warning stub. | none | none |
| FABRIC-DISPATCH-COMMAND | `dispatchCommand` | pending | Warning stub. | none | none |
| FABRIC-FIND-POINT | `findNodeAtPoint` | pending | Warning stub. | none | none |
| FABRIC-COMPARE-POSITION | `compareDocumentPosition` | pending | Warning stub. | none | none |
| FABRIC-BOUNDING-RECT | `getBoundingClientRect` | pending | Warning stub. | none | none |
| FABRIC-DEFAULT-PRIORITY | `unstable_DefaultEventPriority` | pending | Constant exists but lacks contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-DISCRETE-PRIORITY | `unstable_DiscreteEventPriority` | pending | Constant exists but lacks contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-CONTINUOUS-PRIORITY | `unstable_ContinuousEventPriority` | pending | Constant exists but lacks contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-IDLE-PRIORITY | `unstable_IdleEventPriority` | pending | Constant exists but lacks contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
| FABRIC-CURRENT-PRIORITY | `unstable_getCurrentEventPriority` | pending | Always returns default priority and lacks contract coverage. | [Fabric manager](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | none |
