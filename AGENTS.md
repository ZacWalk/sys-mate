# Copilot Instructions for SysMate

## Project overview

SysMate is a Windows x64 desktop application that displays system information
across five views: performance charts, processes, network connections, windows,
and an about/system-summary page.  The application UI is built on
`libdesktop::core` and `libdesktop::ui`: `pf::window_frame` owns the native
window and message loop, `pf::frame_reactor` owns event routing, and
`pf::draw_context` owns painting.  No application source includes the Windows
SDK for UI work.

The Windows inspection APIs are isolated in `src/native_monitoring.cpp` behind
the UTF-8 records in `src/native_monitoring.h`.  PDH, Toolhelp, IP Helper,
window enumeration, process actions and shell actions belong there; reactors
must not hold an `HWND`, `HDC` or a PDH handle.

## Source layout

| File | Contents |
| --- | --- |
| `src/main.cpp` | `pf` reactors for the frame, tabs, filter, lists, chart view and About view |
| `src/native_monitoring.h/.cpp` | Explicit Windows monitoring/action backend |
| `src/ui_model.h` | Headless layout, hit testing, sorting, filtering, row identity and focus model |
| `src/chart.h` | SysMate's neutral anti-aliased chart rasterizer, displayed through `pf::bitmap` |
| `tests/ui_model.cpp` | Headless layout/hit/filter/sort/selection/focus coverage |
| `tests/sort.cpp` | Strict-weak-order regression coverage for table cells |
| `tests/filter_input.cpp` | UTF-8 caret, selection, input and clipboard coverage |

`pf::ui::list_view` supplies the table's scrolling, selection, keyboard
navigation, column headers and popup-menu plumbing.  `pf::ui::edit_box`,
`caret_blinker` and `custom_scrollbar` supply the filter and About controls.
SysMate keeps its domain rows and chart algorithm in the application because
they describe monitoring data, not a generic desktop capability.

## Build

- **Toolset**: MSVC v145 (Visual Studio 2022 17.14+ / Visual Studio 2026),
  driven over CMake and Ninja by the vendored dd driver (`dd.ps1`,
  PowerShell 7.4+).
- **Language**: C++20, Unicode, `/W3`, `/permissive-` and `/sdl`.
- **Platform**: Windows x64 only.
- **Dependency**: `libdesktop` is fetched at the published
  `31aae322af6e0696a9597726705b1121bd31580b` revision.  A sibling
  `..\libdesktop` checkout is used automatically for local development.
  `platform_add_app(sys-mate UI ...)` supplies the manifest, icon, embedded
  icon data and the `libdesktop::core`/`libdesktop::ui` targets.
- **CRT**: Release is statically linked; Debug uses the debug DLL CRT.
- **Output**: `exe\sys-mate-64.exe`, with `sys-mate-64d.exe` for Debug.

Use:

```powershell
.\dd.ps1 build                 # both configurations
.\dd.ps1 test --label sys-mate # six headless tests in both configurations
```

The labelled test command does not open a window.  Do not use the unfiltered
GUI smoke mode for this project unless a human explicitly asks for a visible
UI check.

## UI and platform rules

- Keep all window creation, timers, focus, capture, menus, keyboard/mouse
  routing, drawing and DPI handling in `pf`; do not add a raw Win32 window
  procedure, accelerator table, common control or private `HWND` facade.
- Keep all PDH/process/network/window inspection in the native backend and
  expose neutral UTF-8 values or integer identities to the UI.
- Every refresh must preserve row identity where the same process/window is
  still present.  A sorted row index is never a domain identity.
- Filtering matches every displayed cell.  Sorting uses
  `sysmate::ui::compare_cells`, which must remain a strict weak ordering for
  finite numbers, non-finite-looking names and ordinary text.
- Preserve all five tabs, the chart palette/area renderer, filters, sorting,
  scrolling, selection, context actions, keyboard navigation and per-tab
  filter state.  Do not weaken tests or delete behavior to make a build pass.
- All pixel measurements belong in `ui_model.h`'s neutral layout or are scaled
  with the reactor's current `get_dpi_scale()` value.  Refresh cached fonts,
  scroll geometry and child layout on `dpi_changed`.
- Use `window_frame::text_to_clipboard`, `message_box`, `set_menu` and
  `show_popup_menu` rather than SDK clipboard/dialog/menu calls in the UI.

## Verification

Read the test count, not just the exit code.  The expected labelled suite is
seven tests (`sys_mate_unit`, `sys_mate_sort`, `sys_mate_ui_model`,
`sys_mate_monitoring`, `sys_mate_filter_input`, `sys_mate_offscreen`, and
`sys_mate_ui`) in each configuration.
For a release binary check its
imports with `dumpbin /dependents` after changing the build or platform
boundary.  Do not run a visible UI smoke test as part of routine agent work.
