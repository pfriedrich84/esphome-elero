"""Compile the compatibility boundary against the installed ESPHome headers."""

import shutil
import subprocess
from pathlib import Path

import esphome
import pytest
from esphome.const import __version__


@pytest.mark.parametrize("log_level", [0, 5])
def test_esphome_cpp_compat(tmp_path, log_level):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is required for the ESPHome C++ API compatibility probe")

    # ESPHome normally generates these headers during YAML code generation.
    core = tmp_path / "esphome" / "core"
    core.mkdir(parents=True)
    (core / "defines.h").write_text(f"#pragma once\n#define USE_HOST\n#define ESPHOME_LOG_LEVEL {log_level}\n")
    major, minor, patch = __version__.split(".")[:3]
    (core / "version.h").write_text(
        '#pragma once\n#include "esphome/core/macros.h"\n'
        f"#define ESPHOME_VERSION_CODE VERSION_CODE({major}, {minor}, {patch})\n"
    )
    source = tmp_path / "compat.cpp"
    source.write_text(
        r"""
#include "elero/elero_esphome_compat.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

std::string name_for_web(const esphome::EntityBase &entity) {
  return esphome::elero::compat::entity_name(entity);
}

void log_entity_and_packet(const esphome::EntityBase &entity) {
  const uint8_t packet[64] = {};
  char buffer[esphome::format_hex_pretty_size(sizeof(packet))];
  static_assert(sizeof(buffer) == 192);
  ESP_LOGW("compat", "%s: %s",
           LOG_STR_ARG(esphome::elero::compat::entity_log_name(entity)),
           esphome::format_hex_pretty_to(buffer, packet, sizeof(packet), '.'));
  ESP_LOGV("compat", "%s", esphome::format_hex_pretty_to(buffer, packet, 1, '.'));
}
"""
    )
    repo_root = Path(__file__).resolve().parents[2]
    result = subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Werror=deprecated-declarations",
            "-fsyntax-only",
            "-include",
            str(core / "defines.h"),
            "-I",
            str(tmp_path),
            "-I",
            str(Path(esphome.__file__).resolve().parent.parent),
            "-I",
            str(repo_root / "components"),
            str(source),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
