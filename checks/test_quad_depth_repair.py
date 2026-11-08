#!/usr/bin/env python3
"""The quad-depth helper must be a PipelineCache member, and the upgrade is idempotent."""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import repair_quad_depth_access as repair  # noqa: E402

PATCH = (ROOT / "patches" / "rexglue" / "zzz-afro-quad-depth-offset.patch").read_text(encoding="utf-8")
GOOD = pathlib.Path("/tmp/rexapply")
BROKEN_FN = repair.BROKEN
FIXED_FN = repair.FIXED


def fail(msg: str) -> None:
    print("FAIL:", msg, file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if "bool PipelineCache::AfroQuadDepthShiftEnabled(" not in PATCH:
        fail("patch does not define the helper as a member")
    if "PipelineCache::PipelineGeometryShader geometry_shader" in PATCH:
        fail("patch still names the private enum from outside the class")
    if "static bool AfroQuadDepthShiftEnabled(int32_t depth_bias, PipelineGeometryShader geometry_shader);" not in PATCH:
        fail("patch does not declare the helper inside PipelineCache")
    if 'afro_quad_depth_offset, "half", "GPU",' not in PATCH:
        fail("runtime default is not half")
    if 'afro_quad_depth_offset, "off", "GPU",' in PATCH:
        fail("runtime default is still off")
    picker = (ROOT / "app/src/language_picker.cpp").read_text(encoding="utf-8")
    if "kIdXQdepth" in picker or '"qdepth"' in picker or "QuadDepthIndex" in picker:
        fail("the quad depth selector is still in the launcher")
    if '{"rt", "render_target_path_d3d12"' not in picker.split("const AdvItem kAdv[]")[1].split("};")[0]:
        fail("removing quad depth disturbed the render-path option")
    for label in ("Completo (experimental)", "Mitad (experimental)", "Full (experimental)", "Half (experimental)"):
        if label in picker:
            fail("quad depth label still visible: " + label)
    if not (GOOD / "src/graphics/d3d12/pipeline_cache.cpp").is_file():
        fail("upstream apply result is missing; rerun the patch check")

    with tempfile.TemporaryDirectory() as tmp:
        tree = pathlib.Path(tmp)
        shutil.copytree(GOOD / "include", tree / "include")
        shutil.copytree(GOOD / "src", tree / "src")
        cpp = tree / "src/graphics/d3d12/pipeline_cache.cpp"
        header = tree / "include/rex/graphics/d3d12/pipeline_cache.h"
        text = cpp.read_text(encoding="utf-8").replace(FIXED_FN, BROKEN_FN)
        cpp.write_bytes(text.replace("\n", "\r\n").encode("utf-8"))
        decl = repair.DECL
        header.write_text(header.read_text(encoding="utf-8").replace(decl + "\n", "", 1), encoding="utf-8")
        if repair.repair(tree) != "repaired":
            fail("old private-enum helper was not upgraded")
        def norm(path: pathlib.Path) -> str:
            return path.read_text(encoding="utf-8").replace("\r\n", "\n")
        if norm(cpp) != norm(GOOD / "src/graphics/d3d12/pipeline_cache.cpp"):
            fail("repaired cpp does not match the current patch result")
        if norm(header) != norm(GOOD / "include/rex/graphics/d3d12/pipeline_cache.h"):
            fail("repaired header does not match the current patch result")
        if repair.repair(tree) != "unchanged":
            fail("second repair was not idempotent")
        moved = cpp.read_text(encoding="utf-8").replace("\r\n", "\n")
        ns = moved.find("namespace rex::graphics::d3d12 {\n")
        inside = moved.find("}  // namespace shaders\n\nREXCVAR_DECLARE(std::string, afro_quad_depth_offset);")
        if ns < 0 or "REXCVAR_DECLARE(std::string, afro_quad_depth_offset);\n" not in moved[:ns] or inside >= 0:
            fail("cvar declarations were not kept at file scope")
        moved = moved.replace(
            "}  // namespace shaders\n\n",
            "}  // namespace shaders\n\n"
            "REXCVAR_DECLARE(std::string, afro_quad_depth_offset);\n"
            "REXCVAR_DECLARE(bool, force_convert_quad_lists_to_triangle_lists);\n\n",
            1,
        )
        moved = moved.replace(
            "REXCVAR_DECLARE(std::string, afro_quad_depth_offset);\n"
            "REXCVAR_DECLARE(bool, force_convert_quad_lists_to_triangle_lists);\n\n"
            "namespace rex::graphics::d3d12 {\n",
            "namespace rex::graphics::d3d12 {\n",
            1,
        )
        cpp.write_text(moved, encoding="utf-8")
        cmd = tree / "src/graphics/d3d12/command_processor.cpp"
        cmd.parent.mkdir(parents=True, exist_ok=True)
        cmd.write_text('REXCVAR_DEFINE_STRING(\n    afro_quad_depth_offset, "off", "GPU",\n', encoding="utf-8")
        if repair.repair(tree) != "repaired":
            fail("declarations left inside the d3d12 namespace were not moved")
        if 'afro_quad_depth_offset, "half", "GPU",' not in cmd.read_text(encoding="utf-8"):
            fail("an older off default was not migrated to half")
        if repair.repair(tree) != "unchanged":
            fail("moving the declarations was not idempotent")
        objects = tree / "out"
        objects.mkdir()
        (objects / "keep.obj").write_text("keep", encoding="utf-8")
        repair.repair(tree)
        if (objects / "keep.obj").read_text(encoding="utf-8") != "keep":
            fail("repair deleted build output")

    compiler = shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        print("quad depth repair: text checks passed; no C++ compiler for the access pattern")
        return
    src_bad = r"""
class PipelineCache {
  enum class PipelineGeometryShader : unsigned { kQuadList = 3 };
};
bool AfroQuadDepthShiftEnabled(int depth_bias, PipelineCache::PipelineGeometryShader geometry_shader) {
  return geometry_shader == PipelineCache::PipelineGeometryShader::kQuadList && depth_bias != 0;
}
"""
    src_good = r"""
class PipelineCache {
  enum class PipelineGeometryShader : unsigned { kQuadList = 3 };
  static bool AfroQuadDepthShiftEnabled(int depth_bias, PipelineGeometryShader geometry_shader);
 public:
  static int use(int depth_bias) {
    return AfroQuadDepthShiftEnabled(depth_bias, PipelineGeometryShader::kQuadList) ? 0 : 1;
  }
};
bool PipelineCache::AfroQuadDepthShiftEnabled(int depth_bias, PipelineGeometryShader geometry_shader) {
  return geometry_shader == PipelineGeometryShader::kQuadList && depth_bias != 0;
}
int main() { return PipelineCache::use(1); }
"""
    with tempfile.TemporaryDirectory() as tmp:
        bad = pathlib.Path(tmp) / "bad.cpp"
        good = pathlib.Path(tmp) / "good.cpp"
        bad.write_text(src_bad, encoding="utf-8")
        good.write_text(src_good, encoding="utf-8")
        bad_run = subprocess.run([compiler, "-std=c++17", "-c", str(bad), "-o", str(pathlib.Path(tmp) / "bad.o")],
                                 capture_output=True, text=True)
        if bad_run.returncode == 0:
            fail("the free function was allowed to name the private enum")
        if "private" not in (bad_run.stderr + bad_run.stdout):
            fail("the rejected compile did not report private access")
        good_run = subprocess.run([compiler, "-std=c++17", str(good), "-o", str(pathlib.Path(tmp) / "good")],
                                  capture_output=True, text=True)
        if good_run.returncode != 0:
            fail(good_run.stderr)
        run = subprocess.run([str(pathlib.Path(tmp) / "good")], capture_output=True)
        if run.returncode != 0:
            fail("the member helper did not accept kQuadList")
        macros = r"""
#define REXCVAR_DECLARE(type, name) type& FLAGS_##name##_storage_()
#define REXCVAR_DEFINE_BOOL(name, default_val, category, desc) \
  bool& FLAGS_##name##_storage_() { static bool storage = (default_val); return storage; }
#define REXCVAR_DEFINE_STRING(name, default_val, category, desc) \
  std::string& FLAGS_##name##_storage_() { static std::string storage = (default_val); return storage; }
#define REXCVAR_GET(name) (FLAGS_##name##_storage_())
#include <string>
"""
        define_tu = macros + r"""
REXCVAR_DEFINE_STRING(afro_quad_depth_offset, "off", "GPU", "defined at file scope");
REXCVAR_DEFINE_BOOL(force_convert_quad_lists_to_triangle_lists, false, "GPU", "defined at file scope");
"""
        bad_tu = macros + r"""
namespace rex::graphics::d3d12 {
REXCVAR_DECLARE(std::string, afro_quad_depth_offset);
REXCVAR_DECLARE(bool, force_convert_quad_lists_to_triangle_lists);
int use() {
  return REXCVAR_GET(afro_quad_depth_offset) == "full" || REXCVAR_GET(force_convert_quad_lists_to_triangle_lists);
}
}
"""
        good_tu = macros + r"""
REXCVAR_DECLARE(std::string, afro_quad_depth_offset);
REXCVAR_DECLARE(bool, force_convert_quad_lists_to_triangle_lists);
namespace rex::graphics::d3d12 {
int use() {
  return REXCVAR_GET(afro_quad_depth_offset) == "off" && !REXCVAR_GET(force_convert_quad_lists_to_triangle_lists);
}
}
int main() { return rex::graphics::d3d12::use() ? 0 : 1; }
"""
        for name, text in (("define.cpp", define_tu), ("bad.cpp", bad_tu), ("good.cpp", good_tu)):
            (pathlib.Path(tmp) / name).write_text(text, encoding="utf-8")
        bad_link = subprocess.run(
            [compiler, "-std=c++17", str(pathlib.Path(tmp) / "define.cpp"), str(pathlib.Path(tmp) / "bad.cpp"),
             "-o", str(pathlib.Path(tmp) / "badlink")],
            capture_output=True, text=True)
        if bad_link.returncode == 0:
            fail("declaring the cvars inside rex::graphics::d3d12 linked against file-scope definitions")
        joined = bad_link.stderr + bad_link.stdout
        if "FLAGS_afro_quad_depth_offset_storage_" not in joined or "FLAGS_force_convert_quad_lists_to_triangle_lists_storage_" not in joined:
            fail("the failed link did not report both missing cvar storage functions")
        good_link = subprocess.run(
            [compiler, "-std=c++17", str(pathlib.Path(tmp) / "define.cpp"), str(pathlib.Path(tmp) / "good.cpp"),
             "-o", str(pathlib.Path(tmp) / "goodlink")],
            capture_output=True, text=True)
        if good_link.returncode != 0:
            fail(good_link.stderr)
        if subprocess.run([str(pathlib.Path(tmp) / "goodlink")]).returncode != 0:
            fail("file-scope cvar declarations did not use the file-scope definitions")
    print(f"quad depth repair: idempotent upgrade, {pathlib.Path(compiler).name} access, and cvar link check passed")


if __name__ == "__main__":
    main()
