"""Compile the real entity bodies against a minimal host framework.

Only #include paths/framework declarations are substituted. No method body is
extracted, rewritten, or reimplemented; both entity headers and whole .cpp files
are regenerated on every CMake configure. Hardware behavior is not simulated here.
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
hub = (root / "components/elero/elero.h").read_text()
constants = "\n".join(line for line in hub.splitlines() if line.startswith("static const "))
interfaces = []
for name in ("EleroBlindBase", "EleroLightBase"):
    start = hub.index(f"class {name} {{")
    end = hub.index("\n};", start) + 3
    interfaces.append(hub[start:end])
(out / "interfaces.h").write_text(
    '#pragma once\n#include "elero/elero_profile_delivery_coordinator.h"\n'
    "#include <string>\nnamespace esphome { namespace elero {\n" + constants + "\n" + "\n".join(interfaces) + "\n}}\n"
)
paths = ["components/elero/cover/EleroCover", "components/elero_group/EleroGroupCover"]
for stem in paths:
    for ext in (".h", ".cpp"):
        path = root / (stem + ext)

        def include(match, parent=path.parent):
            target = match.group(1)
            if target.endswith("elero.h"):
                return '#include "hub.h"'
            if target in ("EleroCover.h", "EleroGroupCover.h"):
                return match.group(0)
            if target.startswith("esphome/"):
                return '#include "framework.h"'
            return f'#include "{(parent / target).resolve()}"'

        text = re.sub(r'#include "([^"]+)"', include, path.read_text())
        (out / path.name).write_text(text)
