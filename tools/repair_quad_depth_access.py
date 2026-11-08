#!/usr/bin/env python3
"""Make an already-applied quad-depth patch able to see PipelineGeometryShader.

The enum is private. A free function in pipeline_cache.cpp cannot name it.
The current patch declares PipelineCache::AfroQuadDepthShiftEnabled instead.
A tree patched by the previous version still has the free function and the
old stamp. This rewrite is idempotent and does not delete build output.

Exit 0 when the tree is upstream (nothing to do) or already repaired.
Exit 1 when the broken signature is still present afterwards.
"""
from __future__ import annotations

import pathlib
import sys

BROKEN = (
    "bool AfroQuadDepthShiftEnabled(int32_t depth_bias,\n"
    "                               PipelineCache::PipelineGeometryShader geometry_shader) {\n"
    "  if (geometry_shader != PipelineCache::PipelineGeometryShader::kQuadList || depth_bias == 0) return false;"
)
FIXED = (
    "bool PipelineCache::AfroQuadDepthShiftEnabled(int32_t depth_bias,\n"
    "                               PipelineGeometryShader geometry_shader) {\n"
    "  if (geometry_shader != PipelineGeometryShader::kQuadList || depth_bias == 0) return false;"
)
DECL = (
    "  static bool AfroQuadDepthShiftEnabled(int32_t depth_bias, "
    "PipelineGeometryShader geometry_shader);\n"
)
ANCHOR = "  static bool GetGeometryShaderKey(PipelineGeometryShader geometry_shader_type,\n"
# REXCVAR_* expands in the namespace that contains the macro. The definitions
# are at file scope, so a declare inside rex::graphics::d3d12 names a different
# function and the DLL link fails.
CVAR_DECLS = (
    "REXCVAR_DECLARE(std::string, afro_quad_depth_offset);\n"
    "REXCVAR_DECLARE(bool, force_convert_quad_lists_to_triangle_lists);\n"
)
INSIDE_CVARS = "}  // namespace shaders\n\n" + CVAR_DECLS + "\n"
NAMESPACE = "namespace rex::graphics::d3d12 {\n"
DEFINE_OFF = 'afro_quad_depth_offset, "off", "GPU",'
DEFINE_HALF = 'afro_quad_depth_offset, "half", "GPU",'


def _read(path: pathlib.Path) -> tuple[str, str]:
    raw = path.read_bytes()
    newline = "\r\n" if b"\r\n" in raw else "\n"
    return raw.decode("utf-8").replace("\r\n", "\n"), newline


def _write(path: pathlib.Path, text: str, newline: str) -> None:
    path.write_bytes(text.replace("\n", newline).encode("utf-8"))


def repair(root: pathlib.Path) -> str:
    cpp = root / "src" / "graphics" / "d3d12" / "pipeline_cache.cpp"
    header = root / "include" / "rex" / "graphics" / "d3d12" / "pipeline_cache.h"
    command = root / "src" / "graphics" / "d3d12" / "command_processor.cpp"
    if not cpp.is_file() or not header.is_file():
        return "absent"
    cpp_text, cpp_nl = _read(cpp)
    header_text, header_nl = _read(header)
    changed = False
    if BROKEN in cpp_text:
        cpp_text = cpp_text.replace(BROKEN, FIXED, 1)
        _write(cpp, cpp_text, cpp_nl)
        changed = True
    if "bool PipelineCache::AfroQuadDepthShiftEnabled(" in cpp_text and DECL not in header_text:
        if ANCHOR not in header_text:
            raise SystemExit("quad depth declaration anchor is missing from pipeline_cache.h")
        header_text = header_text.replace(ANCHOR, DECL + "\n" + ANCHOR, 1)
        _write(header, header_text, header_nl)
        changed = True
    if INSIDE_CVARS in cpp_text:
        # Leave an untouched upstream file alone so git apply can add the patch.
        cpp_text = cpp_text.replace(INSIDE_CVARS, "}  // namespace shaders\n\n", 1)
        ns_at = cpp_text.find(NAMESPACE)
        if ns_at < 0:
            raise SystemExit("pipeline_cache.cpp is missing namespace rex::graphics::d3d12")
        if CVAR_DECLS not in cpp_text[:ns_at]:
            cpp_text = cpp_text.replace(NAMESPACE, CVAR_DECLS + "\n" + NAMESPACE, 1)
        changed = True
    if changed:
        _write(cpp, cpp_text, cpp_nl)
    if command.is_file():
        cmd_text, cmd_nl = _read(command)
        if DEFINE_OFF in cmd_text and DEFINE_HALF not in cmd_text:
            _write(command, cmd_text.replace(DEFINE_OFF, DEFINE_HALF, 1), cmd_nl)
            changed = True
    cpp_now, _ = _read(cpp)
    if "PipelineCache::PipelineGeometryShader geometry_shader" in cpp_now:
        raise SystemExit("quad depth helper still names the private enum")
    return "repaired" if changed else "unchanged"


def main() -> None:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    print(repair(root))


if __name__ == "__main__":
    main()
