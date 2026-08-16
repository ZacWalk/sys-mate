# System Stuff

A lightweight Windows system information utility built with the Win32 SDK. Uses 10% of the memory task manager uses.

Way back around 2000, the company I worked for still supported Windows 95 and 98 for the desktop application we sold. I typically volunteered to test on those platforms and had written my own task manager like program. Every 5 years or so I get it building and do a few updates. Originally called sys-mate now called more generically sys-stuff. 

![System Stuff screenshot](screenshot.png)

## Features

### Performance
- Real-time charts for CPU, Memory, Disk, Network, GPU, and Handle counts
- 60-sample scrolling history updated every second
- Anti-aliased line rendering with filled area graphs
- Network and handle charts auto-scale against the peak seen; network throughput is
  reported in real units rather than a percentage of an arbitrary ceiling
- GPU load is read from the `GPU Engine` counters and reported as the busiest engine
  type, matching how Task Manager summarises it
- Responsive grid layout (2 columns wide, 1 column narrow)

### Processes
- Process list with name, PID, and working-set memory (Toolhelp32)
- Module list for the selected process showing path and size
- Filter-as-you-type search that keeps the current selection
- Context menu to show in Explorer or kill a process

### Network
- Active TCP and UDP connections over both IPv4 and IPv6 (IP Helper API)
- Protocol, local/remote address, state, owning PID, and owning process name
- Filter-as-you-type search

### Windows
- Enumeration of visible top-level windows with title, class, PID, and handle
- Detail pane showing rect, size, style, and extended style for the selected window
- Filter-as-you-type search
- Close window or open owning process in Explorer

### About
- System summary: OS build, architecture, processor, GPU, RAM, system drive, uptime
- Copy the whole summary to the clipboard for pasting into a bug report

### Throughout
- Click any column header to sort; click again to reverse
- Drag a header edge to resize a column
- Full keyboard access: Tab moves between controls, arrows/Home/End/PgUp/PgDn move
  within a list
- Per-monitor DPI aware; the UI rescales when dragged between monitors

### Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| `F5` / `Ctrl+R` | Refresh the current tab |
| `Ctrl+F` | Focus the filter box |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab |
| `Ctrl+1` … `Ctrl+5` | Jump to a tab |

## Building

Requires Visual Studio 2022 17.14+ (v145 toolset) or Visual Studio 2026, with the
"Desktop development with C++" workload. There are no third-party dependencies.

```powershell
.\dd.ps1 run      # build Release x64 and launch
.\dd.ps1 build    # build only
.\dd.ps1 run -Configuration Debug -Platform Win32
```

Or build directly, or open `sys-stuff.sln` in Visual Studio:

```
msbuild src\sys-stuff.vcxproj /p:Configuration=Release /p:Platform=x64
```

Binaries land in `exe\` as `sys-stuff-64.exe` (`-32` for Win32, with a `d` suffix
for Debug).

## License

MIT - see [LICENSE](LICENSE).

Copyright (c) 2002+ Zac Walker
