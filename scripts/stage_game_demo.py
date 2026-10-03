#!/usr/bin/env python3
"""Copy the built game into the workspace's Godot projects directory."""
import argparse
import shutil
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]


def stage(destination: Path) -> None:
    source = REPO_ROOT / "samples/game-ui"
    if not (source / "dist/game.bundle.js").is_file():
        raise RuntimeError("Build samples/game-ui with npm run build:godot first")
    project = destination / "project.godot"
    if project.exists() and 'config/name="React Native Godot Game"' not in project.read_text():
        raise RuntimeError("Destination contains another Godot project")
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / "project.godot", project)
    for folder in ("game", "assets", "dist", "fixtures"):
        shutil.copytree(source / folder, destination / folder, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("*.import", "*.map", "*.dependencies.json", "*.js", "server-key.pem") if folder != "dist"
                        else shutil.ignore_patterns("*.import", "*.map", "*.dependencies.json"))
    if (source / "DEMO.md").exists():
        shutil.copy2(source / "DEMO.md", destination / "README.md")
    print(f"Staged Godot game: {destination}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=REPO_ROOT.parent / "godotProjects/Phase6BGame")
    args = parser.parse_args()
    stage(args.destination.resolve())


if __name__ == "__main__":
    main()
