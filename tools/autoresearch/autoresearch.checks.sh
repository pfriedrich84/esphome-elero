#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

bash -n autoresearch.sh
python3 -m py_compile components/elero/__init__.py components/elero/cover/__init__.py components/elero/light/__init__.py
