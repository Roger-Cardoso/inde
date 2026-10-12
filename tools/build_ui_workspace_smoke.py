"""Build the opt-in GTK smoke against the already-built production UI objects.

Requires a CMake Release build of target inde (Ninja or Makefiles). No second copy of the UI or
private-test access is compiled. Running requires a display, isolated config,
and disposable .inde fixtures. This is deliberately not a headless CTest.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import hashlib
import json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--prepare-only",action="store_true",help="Compile verified focal objects only while the clean Release build is running; no driver or passing-test claim")
    parser.add_argument("--sanitize-vocabulary", action="store_true", help="Focal ASan/UBSan on vocabulary forms, annotation explorer, catalog, work setup and modal host; other dependencies Release")
    parser.add_argument("--sanitize-operations", action="store_true", help="Focal ASan/UBSan on operations UI and driver; other production objects Release")
    parser.add_argument("--sanitize-builder", action="store_true", help="Focal ASan/UBSan on builder UI, planning continuity, navigation and driver; remaining production objects Release")
    parser.add_argument("--sanitize-composition", action="store_true", help="Focal ASan/UBSan on composition page, context menu, modal host and driver; remaining UI Release")
    parser.add_argument("--sanitize-worldbuilding", action="store_true", help="Focal ASan/UBSan on calendar/alias UI, entity detail, time projection, graph and driver; remaining UI Release")
    parser.add_argument("--sanitize-temporal-system", action="store_true", help="Focal ASan/UBSan on temporal library, planning shell and driver; remaining UI Release")
    parser.add_argument("--refresh-unit", action="append", choices=("context_menu.cpp","application_theme.cpp","writing_workspace_linguistic.cpp","overlay_dialog.cpp"), default=[], help="Diagnostic only: compile selected UI unit instead of its old object while final build runs")
    parser.add_argument("--test", choices=("keyboard", "author_vocabulary", "narrative_realization", "narrative_access", "narrative_alternatives", "narrative_builder", "removal_assistant", "temporal_workflow", "deletion", "calendar_graphic", "temporal_context", "temporal_templates", "temporal_system", "worldbuilding", "composition", "editor_fixes", "operations", "production", "editorial_review", "continuity", "organization", "reform", "editor_interaction", "semantic_context", "color", "toolbar", "workspace", "explorer", "shell", "relations", "temporal", "writing", "editing", "scale", "recovery", "semantics", "semantic_increment", "session", "review"), default="workspace")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    output = args.output.resolve() if args.output else build / f"inde_ui_{args.test}_smoke"
    if (build / "build.ninja").exists():
        commands = subprocess.check_output(["ninja", "-C", str(build), "-t", "commands", "inde"], text=True).splitlines()
        link = next(line for line in reversed(commands) if " -o inde " in line)
    else:
        link = (build / "CMakeFiles/inde.dir/link.txt").read_text()
    tokens = shlex.split(link)
    objects = [token for token in tokens if token.endswith(".o") and not token.endswith("/src/main.cpp.o")]
    libraries = tokens[tokens.index("-o") + 2:]
    libraries = libraries[:libraries.index("&&")] if "&&" in libraries else libraries
    assert objects and (args.prepare_only or (build / "inde").exists()), "Build target inde first"
    if args.prepare_only:assert args.sanitize_vocabulary or args.sanitize_builder or args.sanitize_vocabulary or args.sanitize_operations or args.sanitize_composition or args.sanitize_worldbuilding or args.sanitize_temporal_system
    flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "gtkmm-4.0"], text=True))
    extra_sources=[]
    sanitizer_flags=[]
    if args.refresh_unit:
        assert not (args.sanitize_vocabulary or args.sanitize_operations or args.sanitize_composition or args.sanitize_worldbuilding or args.sanitize_temporal_system or args.sanitize_builder)
        objects=[obj for obj in objects if not any(obj.endswith("/src/ui/"+name+".o") for name in args.refresh_unit)]
        extra_sources=[str(root/"src/ui"/name) for name in args.refresh_unit]
    if args.sanitize_vocabulary or args.sanitize_operations or args.sanitize_composition or args.sanitize_worldbuilding or args.sanitize_temporal_system or args.sanitize_builder:
        assert sum((args.sanitize_vocabulary,args.sanitize_operations,args.sanitize_composition,args.sanitize_worldbuilding,args.sanitize_temporal_system,args.sanitize_builder)) == 1
        if args.sanitize_vocabulary:
            assert args.test == "author_vocabulary"
            names=("writing_workspace_semantic_detail.cpp","writing_workspace_semantic_inspector.cpp","planning_workspace_entity_types.cpp","planning_workspace_relation_types.cpp","editorial_workspace.cpp","work_setup.cpp","overlay_dialog.cpp")
        elif args.sanitize_builder:
            assert args.test in ("narrative_realization","narrative_access","narrative_alternatives","narrative_builder")
            names=("narrative_builder_page.cpp","narrative_builder_comparison.cpp","narrative_structure_workspace.cpp","narrative_structure_workspace_ui.cpp","narrative_structure_workspace_detail.cpp","narrative_structure_workspace_library.cpp","narrative_library_interaction.cpp","work_setup.cpp","planning_workspace.cpp","editorial_workspace.cpp","editorial_structure_workspace.cpp","context_menu.cpp","workspace_continuity.cpp","main_window.cpp","overlay_dialog.cpp")
            if args.test=="narrative_realization":
                names+=("narrative_realization_ui.cpp","writing_workspace_narrative.cpp","composition_page.cpp","writing_workspace_clipboard.cpp","main_window_context.cpp")
        elif args.sanitize_temporal_system:
            assert args.test in ("temporal_workflow","calendar_graphic","temporal_system","temporal_templates","temporal_context")
            names=("temporal_library_page.cpp","planning_workspace.cpp","planning_workspace_worldbuilding.cpp","workspace_continuity.cpp")
            if args.test=="temporal_context":
                names=("calendar_picker.cpp","temporal_library_page.cpp","writing_workspace_semantic_time.cpp","writing_workspace_semantic_detail.cpp","writing_workspace_semantic_fields.cpp","writing_workspace_semantics.cpp")
            if args.test=="calendar_graphic":
                names=("calendar_picker.cpp","temporal_library_page.cpp","planning_workspace_worldbuilding.cpp")
            if args.test=="temporal_workflow":
                names=("calendar_picker.cpp","temporal_reference_editor.cpp","temporal_context.cpp","writing_workspace_semantic_time.cpp","writing_workspace_interaction.cpp","planning_workspace_worldbuilding.cpp","overlay_dialog.cpp")
        elif args.sanitize_operations:
            assert args.test in ("operations","deletion","removal_assistant")
            names=("project_operations.cpp","main_window_operations.cpp","overlay_dialog.cpp")
            if args.test=="deletion":
                names=("project_operations.cpp","overlay_dialog.cpp","temporal_library_page.cpp","cartography_workspace.cpp","cartography_workspace_ui.cpp")
            if args.test=="removal_assistant":
                names=("project_operations.cpp","main_window_operations.cpp","overlay_dialog.cpp","temporal_library_page.cpp","planning_workspace_time_projection.cpp","planning_workspace_worldbuilding.cpp","workspace_continuity.cpp")
        elif args.sanitize_composition:
            assert args.test in ("composition","editor_fixes","color")
            names=("composition_page.cpp","context_menu.cpp","overlay_dialog.cpp")
            if args.test=="color":
                names=("editor_color_picker.cpp","writing_workspace_controls.cpp","overlay_dialog.cpp","context_menu.cpp")
        else:
            assert args.test=="worldbuilding"
            names=("planning_workspace_worldbuilding.cpp","planning_workspace_entity_detail.cpp","planning_workspace_time_projection.cpp","graphs_workspace.cpp")
        objects=[obj for obj in objects if not any(obj.endswith("/src/ui/"+name+".o") for name in names)]
        extra_sources=[str(root/"src/ui"/name) for name in names]
        sanitizer_flags=["-O1","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
        if args.test=="removal_assistant":
            sanitizer_flags=["-O0","-g1","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
        if args.sanitize_vocabulary or args.sanitize_temporal_system or args.sanitize_builder or args.test=="color":
            # Keep focal GTK instrumentation affordable on small-memory hosts.
            # Production/dependency objects stay Release, not a performance gate.
            sanitizer_flags=["-O0","-g1","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
    compile_flags = ["-std=c++20", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                     "-I" + str(root / "include"), *flags, *sanitizer_flags]
    if sanitizer_flags:
        # Persistent focal objects make a driver-only repair inexpensive, while
        # any project header/flag change invalidates every instrumented unit.
        cache = build / "focal-ui" / args.test
        cache.mkdir(parents=True, exist_ok=True)
        header_hash = hashlib.sha256()
        for header in sorted([*root.joinpath("include").rglob("*.hpp"),
                              *root.joinpath("src").rglob("*.hpp")]):
            header_hash.update(str(header.relative_to(root)).encode())
            header_hash.update(header.read_bytes())
        common = json.dumps(compile_flags).encode() + header_hash.digest()
        focal_objects = []
        for filename in extra_sources:
            unit = Path(filename)
            obj = cache / (unit.name + ".o")
            stamp = cache / (unit.name + ".json")
            signature = hashlib.sha256(common + unit.read_bytes()).hexdigest()
            if not obj.exists() or not stamp.exists() or json.loads(stamp.read_text())["signature"] != signature:
                print("Compiling focal unit", unit.name, flush=True)
                subprocess.run(["c++", *compile_flags, "-c", str(unit), "-o", str(obj)], cwd=build, check=True)
                stamp.write_text(json.dumps({"signature": signature}) + "\n")
            else:
                print("Reusing verified focal unit", unit.name, flush=True)
            focal_objects.append(str(obj))
        objects += focal_objects
        extra_sources = []
    if args.prepare_only:
        print("PREPARED focal objects only; driver link and execution still pending",flush=True)
        return
    command = ["c++", *compile_flags, str(root / f"tests/ui_{args.test}_smoke.cpp"),
               *extra_sources, *objects, *libraries, "-o", str(output)]
    print("Building GTK smoke from", len(objects), "production objects", flush=True)
    subprocess.run(command, cwd=build, check=True)
    print(output)


if __name__ == "__main__":
    main()
