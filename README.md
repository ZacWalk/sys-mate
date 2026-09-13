# System Stuff

[![Build](https://github.com/ZacWalk/system-stuff/actions/workflows/build.yml/badge.svg)](https://github.com/ZacWalk/system-stuff/actions/workflows/build.yml)

A lightweight Windows system information utility built with the Win32 SDK. Uses
10% of the memory task manager uses.

Way back around 2000, the company I worked for still supported Windows 95 and 98 for the desktop application we sold. I typically volunteered to test on those platforms and had written my own task manager like program. Every 5 years or so I get it building and do a few updates. Originally called sys-mate now called more generically sys-stuff.

![System Stuff screenshot](screenshot.png)

## Features

- **Performance** — real-time charts for CPU, memory, disk, network, GPU and
  handle counts, with a 60-sample scrolling history updated every second.
  Network and handle charts auto-scale against the peak seen, and throughput is
  reported in real units rather than a percentage of an arbitrary ceiling.
- **Processes** — name, PID and working-set memory via Toolhelp32, with the
  module list for the selected process. Show in Explorer or kill from the
  context menu.
- **Network** — active TCP and UDP connections over IPv4 and IPv6 via the IP
  Helper API, with protocol, addresses, state and owning process.
- **Windows** — visible top-level windows with title, class, PID and handle, plus
  a detail pane showing rect, size and styles.
- **About** — OS build, architecture, processor, GPU, RAM, system drive and
  uptime, copyable to the clipboard in one action.

Every list sorts by clicking a column header, filters as you type, and is fully
keyboard accessible. The UI is per-monitor DPI aware and rescales when dragged
between monitors.

<kbd>F5</kbd> refreshes, <kbd>Ctrl</kbd>+<kbd>F</kbd> focuses the filter box, and
<kbd>Ctrl</kbd>+<kbd>1</kbd>–<kbd>5</kbd> jump between tabs.

## Building

Requires Windows x64 and Visual Studio with the Desktop C++ workload. There are
no third-party dependencies. The vendored [dd](https://github.com/ZacWalk/dd)
runtime locates Visual Studio and uses the CMake and Ninja that ship with it.

```powershell
.\dd.ps1 build        # both configurations
.\dd.ps1 test         # build and run the suite
.\dd.ps1 run          # build, then launch
```

Binaries land in `exe\` as `sys-stuff-64.exe`, with a `d` suffix for Debug.

## Documentation

[AGENTS.md](AGENTS.md) — conventions for contributors and coding agents.

## License

MIT — see [LICENSE](LICENSE).

Copyright (c) 2002+ Zac Walker
