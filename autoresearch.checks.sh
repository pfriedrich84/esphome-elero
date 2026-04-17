#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

bash -n autoresearch.sh
python3 -m py_compile components/elero/__init__.py components/elero/cover/__init__.py components/elero/light/__init__.py
