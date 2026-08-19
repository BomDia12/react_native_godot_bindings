#!/usr/bin/env python3
import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
MATRIX_DIR = REPO_ROOT / "documentation/compatibility"
MATRIX_FILES = (
    "components.md",
    "apis.md",
    "fabric-methods.md",
    "styles.md",
    "platforms.md",
    "test-coverage.md",
)
REQUIRED_COLUMNS = (
    "ID",
    "Surface",
    "Status",
    "Behavior / limitations",
    "Implementation evidence",
    "Test evidence",
)
ALLOWED_STATUSES = {
    "supported",
    "partially supported",
    "adapted for Godot",
    "unsupported by design",
    "pending",
}
REQUIRED_SURFACES = {
    "components.md": {
        "ActivityIndicator", "Button", "DrawerLayoutAndroid", "EventEmitter", "FlatList",
        "Image", "ImageBackground", "InputAccessoryView", "KeyboardAvoidingView",
        "experimental_LayoutConformance", "Modal", "unstable_NativeText", "unstable_NativeView",
        "Pressable", "ProgressBarAndroid", "RefreshControl", "SafeAreaView", "ScrollView",
        "SectionList", "StatusBar", "Switch", "Text", "unstable_TextAncestorContext",
        "TextInput", "Touchable", "TouchableHighlight", "TouchableNativeFeedback",
        "TouchableOpacity", "TouchableWithoutFeedback", "View", "VirtualizedList",
        "VirtualizedSectionList", "unstable_VirtualView",
    },
    "apis.md": {
        "AccessibilityInfo", "ActionSheetIOS", "Alert", "Animated", "Appearance", "AppRegistry",
        "AppState", "BackHandler", "Clipboard", "codegenNativeCommands", "codegenNativeComponent",
        "DeviceEventEmitter", "DeviceInfo", "DevMenu", "DevSettings", "Dimensions", "DynamicColorIOS",
        "Easing", "findNodeHandle", "I18nManager", "InteractionManager", "Keyboard", "LayoutAnimation",
        "Linking", "LogBox", "NativeAppEventEmitter", "NativeComponentRegistry",
        "NativeDialogManagerAndroid", "NativeEventEmitter", "NativeModules", "Networking", "PanResponder",
        "PermissionsAndroid", "PixelRatio", "Platform", "PlatformColor", "PushNotificationIOS",
        "processColor", "registerCallableModule", "requireNativeComponent", "ReactNativeVersion",
        "RootTagContext", "Settings", "Share", "StyleSheet", "Systrace", "ToastAndroid",
        "TurboModuleRegistry", "UIManager", "unstable_batchedUpdates", "useAnimatedValue",
        "useAnimatedValueXY", "useAnimatedColor", "useColorScheme", "usePressability",
        "useWindowDimensions", "UTFSequence", "Vibration", "VirtualViewMode", "global timers",
    },
}


def parse_table(path: Path) -> list[dict[str, str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if line.strip().startswith("| ID |"):
            headers = [cell.strip() for cell in line.strip().strip("|").split("|")]
            if tuple(headers) != REQUIRED_COLUMNS:
                raise ValueError(f"{path}: expected columns {REQUIRED_COLUMNS}, got {tuple(headers)}")
            rows = []
            for row_line in lines[index + 2 :]:
                if not row_line.strip().startswith("|"):
                    break
                cells = [cell.strip() for cell in row_line.strip().strip("|").split("|")]
                if len(cells) != len(headers):
                    raise ValueError(f"{path}: malformed row: {row_line}")
                rows.append(dict(zip(headers, cells)))
            return rows
    raise ValueError(f"{path}: compatibility table not found")


def validate_links(path: Path, text: str, failures: list[str]) -> None:
    for target in re.findall(r"\[[^]]+\]\(([^)]+)\)", text):
        if "://" in target:
            continue
        relative_target = target.split("#", 1)[0]
        if relative_target and not (path.parent / relative_target).resolve().exists():
            failures.append(f"{path.relative_to(REPO_ROOT)}: broken evidence link {target}")


def main() -> int:
    failures = []
    all_rows = {}
    seen_ids = set()

    for filename in MATRIX_FILES:
        path = MATRIX_DIR / filename
        if not path.is_file():
            failures.append(f"missing matrix: {path.relative_to(REPO_ROOT)}")
            continue
        validate_links(path, path.read_text(encoding="utf-8"), failures)
        try:
            rows = parse_table(path)
        except ValueError as error:
            failures.append(str(error))
            continue
        all_rows[filename] = rows
        for row in rows:
            row_id = row["ID"]
            if not re.fullmatch(r"[A-Z][A-Z0-9-]+", row_id):
                failures.append(f"{filename}: invalid stable ID {row_id}")
            if row_id in seen_ids:
                failures.append(f"{filename}: duplicate stable ID {row_id}")
            seen_ids.add(row_id)
            if row["Status"] not in ALLOWED_STATUSES:
                failures.append(f"{filename}:{row_id}: invalid status {row['Status']}")
            if row["Status"] != "pending":
                if row["Implementation evidence"] == "none":
                    failures.append(f"{filename}:{row_id}: non-pending claim lacks implementation evidence")
                if row["Test evidence"] == "none":
                    failures.append(f"{filename}:{row_id}: non-pending claim lacks test evidence")

    coverage_ids = {row["ID"] for row in all_rows.get("test-coverage.md", [])}
    for filename, rows in all_rows.items():
        if filename == "test-coverage.md":
            continue
        for row in rows:
            if row["Status"] == "pending":
                continue
            evidence_ids = re.findall(r"\[([A-Z][A-Z0-9-]+)\]", row["Test evidence"])
            if not evidence_ids or not any(evidence_id in coverage_ids for evidence_id in evidence_ids):
                failures.append(f"{filename}:{row['ID']}: test evidence has no coverage row")

    for filename, expected_surfaces in REQUIRED_SURFACES.items():
        actual_surfaces = {row["Surface"].strip("`") for row in all_rows.get(filename, [])}
        missing = sorted(expected_surfaces - actual_surfaces)
        extra = sorted(actual_surfaces - expected_surfaces)
        if missing:
            failures.append(f"{filename}: missing React Native 0.84.1 exports: {', '.join(missing)}")
        if extra:
            failures.append(f"{filename}: unrecognized exports: {', '.join(extra)}")

    if failures:
        print("Compatibility validation failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Compatibility validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
