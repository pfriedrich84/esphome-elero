"""Verbatim production radio methods; only hardware/queue boundaries are fake."""

import re
import sys
from pathlib import Path

root, out = (Path(p).resolve() for p in sys.argv[1:])
out.mkdir(parents=True, exist_ok=True)
source = (root / "components/elero/elero_cc1101.cpp").read_text()
core = (root / "components/elero/elero.cpp").read_text()


def body(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/|[{}]', text[opening:], re.S):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                end = opening + token.end()
                if signature.startswith("struct "):
                    end += 1
                return text[start:end]
    raise ValueError(f"Unterminated production body: {signature}")


methods = [
    "struct Elero::RxFifoIO",
    "bool Elero::process_rx(",
    "void Elero::advance_tx()",
    "void Elero::tx_abort_()",
    "void Elero::flush_rx()",
    "void Elero::flush_and_rx()",
    "uint8_t Elero::read_status(",
    "void Elero::read_buf(",
    "bool Elero::send_command_internal_(",
]
if "bool Elero::read_status_stable(" in source:
    methods.extend(
        ["bool Elero::read_status_stable(", "uint8_t Elero::read_status_once_(", "bool Elero::enter_idle_()"]
    )
includes = [
    "elero_crypto.h",
    "elero_utils.h",
    "elero_overflow_logic.h",
    "elero_tx_logic.h",
    "elero_radio_state_logic.h",
]
text = '#include "radio_hub.h"\n' + "\n".join(f'#include "elero/{h}"' for h in includes)
text += "\nnamespace esphome { namespace elero {\n"
text += "\n".join(line for line in source.splitlines() if line.startswith("static const ") and "=" in line)
text += "\n" + body(source, "static const char *marcstate_to_string(")
text += "\n" + "\n".join(body(source, name) for name in methods)
text += "\n" + body(core, "void Elero::publish_tx_completion_(") + "\n}}\n"
(out / "radio.cpp").write_text(text)
hub = (root / "components/elero/elero.h").read_text()
(out / "radio_types.h").write_text(
    '#pragma once\n#include "interfaces.h"\nnamespace esphome { namespace elero {\n'
    + body(hub, "enum class TxState")
    + ";\n"
    + body(hub, "enum class RadioMode")
    + ";\n"
    + body(hub, "struct TxCompletion")
    + "\n}}\n"
)
