# Copilot Instructions for SysMate

## Project overview

SysMate is a Windows desktop application that displays system information across
five tabbed views: performance charts, processes, network connections, windows, and an
about/system-summary page. It is a pure Win32 SDK app with no third-party dependencies.

All UI is custom drawn against a dark theme — the tab strip, list views, scrollbars and
buttons are all owner/self-drawn rather than common controls.

## Source layout

The whole app is one translation unit: `src/main.cpp` plus header-only helpers.

| File | Contents |
| --- | --- |
| `src/main.cpp` | Globals, per-tab data collection, window procs, `wWinMain` |
| `src/custom_controls.h` | `CustomTabControl`, `CustomListView` (sorting, scrolling, painting) |
| `src/chart.h` | `Chart` — software anti-aliased renderer blitted via `SetDIBitsToDevice` |
| `src/helpers.h` | `OwnedWnd` thunk, `DoubleBuffer`, `VScroll`, `Format::*`, table queries |
| `src/layout.h` | `Dpi::Scale`, `Layout` constants, `DrawTextExt` |
| `src/dark_theme.h` | `Dark::` colours and cached brushes |
| `src/resource.h`, `src/sys-mate.rc` | Icons, accelerators, version info |

## Build

- **Toolset**: MSVC v145 (Visual Studio 2022 17.14+ / Visual Studio 2026), driven
  over CMake and Ninja by the vendored dd driver (`dd.ps1`, PowerShell 7.4+)
- **Language**: C++17, Unicode, `/W4` clean
- **Platform**: x64 only
- **Build**: `.\dd.ps1 build` for Debug and Release, or `.\dd.ps1 build release`
- **Run**: `.\dd.ps1 launch` builds Release and starts the app. Close a running
  instance first — the linker cannot overwrite an executable that is in use
- **Test**: `.\dd.ps1 test --label sys-mate` builds both configurations and runs
  CTest: a build smoke check and `tests/sort.cpp`, which checks the list sort order
  (`Sort::CompareCells` in `src/helpers.h`) is a strict weak ordering. There is no
  headless UI suite, so a UI change has to be checked by running the app
- **No precompiled headers** — each file includes only what it needs
- **Compiler and linker settings** in `CMakeLists.txt` carry over the Visual Studio
  project's: `/permissive-` and `/sdl` (the SDL security checks, their warnings as
  errors) in both configurations, and whole-program optimisation in Release through
  `INTERPROCEDURAL_OPTIMIZATION_RELEASE` — `/GL` on the compile, `/LTCG` on the
  link. Its `/Gy` and `/Oi` come with CMake's `/O2`; its Release PDB was not carried
  over. A setting in `CMakeLists.txt` is not evidence that it reached the tools;
  read `FLAGS` and `LINK_FLAGS` for `sys-mate` in `build\release\build.ninja` and
  `build\debug\build.ninja`
- Output goes to `exe\`: `sys-mate-64.exe`, and `sys-mate-64d.exe` for Debug.
  Release links the CRT statically (`MSVC_RUNTIME_LIBRARY` in `CMakeLists.txt`), so
  `sys-mate-64.exe` runs without the VC++ redistributable; check its imports with
  `dumpbin /dependents` after touching the build

## Conventions

- New pixel measurements go in `Layout` (`src/layout.h`) and pass through `Dpi::Scale`,
  never hard-coded at a call site. `CustomListView::AddColumn` scales its own widths.
- Colours come from the `Dark` namespace (`src/dark_theme.h`). Long-lived GDI objects are
  cached in function-local statics; anything created per-paint must be deleted.
- List rows carry their backing-model index in item data. Resolve selections with
  `SelectedModelIndex`, never by treating the row index as a model index — columns are
  user-sortable, so the two diverge.
- Keyboard commands are accelerators in `sys-mate.rc` dispatched through `WM_COMMAND`
  with `IDM_*` ids from `resource.h`. A `WM_KEYDOWN` handler for the same key will never
  run, because `TranslateAccelerator` consumes it first.
- Anything cached from the DPI or the current font must also be refreshed in
  `OnDpiChanged` — the app declares per-monitor DPI awareness v2, so Windows will not
  scale it.
- PDH counters may be absent on a given machine. Add them via `AddCounter`, check the
  handle for null, and check `CStatus == PDH_CSTATUS_VALID_DATA` before trusting a value.
