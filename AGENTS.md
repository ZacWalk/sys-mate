# Copilot Instructions for SysMate

## Project overview

SysMate is a Windows x64 desktop application that displays system information
across five views: performance charts, processes, network connections, windows,
and an about/system-summary page. Its UI is a `libdesktop::ui` application:
`ui::app` supplies lifecycle callbacks, `ui::create_app_frame` creates the
portable/native frame, `platform::run` drives the application loop, and an
element tree contains the drawn menu bar, tabs, controls and monitoring views.
Drawing uses `render::draw_context` and chart pixels are presented with
`render::draw_surface`.

The Windows inspection APIs are isolated in `src/native_monitoring.cpp` behind
the UTF-8 records in `src/native_monitoring.h`. PDH, Toolhelp, IP Helper,
window enumeration, process actions and shell actions belong there; the UI
must not hold an `HWND`, `HDC` or a PDH handle.

## Source layout

| File | Contents |
| --- | --- |
| `src/main.cpp` | `ui::app`, frame host, element tree, menu, tabs, filter, lists, chart and About view |
| `src/native_monitoring.h/.cpp` | Explicit Windows monitoring/action backend |
| `src/ui_model.h` | Headless layout, hit testing, sorting, filtering, row identity and focus model |
| `src/chart.h` | SysMate's neutral anti-aliased chart rasterizer |
| `tests/ui_model.cpp` | Headless layout/hit/filter/sort/selection/focus coverage |
| `tests/sort.cpp` | Strict-weak-order regression coverage for table cells |
| `tests/filter_input.cpp` | UTF-8 caret, selection, input and clipboard coverage |

`ui::list_view` supplies table scrolling, selection, keyboard navigation,
column headers and popup-menu plumbing. `ui::edit` supplies the filter input;
`ui::test` fakes and the offscreen frame modes keep UI behavior testable without
a visible window. SysMate keeps its domain rows and chart algorithm in the
application because they describe monitoring data, not a generic desktop
capability.

## Build

- **Toolset**: MSVC v145 (Visual Studio 2022 17.14+ / Visual Studio 2026),
  driven over CMake and Ninja by the vendored dd driver (`dd.ps1`, PowerShell
  7.4+).
- **Language**: C++20, Unicode, `/W3`, `/permissive-` and `/sdl`.
- **Platform**: Windows x64 only.
- **Dependency**: `libdesktop` is fetched at the published
  `04109ee82a500db38d439fa281bbc3f43124adfe` revision. A sibling `..\libdesktop`
  checkout is used automatically for local development.
- **CRT**: Release is statically linked; Debug uses the debug DLL CRT.
- **Output**: `exe\sys-mate-64.exe`, with `sys-mate-64d.exe` for Debug.

Use:

```powershell
.\dd.ps1 build                 # both configurations
.\dd.ps1 test --label sys-mate # the labelled headless suite in both configurations
```

Do not use the unfiltered GUI smoke mode unless a human explicitly asks for a
visible UI check.

## UI and platform rules

- Keep window creation, timers, focus, capture, menus, keyboard/mouse routing,
  drawing and DPI handling in libdesktop's platform/ui layers and the app's
  `ui::frame_host`; do not add a raw Win32 window procedure or private handle
  facade.
- Keep all PDH/process/network/window inspection in the native backend and
  expose neutral UTF-8 values or integer identities to the UI.
- Every refresh must preserve row identity where the same process/window is
  still present. A sorted row index is never a domain identity.
- Filtering matches every displayed cell. Sorting uses
  `sysmate::ui::compare_cells`, which must remain a strict weak ordering for
  finite numbers, non-finite-looking names and ordinary text.
- Preserve all five tabs, the chart palette/area renderer, filters, sorting,
  scrolling, selection, context actions, keyboard navigation and per-tab filter
  state. Do not weaken tests or delete behavior to make a build pass.
- All pixel measurements belong in `ui_model.h`'s neutral layout or are scaled
  with the current `view_theme::dpi_scale` value. Refresh cached fonts, scroll
  geometry and child layout on DPI changes.
- Use platform clipboard, dialogs, settings and shell services rather than SDK
  clipboard/dialog/menu calls in the UI.

## Verification

Read the test output, not just the exit code. The labelled suite covers the build
smoke check, table sort regression, UI-model layout/filter/selection/focus checks,
native monitoring smoke check, the real `ui::edit` UTF-8 key/clipboard test, and
the offscreen and UI entry modes. The offscreen and UI entry modes never show a
window.
