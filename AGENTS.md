# Copilot Instructions for System Stuff

## Project overview

System Stuff is a Windows desktop application that displays system information across
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
| `src/resource.h`, `src/sys-stuff.rc` | Icons, accelerators, version info |

## Build

- **Toolset**: MSVC v145 (Visual Studio 2022 17.14+ / Visual Studio 2026)
- **Language**: C++17, Unicode, `/W4` clean
- **Platforms**: Win32, x64
- **Preferred**: `.\dd.ps1 run` (builds Release x64 and launches; stops a running
  instance first, since the linker fails with access denied otherwise)
- **Direct**: `msbuild src\sys-stuff.vcxproj /p:Configuration=Debug /p:Platform=x64`
- **No precompiled headers** — each file includes only what it needs
- Output goes to `exe\`. `OutDir` uses `$(ProjectDir)..\` deliberately: `$(SolutionDir)`
  is undefined when building the `.vcxproj` directly, which silently split output
  between `exe\` and `src\exe\` and left stale binaries behind.

## Conventions

- New pixel measurements go in `Layout` (`src/layout.h`) and pass through `Dpi::Scale`,
  never hard-coded at a call site. `CustomListView::AddColumn` scales its own widths.
- Colours come from the `Dark` namespace (`src/dark_theme.h`). Long-lived GDI objects are
  cached in function-local statics; anything created per-paint must be deleted.
- List rows carry their backing-model index in item data. Resolve selections with
  `SelectedModelIndex`, never by treating the row index as a model index — columns are
  user-sortable, so the two diverge.
- Keyboard commands are accelerators in `sys-stuff.rc` dispatched through `WM_COMMAND`
  with `IDM_*` ids from `resource.h`. A `WM_KEYDOWN` handler for the same key will never
  run, because `TranslateAccelerator` consumes it first.
- Anything cached from the DPI or the current font must also be refreshed in
  `OnDpiChanged` — the app declares per-monitor DPI awareness v2, so Windows will not
  scale it.
- PDH counters may be absent on a given machine. Add them via `AddCounter`, check the
  handle for null, and check `CStatus == PDH_CSTATUS_VALID_DATA` before trusting a value.
