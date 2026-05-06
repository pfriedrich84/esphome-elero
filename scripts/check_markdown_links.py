#!/usr/bin/env python3
"""Dependency-free local Markdown link checker."""
from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
IGNORE_PARTS = {
    ".git",
    ".pytest_cache",
    ".ruff_cache",
    ".venv",
    ".esphome",
    "node_modules",
    "dist",
    "build",
    "tests/build",
}
LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
ANCHOR_STRIP_RE = re.compile(r"[^\w _-]", re.UNICODE)


def iter_markdown() -> list[Path]:
    files: list[Path] = []
    for path in ROOT.rglob("*.md"):
        rel = path.relative_to(ROOT)
        if any(part in IGNORE_PARTS for part in rel.parts):
            continue
        files.append(path)
    return sorted(files)


def is_external(target: str) -> bool:
    return bool(re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target))


def split_target(raw: str) -> tuple[str, str]:
    no_query = raw.split("?", 1)[0]
    if "#" in no_query:
        path, anchor = no_query.split("#", 1)
        return unquote(path), unquote(anchor)
    return unquote(no_query), ""


def slugify(heading: str) -> str:
    heading = re.sub(r"<[^>]+>", "", heading).strip().lower()
    heading = ANCHOR_STRIP_RE.sub("", heading)
    heading = re.sub(r"\s+", "-", heading)
    return heading


def anchors_for(path: Path) -> set[str]:
    anchors: set[str] = set()
    text = path.read_text(encoding="utf-8", errors="ignore")
    for line in text.splitlines():
        m = HEADING_RE.match(line)
        if m:
            anchors.add(slugify(m.group(2)))
    return anchors


def main() -> int:
    errors: list[str] = []
    anchor_cache: dict[Path, set[str]] = {}
    for md in iter_markdown():
        text = md.read_text(encoding="utf-8", errors="ignore")
        for match in LINK_RE.finditer(text):
            raw = match.group(1).strip()
            if not raw or is_external(raw) or raw.startswith("mailto:"):
                continue
            target_path, anchor = split_target(raw)
            target = md if not target_path else (md.parent / target_path).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                errors.append(f"{md.relative_to(ROOT)}: link escapes repo: {raw}")
                continue
            if not target.exists():
                errors.append(f"{md.relative_to(ROOT)}: missing link target: {raw}")
                continue
            if anchor and target.suffix.lower() == ".md":
                anchor_key = anchor.lower()
                anchor_cache.setdefault(target, anchors_for(target))
                if anchor_key not in anchor_cache[target]:
                    errors.append(f"{md.relative_to(ROOT)}: missing anchor '{anchor}' in {target.relative_to(ROOT)}")
    if errors:
        print("Broken Markdown links:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Markdown local links OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
