# Copilot Instructions for System Stuff

## Project overview

System Stuff is a Windows desktop application that displays system information across four tabbed views: processes, network connections, windows, and COM objects. It is a pure Win32 SDK app.

## Build

- **Toolset**: MSVC v145 (Visual Studio 2022 17.14+ / Visual Studio 2026)
- **Charset**: Unicode
- **Platforms**: Win32, x64
- **Build command**: `msbuild src\sys-stuff.vcxproj /p:Configuration=Debug /p:Platform=x64`
- **No precompiled headers** — each .cpp includes only what it needs
