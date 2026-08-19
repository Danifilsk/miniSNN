"""Portable source lists shared by the G0-A POSIX and sanitizer gates."""

from pathlib import Path


def repository_root(visualizer_root: Path) -> Path:
    return visualizer_root.resolve().parents[2]


def portable_include_dirs(visualizer_root: Path) -> list[Path]:
    repository = repository_root(visualizer_root)
    return [
        visualizer_root / "include",
        repository / "core" / "include",
        repository / "core" / "src",
        repository / "worlds" / "kernel" / "include",
        repository / "worlds" / "domain" / "include",
        repository / "worlds" / "brain_bridge" / "include",
        repository / "worlds" / "terrain" / "include",
        repository / "worlds" / "scenarios" / "wf0_fish" / "include",
    ]


def portable_sources(visualizer_root: Path, program_source: Path) -> list[Path]:
    repository = repository_root(visualizer_root)
    worlds = repository / "worlds"
    return [
        program_source,
        visualizer_root / "src" / "g0_visualizer_assets.c",
        visualizer_root / "src" / "g0_visualizer_camera.c",
        visualizer_root / "src" / "g0_app_settings.c",
        visualizer_root / "src" / "g0_event_log.c",
        visualizer_root / "src" / "g0_world_document.c",
        visualizer_root / "src" / "g0_world_config.c",
        visualizer_root / "src" / "g0_visualizer_runtime.c",
        visualizer_root / "src" / "g0_visualizer_ui_layout.c",
        worlds / "scenarios" / "wf0_fish" / "src" / "wf0_fish.c",
        worlds / "scenarios" / "wf0_fish" / "src" / "wf0_fish_observation.c",
        worlds / "brain_bridge" / "src" / "minisnn_worlds_brain_bridge.c",
        worlds / "brain_bridge" / "src" / "minisnn_worlds_trainable_brain.c",
        worlds / "terrain" / "src" / "minisnn_worlds_terrain.c",
        worlds / "domain" / "src" / "minisnn_worlds_domain.c",
        worlds / "kernel" / "src" / "minisnn_worlds_kernel.c",
        worlds / "kernel" / "src" / "minisnn_worlds_kernel_snapshot.c",
        worlds / "kernel" / "src" / "minisnn_worlds_kernel_restore.c",
        worlds / "kernel" / "src" / "minisnn_worlds_kernel_command_log.c",
        *sorted((repository / "core" / "src").glob("*.c")),
    ]


def portable_compile_command(
    compiler: str,
    visualizer_root: Path,
    program_source: Path,
    executable: Path,
    extra_flags: list[str] | None = None,
) -> list[str]:
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wformat=2",
        "-Wstrict-prototypes",
    ]
    command.extend(extra_flags or [])
    command.extend(f"-I{directory}" for directory in portable_include_dirs(visualizer_root))
    command.extend(str(source) for source in portable_sources(visualizer_root, program_source))
    command.extend(["-lm", "-o", str(executable)])
    return command
