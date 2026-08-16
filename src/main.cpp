#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <uxtheme.h>
#include <shlwapi.h>

#include <vector>
#include <string>
#include <algorithm>
#include <deque>

#include "resource.h"
#include "chart.h"
#include "layout.h"
#include "dark_theme.h"
#include "helpers.h"
#include "custom_controls.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls'" \
    " version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ============================================================
// Globals
// ============================================================
static HINSTANCE g_hInst;
static HWND g_hMainWnd;
static CustomTabControl g_tabCtrl;
static HWND g_hFilterEdit;
static HFONT g_hFont;
static HFONT g_hFontBold;
static HFONT g_hFontChart;
static HFONT g_hFontTitle;
static HFONT g_hFontSubtitle;
static int g_nCurrentTab;
static std::wstring g_filterText;
static bool g_updatingFilterEdit = false; // suppresses EN_CHANGE while SwitchTab restores text

enum TabId { TAB_PERFORMANCE, TAB_PROCESSES, TAB_NETWORK, TAB_WINDOWS, TAB_ABOUT, TAB_COUNT };

static std::wstring g_tabFilterText[TAB_COUNT];

static const WCHAR* g_tabNames[] = {L"Performance", L"Processes", L"Network", L"Windows", L"About"};
static HWND g_hTabPages[TAB_COUNT];

// ============================================================
// Performance tab
// ============================================================
static HWND g_hPerfPanel;

enum ChartId { CHART_CPU, CHART_MEM, CHART_DISK, CHART_NET, CHART_GPU, CHART_HANDLES, NUM_CHARTS };

static Chart g_charts[NUM_CHARTS];
static const wchar_t* g_chartTitles[NUM_CHARTS] = {L"CPU", L"Memory", L"Disk", L"Network", L"GPU", L"Handles"};
static std::deque<float> g_chartData[NUM_CHARTS];
static constexpr int CHART_HISTORY = 60;
static UINT_PTR g_perfTimer = 0;

static DWORDLONG g_memTotalBytes = 0;
static DWORDLONG g_memAvailBytes = 0;
static float g_rawHandleCount = 0;
static float g_handleMaxSeen = 10000.f;
static double g_netBytesPerSec = 0;
static double g_netMaxSeen = 1024.0 * 1024.0; // 1 MB/s floor keeps an idle link from looking busy
static float g_gpuPercent = 0;
static bool g_gpuAvailable = false;

static std::wstring g_cpuName;
static DWORD g_cpuLogicalCores = 0;
static DWORD g_cpuMhz = 0;
static std::wstring g_gpuName;
static ULONGLONG g_diskTotalBytes = 0;
static ULONGLONG g_diskFreeBytes = 0;
static WCHAR g_systemDriveLetter = L'C';

static void InitHardwareInfo()
{
	// CPU name + speed from registry
	HKEY hKey = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
					  0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		WCHAR name[256] = {};
		DWORD size = sizeof(name);
		if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr,
							 reinterpret_cast<LPBYTE>(name), &size) == ERROR_SUCCESS)
		{
			g_cpuName = name;
			// Trim leading/trailing whitespace
			size_t start = g_cpuName.find_first_not_of(L" \t");
			size_t end = g_cpuName.find_last_not_of(L" \t");
			if (start != std::wstring::npos)
				g_cpuName = g_cpuName.substr(start, end - start + 1);
		}
		DWORD mhz = 0;
		size = sizeof(mhz);
		if (RegQueryValueExW(hKey, L"~MHz", nullptr, nullptr,
							 reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS)
			g_cpuMhz = mhz;
		RegCloseKey(hKey);
	}

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	g_cpuLogicalCores = si.dwNumberOfProcessors;

	// GPU name from primary display device
	DISPLAY_DEVICEW dd = {sizeof(dd)};
	for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); i++)
	{
		if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
		{
			g_gpuName = dd.DeviceString;
			break;
		}
	}
	if (g_gpuName.empty())
	{
		dd.cb = sizeof(dd);
		if (EnumDisplayDevicesW(nullptr, 0, &dd, 0))
			g_gpuName = dd.DeviceString;
	}

	// System drive total/free
	WCHAR sysDir[MAX_PATH] = {};
	if (GetSystemDirectoryW(sysDir, MAX_PATH) > 0 && sysDir[0])
		g_systemDriveLetter = sysDir[0];
	WCHAR root[4] = {g_systemDriveLetter, L':', L'\\', 0};
	ULARGE_INTEGER freeAvail = {}, total = {}, totalFree = {};
	if (GetDiskFreeSpaceExW(root, &freeAvail, &total, &totalFree))
	{
		g_diskTotalBytes = total.QuadPart;
		g_diskFreeBytes = totalFree.QuadPart;
	}
}

// Button bar
enum
{
	BTN_REFRESH_PROC = 2000, BTN_EXPLORER_PROC, BTN_KILL_PROC,
	BTN_REFRESH_NET,
	BTN_REFRESH_WIN, BTN_EXPLORER_WIN, BTN_KILL_WIN,
	BTN_REFRESH_ABOUT, BTN_COPY_ABOUT, BTN_REPORT_ISSUE,
	ID_FILTER_EDIT,
};

static HWND g_btnRefreshProc, g_btnExplorerProc, g_btnKillProc;
static HWND g_btnRefreshNet;
static HWND g_btnRefreshWin, g_btnExplorerWin, g_btnKillWin;
static HWND g_btnRefreshAbout;
static HWND g_btnCopyAbout;
static HWND g_btnReportIssue;
static HWND g_hAboutPanel;

static PDH_HQUERY g_pdhQuery = nullptr;
static PDH_HCOUNTER g_pdhCpu = nullptr;
static PDH_HCOUNTER g_pdhDisk = nullptr;
static PDH_HCOUNTER g_pdhNet = nullptr;
static PDH_HCOUNTER g_pdhGpu = nullptr;
static PDH_HCOUNTER g_pdhHandles = nullptr;

// Adds a counter, leaving the handle null when the counter is unavailable on this machine.
static bool AddCounter(const WCHAR* path, PDH_HCOUNTER* out)
{
	if (PdhAddEnglishCounterW(g_pdhQuery, path, 0, out) != ERROR_SUCCESS)
	{
		*out = nullptr;
		return false;
	}
	return true;
}

static void InitPerfCounters()
{
	InitHardwareInfo();
	PdhOpenQuery(nullptr, 0, &g_pdhQuery);
	if (g_pdhQuery)
	{
		AddCounter(L"\\Processor(_Total)\\% Processor Time", &g_pdhCpu);
		AddCounter(L"\\PhysicalDisk(_Total)\\% Disk Time", &g_pdhDisk);
		AddCounter(L"\\Network Interface(*)\\Bytes Total/sec", &g_pdhNet);
		AddCounter(L"\\Process(_Total)\\Handle Count", &g_pdhHandles);
		// GPU Engine counters exist on Windows 10 1709+ with a WDDM 2.x driver.
		AddCounter(L"\\GPU Engine(*)\\Utilization Percentage", &g_pdhGpu);
		// Prime: rate counters need two collects before yielding a valid delta.
		PdhCollectQueryData(g_pdhQuery);
	}

	static constexpr ChartColor kChartPalette[NUM_CHARTS][2] = {
		{{0, 180, 255}, {0, 120, 200}},   // CPU
		{{180, 0, 255}, {140, 0, 200}},   // Memory
		{{0, 200, 100}, {0, 150, 80}},    // Disk
		{{255, 180, 0}, {200, 140, 0}},   // Network
		{{255, 80, 80}, {200, 50, 50}},   // GPU
		{{100, 220, 220}, {60, 170, 170}} // Handles
	};
	for (int i = 0; i < NUM_CHARTS; i++)
	{
		g_charts[i].SetTitle(g_chartTitles[i]);
		ChartStyle s;
		s.lineColor = kChartPalette[i][0];
		s.fillColor = kChartPalette[i][1];
		g_charts[i].SetStyle(s);
		g_chartData[i].resize(CHART_HISTORY, 0.f);
	}
}

static void PushChartSample(const int chartIdx, const float value)
{
	g_chartData[chartIdx].push_back(std::clamp(value, 0.f, 100.f));
	if (g_chartData[chartIdx].size() > CHART_HISTORY) g_chartData[chartIdx].pop_front();
}

static double ReadCounter(const PDH_HCOUNTER counter)
{
	PDH_FMT_COUNTERVALUE val = {};
	if (counter && PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS
		&& val.CStatus == PDH_CSTATUS_VALID_DATA)
		return val.doubleValue;
	return 0.0;
}

// Reads a wildcard counter's instances into `buf`. Returns the instance count (0 on failure).
static DWORD ReadCounterArray(const PDH_HCOUNTER counter, std::vector<BYTE>& buf)
{
	if (!counter) return 0;
	DWORD size = 0, count = 0;
	if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, nullptr) != PDH_MORE_DATA || size == 0)
		return 0;
	buf.resize(size);
	if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count,
	                                 reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data())) != ERROR_SUCCESS)
		return 0;
	return count;
}

// Task Manager reports GPU load as the busiest engine type, not the sum of all
// engines (which would multiply-count 3D, copy and video queues).
static float SampleGpuPercent(std::vector<BYTE>& buf)
{
	const DWORD count = ReadCounterArray(g_pdhGpu, buf);
	// The counter can exist while no engine instance is reporting (no WDDM 2.x driver).
	g_gpuAvailable = g_pdhGpu != nullptr && count > 0;
	if (count == 0) return 0.f;
	const auto* items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());

	std::vector<std::pair<std::wstring, double>> byEngineType;
	for (DWORD i = 0; i < count; i++)
	{
		if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA) continue;
		const WCHAR* name = items[i].szName ? items[i].szName : L"";
		const WCHAR* tag = wcsstr(name, L"engtype_");
		const std::wstring key = tag ? tag + 8 : L"";
		const auto it = std::find_if(byEngineType.begin(), byEngineType.end(),
		                             [&](const auto& p) { return p.first == key; });
		if (it == byEngineType.end()) byEngineType.emplace_back(key, items[i].FmtValue.doubleValue);
		else it->second += items[i].FmtValue.doubleValue;
	}

	double busiest = 0;
	for (const auto& p : byEngineType) busiest = std::max(busiest, p.second);
	return static_cast<float>(std::clamp(busiest, 0.0, 100.0));
}

static void SamplePerfData()
{
	if (!g_pdhQuery) return;
	PdhCollectQueryData(g_pdhQuery);

	PushChartSample(CHART_CPU, static_cast<float>(ReadCounter(g_pdhCpu)));

	MEMORYSTATUSEX ms = {sizeof(ms)};
	GlobalMemoryStatusEx(&ms);
	g_memTotalBytes = ms.ullTotalPhys;
	g_memAvailBytes = ms.ullAvailPhys;
	PushChartSample(CHART_MEM, static_cast<float>(ms.dwMemoryLoad));

	PushChartSample(CHART_DISK, static_cast<float>(ReadCounter(g_pdhDisk)));

	WCHAR diskRoot[4] = {g_systemDriveLetter, L':', L'\\', 0};
	ULARGE_INTEGER freeAvail = {}, total = {}, totalFree = {};
	if (GetDiskFreeSpaceExW(diskRoot, &freeAvail, &total, &totalFree))
	{
		g_diskTotalBytes = total.QuadPart;
		g_diskFreeBytes = totalFree.QuadPart;
	}

	std::vector<BYTE> buf;

	// Network throughput is unbounded, so the chart auto-scales against the peak seen.
	g_netBytesPerSec = 0;
	if (const DWORD count = ReadCounterArray(g_pdhNet, buf))
	{
		const auto* items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
		for (DWORD i = 0; i < count; i++)
			if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA)
				g_netBytesPerSec += items[i].FmtValue.doubleValue;
	}
	if (g_netBytesPerSec > g_netMaxSeen) g_netMaxSeen = g_netBytesPerSec * 1.2;
	PushChartSample(CHART_NET, static_cast<float>(g_netBytesPerSec / g_netMaxSeen * 100.0));

	g_gpuPercent = SampleGpuPercent(buf);
	PushChartSample(CHART_GPU, g_gpuPercent);

	g_rawHandleCount = static_cast<float>(ReadCounter(g_pdhHandles));
	if (g_rawHandleCount > g_handleMaxSeen) g_handleMaxSeen = g_rawHandleCount * 1.2f;
	PushChartSample(CHART_HANDLES, g_rawHandleCount / g_handleMaxSeen * 100.f);
}

// Big number shown in the top-right of a chart.
static std::wstring ChartValueText(const int id)
{
	const float latest = g_chartData[id].empty() ? 0.f : g_chartData[id].back();
	WCHAR buf[64];
	switch (id)
	{
	case CHART_NET:
		return Format::Rate(g_netBytesPerSec);
	case CHART_HANDLES:
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d", static_cast<int>(g_rawHandleCount));
		return buf;
	case CHART_GPU:
		if (!g_gpuAvailable) return L"n/a";
		[[fallthrough]];
	default:
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.1f%%", latest);
		return buf;
	}
}

// Secondary line under the chart title. Empty means "no detail for this chart".
static std::wstring ChartDetailText(const int id)
{
	WCHAR buf[192];
	switch (id)
	{
	case CHART_CPU:
		if (!g_cpuName.empty())
			_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%s  (%u cores @ %.2f GHz)",
			             g_cpuName.c_str(), g_cpuLogicalCores, g_cpuMhz / 1000.0);
		else
			_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%u logical cores @ %.2f GHz",
			             g_cpuLogicalCores, g_cpuMhz / 1000.0);
		return buf;
	case CHART_MEM:
		return L"Used " + Format::Gb(g_memTotalBytes - g_memAvailBytes)
			+ L" / " + Format::Gb(g_memTotalBytes)
			+ L"  (Available " + Format::Gb(g_memAvailBytes) + L")";
	case CHART_DISK:
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%c:  Used %s / %s  (Free %s)",
		             g_systemDriveLetter,
		             Format::Gb(g_diskTotalBytes - g_diskFreeBytes).c_str(),
		             Format::Gb(g_diskTotalBytes).c_str(),
		             Format::Gb(g_diskFreeBytes).c_str());
		return buf;
	case CHART_NET:
		return L"Scale " + Format::Rate(g_netMaxSeen) + L" (peak seen)";
	case CHART_GPU:
		if (!g_gpuAvailable) return g_gpuName.empty() ? L"GPU counters unavailable" : g_gpuName;
		return g_gpuName.empty() ? L"Busiest engine" : g_gpuName;
	case CHART_HANDLES:
		return L"Total system handles";
	default:
		return L"";
	}
}

static void RenderPerfCharts(const HWND hWnd, const HDC hdc)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	FillRect(hdc, &rc, Dark::BrushWindow());
	const int totalW = rc.right, totalH = rc.bottom;
	if (totalW <= 0 || totalH <= 0) return;

	const int pad = Dpi::Scale(Layout::ChartPadding);
	const int spacing = Dpi::Scale(Layout::ChartSpacing);
	const int cols = (totalW > Dpi::Scale(500)) ? 2 : 1;
	const int rows = (NUM_CHARTS + cols - 1) / cols;
	const int chartW = totalW / cols - spacing;
	const int chartH = totalH / rows - spacing;
	if (chartW < Dpi::Scale(50) || chartH < Dpi::Scale(30)) return;

	const int textMargin = Dpi::Scale(Layout::ChartTextMargin);
	const int textTop = Dpi::Scale(Layout::ChartTextTop);
	const int textBottom = Dpi::Scale(Layout::ChartTextBottom);
	const int valRightMargin = Dpi::Scale(Layout::ChartValueRightMargin);
	const int detailBottom = Dpi::Scale(Layout::ChartDetailBottom);

	SetBkMode(hdc, TRANSPARENT);

	for (int i = 0; i < NUM_CHARTS; i++)
	{
		const int col = i % cols, row = i / cols;
		const int x = pad + col * (chartW + spacing);
		const int y = pad + row * (chartH + spacing);

		g_charts[i].Resize(chartW, chartH);
		g_charts[i].SetMargin(Dpi::Scale(Layout::ChartGridMargin));
		g_charts[i].SetData(g_chartData[i], 0, 100);
		g_charts[i].Render();
		g_charts[i].Paint(hdc, x, y);

		SelectObject(hdc, g_hFontChart);
		SetTextColor(hdc, RGB(200, 200, 200));
		const RECT titleRc = {x + textMargin, y + textTop, x + chartW - spacing, y + textBottom};
		DrawTextExt(hdc, g_chartTitles[i], -1, titleRc, TextAlign::Left);

		const std::wstring value = ChartValueText(i);
		SetTextColor(hdc, RGB(255, 255, 255));
		const RECT valRc = {x + chartW - valRightMargin, y + textTop, x + chartW - textMargin, y + textBottom};
		DrawTextExt(hdc, value.c_str(), -1, valRc, TextAlign::Right);

		const std::wstring detail = ChartDetailText(i);
		if (!detail.empty())
		{
			SelectObject(hdc, g_hFont);
			SetTextColor(hdc, RGB(160, 160, 160));
			const RECT detRc = {x + textMargin, y + textBottom, x + chartW - spacing, y + detailBottom};
			DrawTextExt(hdc, detail.c_str(), -1, detRc, TextAlign::Left | TextAlign::Ellipsis);
		}
	}
}

static LRESULT CALLBACK PerfPanelProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			RenderPerfCharts(hWnd, ps.hdc);
			EndPaint(hWnd, &ps);
			return 0;
		}
	case WM_ERASEBKGND: return 1;
	case WM_TIMER:
		SamplePerfData();
		InvalidateRect(hWnd, nullptr, FALSE);
		return 0;
	case WM_DESTROY:
		if (g_perfTimer) { KillTimer(hWnd, g_perfTimer); g_perfTimer = 0; }
		if (g_pdhQuery) { PdhCloseQuery(g_pdhQuery); g_pdhQuery = nullptr; }
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================
// Process tab
// ============================================================
static CustomListView g_procList;
static CustomListView g_moduleList;

struct ProcessInfo
{
	DWORD pid;
	std::wstring name;
	std::wstring path;
	SIZE_T workingSet;
};

static std::vector<ProcessInfo> g_processes;

static void UpdateProcessButtons();
static void UpdateWindowButtons();
static void RefreshModuleList(DWORD pid);
static void ShowWindowProperties();

// Rows carry their model index in item data, so sorting the list cannot
// desynchronise the view from the backing vector.
static int SelectedModelIndex(const CustomListView& list, const size_t modelSize)
{
	const int sel = list.GetSelected();
	if (sel < 0) return -1;
	const auto idx = static_cast<int>(list.GetItemData(sel));
	return (idx >= 0 && idx < static_cast<int>(modelSize)) ? idx : -1;
}

static bool MatchesFilter(const std::wstring& text)
{
	if (g_filterText.empty()) return true;
	return StrStrIW(text.c_str(), g_filterText.c_str()) != nullptr;
}

static void SetFilterText(const std::wstring& s)
{
	g_filterText = s;
}

// Empty-state text that distinguishes "nothing here" from "nothing matched".
static const WCHAR* EmptyListText()
{
	return g_filterText.empty() ? L"Empty" : L"No matching items";
}

static void RefreshProcessList()
{
	// Remember the selection by PID so filtering and refreshing don't lose the user's place.
	const int prevIdx = SelectedModelIndex(g_procList, g_processes.size());
	const DWORD prevPid = prevIdx >= 0 ? g_processes[prevIdx].pid : 0;

	g_procList.SetRedraw(false);
	g_procList.DeleteAllItems();
	g_procList.SetEmptyText(EmptyListText());
	g_processes.clear();

	const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
	{
		g_procList.SetRedraw(true);
		return;
	}

	PROCESSENTRY32W pe = {sizeof(pe)};
	if (Process32FirstW(hSnap, &pe))
	{
		do
		{
			if (pe.th32ProcessID == 0) continue;
			if (!MatchesFilter(pe.szExeFile)) continue;

			ProcessInfo pi;
			pi.pid = pe.th32ProcessID;
			pi.name = pe.szExeFile;
			pi.workingSet = 0;

			// PROCESS_QUERY_LIMITED_INFORMATION alone is enough for both calls below and
			// succeeds against far more processes than adding PROCESS_VM_READ would.
			const HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
			if (hProc)
			{
				PROCESS_MEMORY_COUNTERS pmc = {};
				if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc)))
					pi.workingSet = pmc.WorkingSetSize;
				WCHAR path[MAX_PATH] = {};
				DWORD pathLen = MAX_PATH;
				if (QueryFullProcessImageNameW(hProc, 0, path, &pathLen))
					pi.path = path;
				CloseHandle(hProc);
			}

			g_processes.push_back(std::move(pi));
		}
		while (Process32NextW(hSnap, &pe));
	}
	CloseHandle(hSnap);

	std::sort(g_processes.begin(), g_processes.end(),
	          [](const ProcessInfo& a, const ProcessInfo& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });

	for (int i = 0; i < static_cast<int>(g_processes.size()); i++)
	{
		auto& pi = g_processes[i];
		const int idx = g_procList.AddItem(pi.name.c_str());
		g_procList.SetItemData(idx, i);
		g_procList.SetItemText(idx, 1, Format::U(pi.pid));

		WCHAR memStr[32];
		_snwprintf_s(memStr, _countof(memStr), _TRUNCATE, L"%.1f MB", pi.workingSet / (1024.0 * 1024.0));
		g_procList.SetItemText(idx, 2, memStr);
	}

	g_procList.SetRedraw(true);

	// Restore the previous selection if that process still exists and still matches.
	int restoredPid = 0;
	if (prevPid != 0)
	{
		for (int i = 0; i < static_cast<int>(g_processes.size()); i++)
			if (g_processes[i].pid == prevPid)
			{
				const int row = g_procList.FindItemByData(i);
				if (row >= 0) { g_procList.SetSelected(row); restoredPid = prevPid; }
				break;
			}
	}

	if (restoredPid != 0)
	{
		RefreshModuleList(restoredPid);
	}
	else
	{
		g_moduleList.DeleteAllItems();
		g_moduleList.Invalidate();
	}
	UpdateProcessButtons();
}

static void RefreshModuleList(const DWORD pid)
{
	g_moduleList.SetRedraw(false);
	g_moduleList.DeleteAllItems();

	const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (hSnap == INVALID_HANDLE_VALUE)
	{
		g_moduleList.SetEmptyText(L"Modules unavailable (access denied)");
		g_moduleList.SetRedraw(true);
		return;
	}
	g_moduleList.SetEmptyText(L"Empty");

	MODULEENTRY32W me = {sizeof(me)};
	if (Module32FirstW(hSnap, &me))
	{
		do
		{
			const int item = g_moduleList.AddItem(me.szModule);
			g_moduleList.SetItemText(item, 1, me.szExePath);
			WCHAR sizeStr[32];
			_snwprintf_s(sizeStr, _countof(sizeStr), _TRUNCATE, L"%u KB", me.modBaseSize / 1024);
			g_moduleList.SetItemText(item, 2, sizeStr);
		}
		while (Module32NextW(hSnap, &me));
	}
	CloseHandle(hSnap);
	g_moduleList.SetRedraw(true);
}

static void ShowInExplorer(const WCHAR* path)
{
	if (!path || !path[0]) return;
	WCHAR cmd[MAX_PATH + 32];
	_snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"/select,\"%s\"", path);
	ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd, nullptr, SW_SHOWNORMAL);
}

static void KillSelectedProcess()
{
	const int idx = SelectedModelIndex(g_procList, g_processes.size());
	if (idx < 0) return;
	const ProcessInfo& pi = g_processes[idx];

	WCHAR prompt[MAX_PATH + 64];
	_snwprintf_s(prompt, _countof(prompt), _TRUNCATE,
	             L"Terminate %s (PID %u)?\n\nUnsaved work in this process will be lost.",
	             pi.name.c_str(), pi.pid);
	if (!ConfirmAction(g_hMainWnd, prompt)) return;

	const HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pi.pid);
	if (hProc)
	{
		const BOOL ok = TerminateProcess(hProc, 1);
		CloseHandle(hProc);
		if (!ok)
			MessageBox(g_hMainWnd, L"Failed to terminate process.", L"System Stuff", MB_OK | MB_ICONERROR);
		RefreshProcessList();
	}
	else
	{
		MessageBox(g_hMainWnd, L"Could not open process for termination.\nAccess denied?",
			L"System Stuff", MB_OK | MB_ICONERROR);
	}
}

static void ShowSelectedProcessInExplorer()
{
	const int idx = SelectedModelIndex(g_procList, g_processes.size());
	if (idx >= 0 && !g_processes[idx].path.empty())
		ShowInExplorer(g_processes[idx].path.c_str());
}

static void OnProcessContextMenu(const HWND hWnd, const int x, const int y)
{
	const int idx = SelectedModelIndex(g_procList, g_processes.size());
	if (idx < 0) return;
	const bool hasPath = !g_processes[idx].path.empty();
	const UINT cmd = RunPopupMenu(hWnd, x, y,
	                              {{1, L"Show in Explorer", hasPath}, {2, L"Kill Process"}});
	if (cmd == 1) ShowSelectedProcessInExplorer();
	else if (cmd == 2) KillSelectedProcess();
}

static void OnModuleContextMenu(const HWND hWnd, const int x, const int y)
{
	const int sel = g_moduleList.GetSelected();
	if (sel < 0) return;
	const std::wstring path = g_moduleList.GetItemText(sel, 1);
	const UINT cmd = RunPopupMenu(hWnd, x, y, {{1, L"Show in Explorer"}});
	if (cmd == 1 && !path.empty()) ShowInExplorer(path.c_str());
}

// ============================================================
// Network tab
// ============================================================
static CustomListView g_netList;

static const WCHAR* TcpStateStr(const DWORD state)
{
	switch (state)
	{
	case MIB_TCP_STATE_CLOSED: return L"CLOSED";
	case MIB_TCP_STATE_LISTEN: return L"LISTENING";
	case MIB_TCP_STATE_SYN_SENT: return L"SYN_SENT";
	case MIB_TCP_STATE_SYN_RCVD: return L"SYN_RCVD";
	case MIB_TCP_STATE_ESTAB: return L"ESTABLISHED";
	case MIB_TCP_STATE_FIN_WAIT1: return L"FIN_WAIT1";
	case MIB_TCP_STATE_FIN_WAIT2: return L"FIN_WAIT2";
	case MIB_TCP_STATE_CLOSE_WAIT: return L"CLOSE_WAIT";
	case MIB_TCP_STATE_CLOSING: return L"CLOSING";
	case MIB_TCP_STATE_LAST_ACK: return L"LAST_ACK";
	case MIB_TCP_STATE_TIME_WAIT: return L"TIME_WAIT";
	default: return L"?";
	}
}

static void FormatAddr(const int family, const void* addr, const DWORD port, const DWORD scopeId,
                       WCHAR* buf, const size_t bufLen)
{
	WCHAR ip[INET6_ADDRSTRLEN] = {};
	if (!InetNtopW(family, addr, ip, _countof(ip)))
		wcscpy_s(ip, L"?");
	const WORD p = ntohs(static_cast<WORD>(port));
	if (family == AF_INET6 && scopeId != 0)
		_snwprintf_s(buf, bufLen, _TRUNCATE, L"[%s%%%u]:%u", ip, scopeId, p);
	else if (family == AF_INET6)
		_snwprintf_s(buf, bufLen, _TRUNCATE, L"[%s]:%u", ip, p);
	else
		_snwprintf_s(buf, bufLen, _TRUNCATE, L"%s:%u", ip, p);
}

// pid -> image name, sorted by pid for binary search.
struct PidName
{
	DWORD pid;
	std::wstring name;
};

static std::vector<PidName> SnapshotPidNames()
{
	std::vector<PidName> names;
	const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) return names;
	PROCESSENTRY32W pe = {sizeof(pe)};
	if (Process32FirstW(hSnap, &pe))
		do { names.push_back({pe.th32ProcessID, pe.szExeFile}); }
		while (Process32NextW(hSnap, &pe));
	CloseHandle(hSnap);
	std::sort(names.begin(), names.end(), [](const PidName& a, const PidName& b) { return a.pid < b.pid; });
	return names;
}

static std::wstring LookupPidName(const std::vector<PidName>& names, const DWORD pid)
{
	const auto it = std::lower_bound(names.begin(), names.end(), pid,
	                                 [](const PidName& a, const DWORD p) { return a.pid < p; });
	return (it != names.end() && it->pid == pid) ? it->name : std::wstring();
}

static void RefreshNetworkList()
{
	g_netList.SetRedraw(false);
	g_netList.DeleteAllItems();
	g_netList.SetEmptyText(EmptyListText());

	const std::vector<PidName> pidNames = SnapshotPidNames();

	// Adds a row only when it matches the current filter.
	auto addRow = [&](const WCHAR* proto, const WCHAR* local, const WCHAR* remote,
	                  const WCHAR* state, const DWORD pid)
	{
		const std::wstring owner = LookupPidName(pidNames, pid);
		const std::wstring pidStr = Format::U(pid);
		if (!MatchesFilter(std::wstring(proto) + L" " + local + L" " + remote + L" " +
			state + L" " + pidStr + L" " + owner))
			return;
		const int idx = g_netList.AddItem(proto);
		g_netList.SetItemText(idx, 1, local);
		g_netList.SetItemText(idx, 2, remote);
		g_netList.SetItemText(idx, 3, state);
		g_netList.SetItemText(idx, 4, pidStr);
		g_netList.SetItemText(idx, 5, owner);
	};

	WCHAR local[80], remote[80];

	auto* tcp4 = QueryExtTable<MIB_TCPTABLE_OWNER_PID>([](void* buf, ULONG* sz)
	{
		return GetExtendedTcpTable(buf, sz, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
	});
	if (tcp4)
	{
		for (DWORD i = 0; i < tcp4->dwNumEntries; i++)
		{
			const auto& row = tcp4->table[i];
			FormatAddr(AF_INET, &row.dwLocalAddr, row.dwLocalPort, 0, local, _countof(local));
			FormatAddr(AF_INET, &row.dwRemoteAddr, row.dwRemotePort, 0, remote, _countof(remote));
			addRow(L"TCP", local, remote, TcpStateStr(row.dwState), row.dwOwningPid);
		}
	}
	FreeExtTable(tcp4);

	auto* tcp6 = QueryExtTable<MIB_TCP6TABLE_OWNER_PID>([](void* buf, ULONG* sz)
	{
		return GetExtendedTcpTable(buf, sz, TRUE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
	});
	if (tcp6)
	{
		for (DWORD i = 0; i < tcp6->dwNumEntries; i++)
		{
			const auto& row = tcp6->table[i];
			FormatAddr(AF_INET6, row.ucLocalAddr, row.dwLocalPort, row.dwLocalScopeId, local, _countof(local));
			FormatAddr(AF_INET6, row.ucRemoteAddr, row.dwRemotePort, row.dwRemoteScopeId, remote, _countof(remote));
			addRow(L"TCPv6", local, remote, TcpStateStr(row.dwState), row.dwOwningPid);
		}
	}
	FreeExtTable(tcp6);

	auto* udp4 = QueryExtTable<MIB_UDPTABLE_OWNER_PID>([](void* buf, ULONG* sz)
	{
		return GetExtendedUdpTable(buf, sz, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
	});
	if (udp4)
	{
		for (DWORD i = 0; i < udp4->dwNumEntries; i++)
		{
			const auto& row = udp4->table[i];
			FormatAddr(AF_INET, &row.dwLocalAddr, row.dwLocalPort, 0, local, _countof(local));
			addRow(L"UDP", local, L"*:*", L"", row.dwOwningPid);
		}
	}
	FreeExtTable(udp4);

	auto* udp6 = QueryExtTable<MIB_UDP6TABLE_OWNER_PID>([](void* buf, ULONG* sz)
	{
		return GetExtendedUdpTable(buf, sz, TRUE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
	});
	if (udp6)
	{
		for (DWORD i = 0; i < udp6->dwNumEntries; i++)
		{
			const auto& row = udp6->table[i];
			FormatAddr(AF_INET6, row.ucLocalAddr, row.dwLocalPort, row.dwLocalScopeId, local, _countof(local));
			addRow(L"UDPv6", local, L"*:*", L"", row.dwOwningPid);
		}
	}
	FreeExtTable(udp6);

	g_netList.SetRedraw(true);
}

// ============================================================
// Windows tab
// ============================================================
static CustomListView g_winList;
static CustomListView g_winPropList;

struct WindowInfo
{
	HWND hwnd;
	std::wstring title;
	std::wstring className;
	DWORD pid;
	DWORD tid;
	RECT rect;
};

static std::vector<WindowInfo> g_windows;

static BOOL CALLBACK EnumWindowsForList(const HWND hwnd, const LPARAM lParam)
{
	if (!IsWindowVisible(hwnd)) return TRUE;
	WindowInfo wi;
	wi.hwnd = hwnd;
	WCHAR buf[256] = {};
	GetWindowTextW(hwnd, buf, _countof(buf));
	wi.title = buf;
	GetClassNameW(hwnd, buf, _countof(buf));
	wi.className = buf;
	wi.tid = GetWindowThreadProcessId(hwnd, &wi.pid);
	GetWindowRect(hwnd, &wi.rect);

	if (!MatchesFilter(wi.title + L" " + wi.className)) return TRUE;

	auto* vec = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
	vec->push_back(std::move(wi));
	return TRUE;
}

static void RefreshWindowList()
{
	// Remember the selection by window handle across refreshes.
	const int prevIdx = SelectedModelIndex(g_winList, g_windows.size());
	const HWND prevHwnd = prevIdx >= 0 ? g_windows[prevIdx].hwnd : nullptr;

	g_winList.SetRedraw(false);
	g_winList.DeleteAllItems();
	g_winList.SetEmptyText(EmptyListText());
	g_windows.clear();
	EnumWindows(EnumWindowsForList, reinterpret_cast<LPARAM>(&g_windows));

	for (int i = 0; i < static_cast<int>(g_windows.size()); i++)
	{
		auto& wi = g_windows[i];
		const int idx = g_winList.AddItem(wi.title.empty() ? wi.className.c_str() : wi.title.c_str());
		g_winList.SetItemData(idx, i);
		g_winList.SetItemText(idx, 1, wi.className.c_str());
		g_winList.SetItemText(idx, 2, Format::U(wi.pid));
		g_winList.SetItemText(idx, 3, Format::Ptr(wi.hwnd));
	}

	g_winList.SetRedraw(true);

	if (prevHwnd)
	{
		for (int i = 0; i < static_cast<int>(g_windows.size()); i++)
			if (g_windows[i].hwnd == prevHwnd)
			{
				const int row = g_winList.FindItemByData(i);
				if (row >= 0) g_winList.SetSelected(row);
				break;
			}
	}

	ShowWindowProperties();
	UpdateWindowButtons();
}

static void ShowWindowProperties()
{
	g_winPropList.SetRedraw(false);
	g_winPropList.DeleteAllItems();
	const int idx = SelectedModelIndex(g_winList, g_windows.size());
	if (idx < 0)
	{
		g_winPropList.SetRedraw(true);
		return;
	}
	auto& wi = g_windows[idx];

	auto addProp = [](const WCHAR* name, const std::wstring& value)
	{
		const int item = g_winPropList.AddItem(name);
		g_winPropList.SetItemText(item, 1, value.c_str());
	};

	addProp(L"Title", wi.title);
	addProp(L"Class", wi.className);
	addProp(L"PID", Format::U(wi.pid));
	addProp(L"Thread ID", Format::U(wi.tid));
	addProp(L"Handle", Format::Ptr(wi.hwnd));
	WCHAR buf[128];
	_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d, %d, %d, %d", wi.rect.left, wi.rect.top, wi.rect.right,
	             wi.rect.bottom);
	addProp(L"Rect", buf);
	_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d \u00D7 %d",
	             wi.rect.right - wi.rect.left, wi.rect.bottom - wi.rect.top);
	addProp(L"Size", buf);
	addProp(L"Style", Format::Hex32(static_cast<unsigned>(GetWindowLong(wi.hwnd, GWL_STYLE))));
	addProp(L"ExStyle", Format::Hex32(static_cast<unsigned>(GetWindowLong(wi.hwnd, GWL_EXSTYLE))));

	g_winPropList.SetRedraw(true);
}

// ============================================================
// About tab (custom drawn panel)
// ============================================================

static void CollectAboutRows(std::vector<std::pair<std::wstring, std::wstring>>& rows)
{
	rows.clear();
	WCHAR buf[256];

	DWORD nameLen = _countof(buf);
	if (GetComputerNameW(buf, &nameLen)) rows.emplace_back(L"Computer Name", buf);
	nameLen = _countof(buf);
	if (GetUserNameW(buf, &nameLen)) rows.emplace_back(L"User Name", buf);

	OSVERSIONINFOW ovi = {sizeof(ovi)};
	HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
	if (hNtDll)
	{
		typedef LONG (WINAPI* RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);
		if (auto pRtlGetVersion = reinterpret_cast<RtlGetVersionFunc>(GetProcAddress(hNtDll, "RtlGetVersion")))
			pRtlGetVersion(&ovi);
	}
	if (ovi.dwMajorVersion == 0)
	{
#pragma warning(suppress: 4996)
		GetVersionExW(&ovi);
	}
	_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%u.%u.%u", ovi.dwMajorVersion, ovi.dwMinorVersion,
				 ovi.dwBuildNumber);
	rows.emplace_back(L"OS Version", buf);

	SYSTEM_INFO si = {};
	GetNativeSystemInfo(&si);

	const WCHAR* arch = L"Unknown";
	switch (si.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64: arch = L"x64"; break;
	case PROCESSOR_ARCHITECTURE_ARM64: arch = L"ARM64"; break;
	case PROCESSOR_ARCHITECTURE_INTEL: arch = L"x86"; break;
	}
	rows.emplace_back(L"Architecture", arch);

	if (!g_cpuName.empty())
		rows.emplace_back(L"Processor", g_cpuName);
	rows.emplace_back(L"Logical Cores", Format::U(si.dwNumberOfProcessors));
	if (g_cpuMhz > 0)
		rows.emplace_back(L"CPU Speed", Format::Ghz(g_cpuMhz));

	if (!g_gpuName.empty())
		rows.emplace_back(L"Graphics", g_gpuName);

	MEMORYSTATUSEX ms = {sizeof(ms)};
	GlobalMemoryStatusEx(&ms);
	rows.emplace_back(L"Total RAM", Format::Gb(ms.ullTotalPhys));
	rows.emplace_back(L"Available RAM", Format::Gb(ms.ullAvailPhys));
	_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%u%%", ms.dwMemoryLoad);
	rows.emplace_back(L"Memory Load", buf);

	if (g_diskTotalBytes > 0)
	{
		WCHAR label[32];
		_snwprintf_s(label, _countof(label), _TRUNCATE, L"Disk %c: Total", g_systemDriveLetter);
		rows.emplace_back(label, Format::Gb(g_diskTotalBytes));
		_snwprintf_s(label, _countof(label), _TRUNCATE, L"Disk %c: Free", g_systemDriveLetter);
		rows.emplace_back(label, Format::Gb(g_diskFreeBytes));
	}

	const ULONGLONG uptime = GetTickCount64();
	const ULONGLONG hours = uptime / 3600000;
	const ULONGLONG mins = (uptime % 3600000) / 60000;
	_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%llu hours, %llu minutes", hours, mins);
	rows.emplace_back(L"System Uptime", buf);
}

static void CopyAboutInfoToClipboard()
{
	std::vector<std::pair<std::wstring, std::wstring>> rows;
	CollectAboutRows(rows);

	std::wstring text = L"System Stuff - System Information\r\n";
	text += L"================================\r\n";
	for (const auto& r : rows)
	{
		text += r.first;
		text += L": ";
		text += r.second;
		text += L"\r\n";
	}

	if (!OpenClipboard(g_hMainWnd)) return;
	EmptyClipboard();
	const size_t bytes = (text.size() + 1) * sizeof(WCHAR);
	if (const HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes))
	{
		bool owned = false;
		if (auto* dst = static_cast<WCHAR*>(GlobalLock(hMem)))
		{
			memcpy(dst, text.c_str(), bytes);
			GlobalUnlock(hMem);
			owned = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;
		}
		if (!owned) GlobalFree(hMem); // clipboard only takes ownership on success
	}
	CloseClipboard();
}

// About panel scroll state
static VScroll g_aboutScroll;
static std::vector<std::pair<std::wstring, std::wstring>> g_aboutRows;
static ULONGLONG g_aboutRowsCollectedTick = 0;

static void RefreshAboutInfo()
{
	g_aboutRows.clear(); // force recollect on next paint
	if (g_hAboutPanel) InvalidateRect(g_hAboutPanel, nullptr, FALSE);
}

static void EnsureAboutRows()
{
	const ULONGLONG now = GetTickCount64();
	// Recollect at most every 1s; volatile fields (uptime, RAM) can be stale briefly.
	if (g_aboutRows.empty() || now - g_aboutRowsCollectedTick > 1000)
	{
		CollectAboutRows(g_aboutRows);
		g_aboutRowsCollectedTick = now;
	}
}

static void DrawDarkButton(const DRAWITEMSTRUCT* dis)
{
	const HDC hdc = dis->hDC;
	const RECT rc = dis->rcItem;
	const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
	const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
	const bool focused = (dis->itemState & ODS_FOCUS) != 0;

	COLORREF bg = pressed ? RGB(0, 100, 180) : RGB(55, 55, 60);
	if (disabled) bg = RGB(40, 40, 43);
	const HBRUSH br = CreateSolidBrush(bg);
	FillRect(hdc, &rc, br);
	DeleteObject(br);

	const HPEN pen = CreatePen(PS_SOLID, 1, focused && !disabled ? RGB(0, 150, 255) : Dark::Border);
	const auto oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
	SelectObject(hdc, GetStockObject(NULL_BRUSH));
	Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
	SelectObject(hdc, oldPen);
	DeleteObject(pen);

	WCHAR text[64] = {};
	GetWindowTextW(dis->hwndItem, text, 64);
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, disabled ? RGB(100, 100, 100) : Dark::Text);
	SelectObject(hdc, g_hFont);
	DrawTextExt(hdc, text, -1, rc, TextAlign::Center | TextAlign::VCenter);
}

static void UpdateProcessButtons()
{
	const int idx = SelectedModelIndex(g_procList, g_processes.size());
	EnableWindow(g_btnExplorerProc, idx >= 0 && !g_processes[idx].path.empty());
	EnableWindow(g_btnKillProc, idx >= 0);
}

static void UpdateWindowButtons()
{
	const bool hasSel = SelectedModelIndex(g_winList, g_windows.size()) >= 0;
	EnableWindow(g_btnExplorerWin, hasSel);
	EnableWindow(g_btnKillWin, hasSel);
}

static void ShowWindowProcessInExplorer()
{
	const int idx = SelectedModelIndex(g_winList, g_windows.size());
	if (idx < 0) return;
	const HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_windows[idx].pid);
	if (!hProc)
	{
		MessageBox(g_hMainWnd, L"Could not open the owning process.\nAccess denied?",
			L"System Stuff", MB_OK | MB_ICONERROR);
		return;
	}
	WCHAR path[MAX_PATH] = {};
	DWORD len = MAX_PATH;
	if (QueryFullProcessImageNameW(hProc, 0, path, &len))
		ShowInExplorer(path);
	CloseHandle(hProc);
}

static void KillSelectedWindow()
{
	const int idx = SelectedModelIndex(g_winList, g_windows.size());
	if (idx < 0) return;
	const WindowInfo& wi = g_windows[idx];

	WCHAR prompt[320];
	_snwprintf_s(prompt, _countof(prompt), _TRUNCATE, L"Close \"%s\"?",
	             wi.title.empty() ? wi.className.c_str() : wi.title.c_str());
	if (!ConfirmAction(g_hMainWnd, prompt)) return;
	PostMessage(wi.hwnd, WM_CLOSE, 0, 0);
	// Don't refresh immediately: WM_CLOSE is async and may be cancelled.
	// User can hit F5 / Refresh to see the result.
}

// Sync About panel's VScroll geometry from the current client rect.
// Content height is derived from the row count so the scrollbar is correct on
// the very first paint, before any drawing has happened.
static void SyncAboutScroll(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	const int sbW = Dpi::Scale(Layout::ScrollbarWidth);
	g_aboutScroll.viewH = rc.bottom;
	g_aboutScroll.trackTop = 0;
	g_aboutScroll.trackBottom = rc.bottom;
	g_aboutScroll.sbX = rc.right - sbW;
	g_aboutScroll.sbRight = rc.right;

	EnsureAboutRows();
	const int headerH = Dpi::Scale(Layout::AboutTitleSpacing)
		+ Dpi::Scale(Layout::AboutSubtitleSpacing)
		+ Dpi::Scale(Layout::AboutPostCopyrightSpacing)
		+ Dpi::Scale(Layout::AboutPostSepSpacing);
	g_aboutScroll.contentH = Dpi::Scale(Layout::AboutTopMargin) * 2 + headerH
		+ static_cast<int>(g_aboutRows.size()) * Dpi::Scale(Layout::AboutRowSpacing);
	g_aboutScroll.Clamp();
}

static LRESULT CALLBACK AboutPanelProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
		{
			SyncAboutScroll(hWnd);
			DoubleBuffer db(hWnd);
			const RECT& rc = db.rc;
			HDC memDC = db.dc;

			FillRect(memDC, &rc, Dark::BrushWindow());
			SetBkMode(memDC, TRANSPARENT);

			const int leftMargin = Dpi::Scale(Layout::AboutLeftMargin);
			const int contentRight = g_aboutScroll.Needed() ? g_aboutScroll.sbX : rc.right;
			const int rightEdge = contentRight - Dpi::Scale(Layout::AboutRightMargin);
			int y = Dpi::Scale(Layout::AboutTopMargin) - g_aboutScroll.pos;

			// App icon (left of title block)
			const int iconSize = Dpi::Scale(Layout::AboutIconSize);
			const int textLeft = leftMargin + iconSize + Dpi::Scale(Layout::AboutIconGap);
			static HICON s_hAboutIcon = nullptr;
			static int s_aboutIconSize = 0;
			if (!s_hAboutIcon || s_aboutIconSize != iconSize)
			{
				if (s_hAboutIcon) DestroyIcon(s_hAboutIcon);
				s_hAboutIcon = static_cast<HICON>(LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_LIGHT),
					IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
				s_aboutIconSize = iconSize;
			}
			if (s_hAboutIcon)
			{
				DrawIconEx(memDC, leftMargin, y, s_hAboutIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
			}

			// Title
			SelectObject(memDC, g_hFontTitle);
			SetTextColor(memDC, RGB(255, 255, 255));
			const RECT titleRc = {textLeft, y, rightEdge, y + Dpi::Scale(Layout::AboutTitleHeight)};
			DrawTextExt(memDC, L"System Stuff", -1, titleRc, TextAlign::Left);
			y += Dpi::Scale(Layout::AboutTitleSpacing);

			// Subtitle
			SelectObject(memDC, g_hFontSubtitle);
			SetTextColor(memDC, RGB(180, 180, 180));
			const RECT verRc = {textLeft, y, rightEdge, y + Dpi::Scale(Layout::AboutSubtitleHeight)};
			DrawTextExt(memDC, L"Version 2.0 \u2014 System Information Utility", -1, verRc, TextAlign::Left);
			y += Dpi::Scale(Layout::AboutSubtitleSpacing);

			// Copyright
			SelectObject(memDC, g_hFont);
			SetTextColor(memDC, RGB(140, 140, 140));
			const RECT copyrRc = {textLeft, y, rightEdge, y + Dpi::Scale(Layout::AboutCopyrightHeight)};
			DrawTextExt(memDC, L"Copyright \u00A9 2002 Zac Walker", -1, copyrRc, TextAlign::Left);
			y += Dpi::Scale(Layout::AboutPostCopyrightSpacing);

			// Separator
			const HPEN pen = CreatePen(PS_SOLID, 1, Dark::Border);
			const auto oldPen = static_cast<HPEN>(SelectObject(memDC, pen));
			MoveToEx(memDC, leftMargin, y, nullptr);
			LineTo(memDC, rightEdge, y);
			SelectObject(memDC, oldPen);
			DeleteObject(pen);
			y += Dpi::Scale(Layout::AboutPostSepSpacing);

			// System info rows
			auto drawRow = [&](const WCHAR* label, const WCHAR* value)
			{
				const int rowH = Dpi::Scale(Layout::AboutRowHeight);
				const int labelW = Dpi::Scale(Layout::AboutLabelWidth);
				const int valueX = leftMargin + Dpi::Scale(Layout::AboutValueOffset);
				SelectObject(memDC, g_hFontBold);
				SetTextColor(memDC, RGB(140, 140, 140));
				const RECT lr = {leftMargin, y, leftMargin + labelW, y + rowH};
				DrawTextExt(memDC, label, -1, lr, TextAlign::Left | TextAlign::VCenter);
				SelectObject(memDC, g_hFont);
				SetTextColor(memDC, RGB(220, 220, 220));
				const RECT vr = {valueX, y, rightEdge, y + rowH};
				DrawTextExt(memDC, value, -1, vr, TextAlign::Left | TextAlign::VCenter);
				y += Dpi::Scale(Layout::AboutRowSpacing);
			};

			std::vector<std::pair<std::wstring, std::wstring>>& rows = g_aboutRows;
			EnsureAboutRows();
			for (const auto& r : rows)
				drawRow(r.first.c_str(), r.second.c_str());

			g_aboutScroll.Draw(memDC);
			return 0;
		}
	case WM_ERASEBKGND: return 1;

	case WM_GETDLGCODE: return DLGC_WANTARROWS;

	case WM_MOUSEWHEEL:
		{
			SyncAboutScroll(hWnd);
			if (g_aboutScroll.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam),
			                               Dpi::Scale(Layout::AboutRowSpacing)))
				InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}

	case WM_LBUTTONDOWN:
		{
			SetFocus(hWnd);
			SyncAboutScroll(hWnd);
			if (g_aboutScroll.OnLButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
				InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}

	case WM_LBUTTONUP:
		g_aboutScroll.OnLButtonUp();
		return 0;

	case WM_MOUSEMOVE:
		SyncAboutScroll(hWnd);
		if (g_aboutScroll.OnMouseMove(GET_Y_LPARAM(lParam)))
			InvalidateRect(hWnd, nullptr, FALSE);
		return 0;

	case WM_KEYDOWN:
		{
			SyncAboutScroll(hWnd);
			const int line = Dpi::Scale(Layout::AboutRowSpacing);
			if (wParam == VK_UP) g_aboutScroll.pos -= line;
			else if (wParam == VK_DOWN) g_aboutScroll.pos += line;
			else if (wParam == VK_PRIOR) g_aboutScroll.pos -= g_aboutScroll.viewH;
			else if (wParam == VK_NEXT) g_aboutScroll.pos += g_aboutScroll.viewH;
			else if (wParam == VK_HOME) g_aboutScroll.pos = 0;
			else if (wParam == VK_END) g_aboutScroll.pos = g_aboutScroll.MaxScroll();
			else break;
			g_aboutScroll.Clamp();
			InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================
// Tab page proc
// ============================================================
static LRESULT CALLBACK TabPageProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
		{
			const auto hdc = (HDC)wParam;
			RECT rc;
			GetClientRect(hWnd, &rc);
			FillRect(hdc, &rc, Dark::BrushWindow());
			return 1;
		}
	case WM_CTLCOLOREDIT:
		return Dark::OnCtlColorEdit((HDC)wParam);
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================
// Layout
// ============================================================
static constexpr int SPLITTER_WIDTH = 4;
static int g_procSplitter = 0;
static int g_winSplitter = 0;

static void LayoutTabPages()
{
	RECT rc;
	GetClientRect(g_hMainWnd, &rc);
	const int tabH = Dpi::Scale(Layout::TabHeight);
	const bool hasBottomBar = (g_nCurrentTab != TAB_PERFORMANCE);
	const int barH = hasBottomBar ? Dpi::Scale(Layout::BottomBarHeight) : 0;

	const int pageH = rc.bottom - tabH - barH;
	const int pageW = rc.right;

	for (int i = 0; i < TAB_COUNT; i++)
		if (g_hTabPages[i])
			MoveWindow(g_hTabPages[i], 0, tabH, pageW, pageH, TRUE);

	// Bottom bar: filter (left) + buttons (right), all on one line
	const int barY = rc.bottom - barH;
	const int btnH = Dpi::Scale(Layout::ButtonHeight);
	const int btnW = Dpi::Scale(Layout::ButtonWidth);
	const int btnSpace = Dpi::Scale(Layout::ButtonSpacing);
	const int btnY = barY + (barH - btnH) / 2;

	// Filter edit
	const bool needsFilter = (g_nCurrentTab == TAB_PROCESSES || g_nCurrentTab == TAB_NETWORK ||
		g_nCurrentTab == TAB_WINDOWS);
	ShowWindow(g_hFilterEdit, (hasBottomBar && needsFilter) ? SW_SHOW : SW_HIDE);
	if (hasBottomBar && needsFilter)
	{
		const int filterW = Dpi::Scale(Layout::FilterWidth);
		MoveWindow(g_hFilterEdit, btnSpace, btnY, filterW, btnH, TRUE);
	}

	// Hide all buttons then show per-tab
	auto hideBtn = [](const HWND btn) { if (btn) ShowWindow(btn, SW_HIDE); };
	hideBtn(g_btnRefreshProc);
	hideBtn(g_btnExplorerProc);
	hideBtn(g_btnKillProc);
	hideBtn(g_btnRefreshNet);
	hideBtn(g_btnRefreshWin);
	hideBtn(g_btnExplorerWin);
	hideBtn(g_btnKillWin);
	hideBtn(g_btnRefreshAbout);
	hideBtn(g_btnCopyAbout);
	hideBtn(g_btnReportIssue);

	auto layoutButtons = [&](const std::vector<HWND>& btns)
	{
		int btnX = rc.right;
		for (int i = static_cast<int>(btns.size()) - 1; i >= 0; i--)
		{
			btnX -= btnSpace + btnW;
			MoveWindow(btns[i], btnX, btnY, btnW, btnH, TRUE);
			ShowWindow(btns[i], SW_SHOW);
		}
	};

	switch (g_nCurrentTab)
	{
	case TAB_PROCESSES:
		layoutButtons({g_btnRefreshProc, g_btnExplorerProc, g_btnKillProc});
		break;
	case TAB_NETWORK:
		layoutButtons({g_btnRefreshNet});
		break;
	case TAB_WINDOWS:
		layoutButtons({g_btnRefreshWin, g_btnExplorerWin, g_btnKillWin});
		break;
	case TAB_ABOUT:
		layoutButtons({g_btnReportIssue, g_btnCopyAbout, g_btnRefreshAbout});
		break;
	}

	// Performance
	if (g_hPerfPanel)
	{
		RECT pr;
		GetClientRect(g_hTabPages[TAB_PERFORMANCE], &pr);
		MoveWindow(g_hPerfPanel, 0, 0, pr.right, pr.bottom, TRUE);
	}

	// Process split (always 50%)
	if (g_procList.GetHWND() && g_moduleList.GetHWND())
	{
		RECT pr;
		GetClientRect(g_hTabPages[TAB_PROCESSES], &pr);
		const int splitter = Dpi::Scale(SPLITTER_WIDTH);
		g_procSplitter = pr.right / 2;
		MoveWindow(g_procList.GetHWND(), 0, 0, g_procSplitter, pr.bottom, TRUE);
		MoveWindow(g_moduleList.GetHWND(), g_procSplitter + splitter, 0,
		           pr.right - g_procSplitter - splitter, pr.bottom, TRUE);
	}

	// Network
	if (g_netList.GetHWND())
	{
		RECT pr;
		GetClientRect(g_hTabPages[TAB_NETWORK], &pr);
		MoveWindow(g_netList.GetHWND(), 0, 0, pr.right, pr.bottom, TRUE);
	}

	// Windows split (always 50%)
	if (g_winList.GetHWND() && g_winPropList.GetHWND())
	{
		RECT pr;
		GetClientRect(g_hTabPages[TAB_WINDOWS], &pr);
		const int splitter = Dpi::Scale(SPLITTER_WIDTH);
		g_winSplitter = pr.right / 2;
		MoveWindow(g_winList.GetHWND(), 0, 0, g_winSplitter, pr.bottom, TRUE);
		MoveWindow(g_winPropList.GetHWND(), g_winSplitter + splitter, 0,
		           pr.right - g_winSplitter - splitter, pr.bottom, TRUE);
	}

	// About
	if (g_hAboutPanel)
	{
		RECT pr;
		GetClientRect(g_hTabPages[TAB_ABOUT], &pr);
		MoveWindow(g_hAboutPanel, 0, 0, pr.right, pr.bottom, TRUE);
	}
}

static void RefreshCurrentView()
{
	switch (g_nCurrentTab)
	{
	case TAB_PROCESSES: RefreshProcessList();
		break;
	case TAB_NETWORK: RefreshNetworkList();
		break;
	case TAB_WINDOWS: RefreshWindowList();
		break;
	case TAB_ABOUT: RefreshAboutInfo();
		break;
	case TAB_PERFORMANCE: InvalidateRect(g_hPerfPanel, nullptr, FALSE);
		break;
	}
}

static void SwitchTab(const int newTab)
{
	if (newTab < 0 || newTab >= TAB_COUNT || newTab == g_nCurrentTab) return;

	// Save current tab's filter text
	WCHAR buf[256] = {};
	GetWindowTextW(g_hFilterEdit, buf, _countof(buf));
	g_tabFilterText[g_nCurrentTab] = buf;

	if (g_hTabPages[g_nCurrentTab]) ShowWindow(g_hTabPages[g_nCurrentTab], SW_HIDE);
	g_nCurrentTab = newTab;
	if (g_hTabPages[g_nCurrentTab]) ShowWindow(g_hTabPages[g_nCurrentTab], SW_SHOW);
	g_tabCtrl.SetCurSel(g_nCurrentTab);

	// Restore new tab's filter text
	SetFilterText(g_tabFilterText[g_nCurrentTab]);
	g_updatingFilterEdit = true;
	SetWindowTextW(g_hFilterEdit, g_filterText.c_str());
	g_updatingFilterEdit = false;

	LayoutTabPages();

	// Snapshots go stale while a tab is hidden, so re-collect on activation.
	RefreshCurrentView();
}

// ============================================================
// Main window
// ============================================================
static void CreateAppFonts()
{
	g_hFont         = MakeUiFont(13, FW_NORMAL);
	g_hFontBold     = MakeUiFont(13, FW_BOLD);
	g_hFontChart    = MakeUiFont(20, FW_BOLD);
	g_hFontTitle    = MakeUiFont(32, FW_BOLD);
	g_hFontSubtitle = MakeUiFont(18, FW_NORMAL);
}

static void DestroyAppFonts()
{
	for (HFONT* f : {&g_hFont, &g_hFontBold, &g_hFontChart, &g_hFontTitle, &g_hFontSubtitle})
	{
		if (*f) DeleteObject(*f);
		*f = nullptr;
	}
}

// Re-scales every cached metric after the window moves to a monitor with a different DPI.
static void OnDpiChanged(const HWND hWnd, const UINT newDpi, const RECT* suggested)
{
	const float oldScale = Dpi::g_scale;
	Dpi::g_scale = newDpi / 96.0f;
	const float ratio = (oldScale > 0) ? Dpi::g_scale / oldScale : 1.0f;

	// Hand the new fonts to every control before destroying the old ones, so nothing
	// can paint with a deleted HFONT in between.
	HFONT old[] = {g_hFont, g_hFontBold, g_hFontChart, g_hFontTitle, g_hFontSubtitle};
	CreateAppFonts();

	g_tabCtrl.SetHeight(Dpi::Scale(Layout::TabHeight));
	g_tabCtrl.SetFont(g_hFont);
	for (CustomListView* lv : {&g_procList, &g_moduleList, &g_netList, &g_winList, &g_winPropList})
		lv->OnDpiChanged(g_hFont, g_hFontBold, ratio);

	for (const HWND ctl : {g_btnRefreshProc, g_btnExplorerProc, g_btnKillProc, g_btnRefreshNet,
		     g_btnRefreshWin, g_btnExplorerWin, g_btnKillWin, g_btnRefreshAbout,
		     g_btnCopyAbout, g_btnReportIssue, g_hFilterEdit})
		if (ctl) SendMessage(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), TRUE);

	SetWindowPos(hWnd, nullptr, suggested->left, suggested->top,
	             suggested->right - suggested->left, suggested->bottom - suggested->top,
	             SWP_NOZORDER | SWP_NOACTIVATE);
	InvalidateRect(hWnd, nullptr, TRUE);

	for (const HFONT f : old)
		if (f) DeleteObject(f);
}

static LRESULT CALLBACK MainWndProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		{
			g_hMainWnd = hWnd;
			Dpi::g_scale = GetDpiForWindow(hWnd) / 96.0f;
			CreateAppFonts();

			// Custom tab control
			g_tabCtrl.Create(hWnd, g_hInst, Dpi::Scale(Layout::TabHeight), g_hFont);
			for (int i = 0; i < TAB_COUNT; i++)
				g_tabCtrl.AddTab(g_tabNames[i]);
			g_tabCtrl.SetChangeCallback([](const int idx) { SwitchTab(idx); });

			RegisterSimpleClass(g_hInst, L"SystemStuffTabPage", TabPageProc);

			// WS_EX_CONTROLPARENT lets IsDialogMessage tab into the lists on each page.
			for (int i = 0; i < TAB_COUNT; i++)
				g_hTabPages[i] = CreateWindowEx(WS_EX_CONTROLPARENT, L"SystemStuffTabPage", L"",
				                                WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, hWnd, nullptr, g_hInst,
				                                nullptr);

			RegisterSimpleClass(g_hInst, L"SystemStuffPerfPanel", PerfPanelProc);

			g_hPerfPanel = CreateWindowEx(0, L"SystemStuffPerfPanel", L"",
			                              WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, g_hTabPages[TAB_PERFORMANCE], nullptr,
			                              g_hInst, nullptr);
			InitPerfCounters();
			g_perfTimer = SetTimer(g_hPerfPanel, 1, 1000, nullptr);

			// Process list
			g_procList.Create(g_hTabPages[TAB_PROCESSES], g_hInst, g_hFont, g_hFontBold);
			g_procList.AddColumn(L"Name", 200);
			g_procList.AddColumn(L"PID", 70);
			g_procList.AddColumn(L"Memory", 100);
			g_procList.SetSelectionCallback([](const int row)
			{
				const int idx = SelectedModelIndex(g_procList, g_processes.size());
				if (idx >= 0) RefreshModuleList(g_processes[idx].pid);
				else g_moduleList.DeleteAllItems();
				(void)row;
				UpdateProcessButtons();
			});
			g_procList.SetContextMenuCallback([](const HWND hw, const int x, const int y)
			{
				(void)hw;
				OnProcessContextMenu(g_hMainWnd, x, y);
			});

			// Module list
			g_moduleList.Create(g_hTabPages[TAB_PROCESSES], g_hInst, g_hFont, g_hFontBold);
			g_moduleList.AddColumn(L"Module", 180);
			g_moduleList.AddColumn(L"Path", 300);
			g_moduleList.AddColumn(L"Size", 80);
			g_moduleList.SetContextMenuCallback([](const HWND hw, const int x, const int y)
			{
				(void)hw;
				OnModuleContextMenu(g_hMainWnd, x, y);
			});

			// Network list
			g_netList.Create(g_hTabPages[TAB_NETWORK], g_hInst, g_hFont, g_hFontBold);
			g_netList.AddColumn(L"Proto", 60);
			g_netList.AddColumn(L"Local Address", 190);
			g_netList.AddColumn(L"Remote Address", 190);
			g_netList.AddColumn(L"State", 110);
			g_netList.AddColumn(L"PID", 70);
			g_netList.AddColumn(L"Process", 160);

			// Windows list
			g_winList.Create(g_hTabPages[TAB_WINDOWS], g_hInst, g_hFont, g_hFontBold);
			g_winList.AddColumn(L"Title", 250);
			g_winList.AddColumn(L"Class", 150);
			g_winList.AddColumn(L"PID", 70);
			g_winList.AddColumn(L"Handle", 80);
			g_winList.SetSelectionCallback([](int)
			{
				ShowWindowProperties();
				UpdateWindowButtons();
			});

			// Window properties list
			g_winPropList.Create(g_hTabPages[TAB_WINDOWS], g_hInst, g_hFont, g_hFontBold);
			g_winPropList.AddColumn(L"Property", 120);
			g_winPropList.AddColumn(L"Value", 300);

			// About panel
			RegisterSimpleClass(g_hInst, L"SystemStuffAboutPanel", AboutPanelProc);
			g_hAboutPanel = CreateWindowEx(0, L"SystemStuffAboutPanel", L"",
			                               WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 100, 100,
			                               g_hTabPages[TAB_ABOUT], nullptr, g_hInst, nullptr);

			// Create buttons (owner-draw, parented to main window for bottom bar)
			const int btnW = Dpi::Scale(Layout::ButtonWidth);
			const int btnH = Dpi::Scale(Layout::ButtonHeight);
			auto makeBtn = [&](const int id, const WCHAR* text) -> HWND
			{
				const HWND btn = CreateWindowEx(0, L"BUTTON", text,
				                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
				                                0, 0, btnW, btnH, hWnd,
				                                (HMENU)static_cast<INT_PTR>(id), g_hInst, nullptr);
				SendMessage(btn, WM_SETFONT, (WPARAM)g_hFont, 0);
				return btn;
			};

			g_btnRefreshProc = makeBtn(BTN_REFRESH_PROC, L"Refresh");
			g_btnExplorerProc = makeBtn(BTN_EXPLORER_PROC, L"Open in Explorer");
			g_btnKillProc = makeBtn(BTN_KILL_PROC, L"Kill Process");
			EnableWindow(g_btnExplorerProc, FALSE);
			EnableWindow(g_btnKillProc, FALSE);

			g_btnRefreshNet = makeBtn(BTN_REFRESH_NET, L"Refresh");

			g_btnRefreshWin = makeBtn(BTN_REFRESH_WIN, L"Refresh");
			g_btnExplorerWin = makeBtn(BTN_EXPLORER_WIN, L"Open in Explorer");
			g_btnKillWin = makeBtn(BTN_KILL_WIN, L"Close Window");
			EnableWindow(g_btnExplorerWin, FALSE);
			EnableWindow(g_btnKillWin, FALSE);

			g_btnRefreshAbout = makeBtn(BTN_REFRESH_ABOUT, L"Refresh");
			g_btnCopyAbout = makeBtn(BTN_COPY_ABOUT, L"Copy to Clipboard");
			g_btnReportIssue = makeBtn(BTN_REPORT_ISSUE, L"Report an Issue");

			g_hFilterEdit = CreateWindowEx(0, L"EDIT", L"",
			                               WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
			                               0, 0, Dpi::Scale(Layout::FilterWidth), btnH,
			                               hWnd, (HMENU)ID_FILTER_EDIT, g_hInst, nullptr);
			SendMessage(g_hFilterEdit, WM_SETFONT, (WPARAM)g_hFont, 0);
			SendMessage(g_hFilterEdit, EM_SETCUEBANNER, 0, (LPARAM)L"Filter...");
			SetWindowTheme(g_hFilterEdit, L"", L"");

			WCHAR compName[MAX_COMPUTERNAME_LENGTH + 32];
			DWORD nameLen2 = MAX_COMPUTERNAME_LENGTH + 1;
			if (GetComputerNameW(compName, &nameLen2))
			{
				WCHAR title[256];
				_snwprintf_s(title, _countof(title), _TRUNCATE, L"System Stuff - %s", compName);
				SetWindowText(hWnd, title);
			}

			g_nCurrentTab = TAB_PERFORMANCE;
			for (int i = 0; i < TAB_COUNT; i++)
				ShowWindow(g_hTabPages[i], i == g_nCurrentTab ? SW_SHOW : SW_HIDE);

			// The other tabs snapshot lazily when first activated, so startup stays fast.
			return 0;
		}

	case WM_SIZE:
		{
			RECT rc;
			GetClientRect(hWnd, &rc);
			MoveWindow(g_tabCtrl.GetHWND(), 0, 0, rc.right, Dpi::Scale(Layout::TabHeight), TRUE);
			LayoutTabPages();
			return 0;
		}

	case WM_GETMINMAXINFO:
		{
			// Below this the charts stop rendering and the button bar overlaps the filter.
			auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
			mmi->ptMinTrackSize.x = Dpi::Scale(Layout::MinWindowWidth);
			mmi->ptMinTrackSize.y = Dpi::Scale(Layout::MinWindowHeight);
			return 0;
		}

	case WM_DPICHANGED:
		OnDpiChanged(hWnd, HIWORD(wParam), reinterpret_cast<const RECT*>(lParam));
		return 0;

	case WM_CTLCOLOREDIT:
		return Dark::OnCtlColorEdit((HDC)wParam);

	case WM_DRAWITEM:
		{
			const auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
			if (dis->CtlType == ODT_BUTTON)
			{
				DrawDarkButton(dis);
				return TRUE;
			}
			break;
		}

	case WM_ERASEBKGND:
		{
			const auto hdc = (HDC)wParam;
			RECT rc;
			GetClientRect(hWnd, &rc);
			FillRect(hdc, &rc, Dark::BrushWindow());
			return 1;
		}

	case WM_COMMAND:
		if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == ID_FILTER_EDIT)
		{
			if (g_updatingFilterEdit) return 0;
			WCHAR buf[256] = {};
			GetWindowTextW(g_hFilterEdit, buf, _countof(buf));
			SetFilterText(buf);
			g_tabFilterText[g_nCurrentTab] = g_filterText;
			RefreshCurrentView();
			return 0;
		}
		switch (LOWORD(wParam))
		{
		case IDM_REFRESH: RefreshCurrentView();
			return 0;
		case IDM_NEXT_TAB: SwitchTab((g_nCurrentTab + 1) % TAB_COUNT);
			return 0;
		case IDM_PREV_TAB: SwitchTab((g_nCurrentTab + TAB_COUNT - 1) % TAB_COUNT);
			return 0;
		case IDM_FOCUS_FILTER:
			if (IsWindowVisible(g_hFilterEdit))
			{
				SetFocus(g_hFilterEdit);
				SendMessage(g_hFilterEdit, EM_SETSEL, 0, -1);
			}
			return 0;
		case BTN_REFRESH_PROC: RefreshProcessList();
			return 0;
		case BTN_EXPLORER_PROC: ShowSelectedProcessInExplorer();
			return 0;
		case BTN_KILL_PROC: KillSelectedProcess();
			return 0;
		case BTN_REFRESH_NET: RefreshNetworkList();
			return 0;
		case BTN_REFRESH_WIN: RefreshWindowList();
			return 0;
		case BTN_EXPLORER_WIN: ShowWindowProcessInExplorer();
			return 0;
		case BTN_KILL_WIN: KillSelectedWindow();
			return 0;
		case BTN_REFRESH_ABOUT: RefreshAboutInfo();
			return 0;
		case BTN_COPY_ABOUT: CopyAboutInfoToClipboard();
			return 0;
		case BTN_REPORT_ISSUE:
			ShellExecuteW(hWnd, L"open", L"https://github.com/ZacWalk/system-stuff/issues",
			              nullptr, nullptr, SW_SHOWNORMAL);
			return 0;
		default:
			if (LOWORD(wParam) >= IDM_TAB_FIRST && LOWORD(wParam) < IDM_TAB_FIRST + TAB_COUNT)
			{
				SwitchTab(LOWORD(wParam) - IDM_TAB_FIRST);
				return 0;
			}
			break;
		}
		break;

	case WM_DESTROY:
		DestroyAppFonts();
		UnregisterApplicationRestart();
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(const HINSTANCE hInstance, HINSTANCE, LPWSTR, const int nCmdShow)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	g_hInst = hInstance;
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	// Register with the Restart Manager so Windows relaunches System Stuff after a
	// reboot (e.g., Windows Update) or unexpected system restart. Passing 0
	// for the flags enables restart for all scenarios (crash, hang, patch,
	// reboot). No command line is passed so it relaunches with no args.
	RegisterApplicationRestart(nullptr, 0);

	WNDCLASSEX wc = {sizeof(wc)};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = hInstance;
	// Big icon: taskbar / Alt-Tab (light variant).
	// Small icon: window caption (dark variant tuned for the title bar).
	wc.hIcon = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_LIGHT),
		IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
	wc.hIconSm = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_DARK),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszClassName = L"SystemStuffMainClass";
	RegisterClassEx(&wc);

	g_hMainWnd = CreateWindowEx(0, L"SystemStuffMainClass", L"System Stuff",
	                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
	                            CW_USEDEFAULT, CW_USEDEFAULT, 1000, 650,
	                            nullptr, nullptr, hInstance, nullptr);
	if (!g_hMainWnd) return 1;

	ShowWindow(g_hMainWnd, nCmdShow);
	UpdateWindow(g_hMainWnd);

	const HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL));
	MSG msg = {};
	for (;;)
	{
		const BOOL got = GetMessage(&msg, nullptr, 0, 0);
		if (got == 0 || got == -1) break; // WM_QUIT, or an unrecoverable queue error
		if (TranslateAccelerator(g_hMainWnd, hAccel, &msg)) continue;
		if (IsDialogMessage(g_hMainWnd, &msg)) continue; // Tab / Shift+Tab navigation
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	CoUninitialize();
	return static_cast<int>(msg.wParam);
}
