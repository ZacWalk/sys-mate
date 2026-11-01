#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "native_monitoring.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winternl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cwchar>
#include <format>
#include <map>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sysmate::native
{
	namespace
	{
		std::string utf8(const std::wstring_view text)
		{
			if (text.empty()) return {};
			const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
			                                      text.data(), static_cast<int>(text.size()),
			                                      nullptr, 0, nullptr, nullptr);
			if (bytes <= 0) return {};
			std::string result(static_cast<size_t>(bytes), '\0');
			WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
			                    text.data(), static_cast<int>(text.size()),
			                    result.data(), bytes, nullptr, nullptr);
			return result;
		}

		std::wstring wide(const std::string_view text)
		{
			if (text.empty()) return {};
			const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			                                      text.data(), static_cast<int>(text.size()),
			                                      nullptr, 0);
			if (chars <= 0) return {};
			std::wstring result(static_cast<size_t>(chars), L'\0');
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			                    text.data(), static_cast<int>(text.size()),
			                    result.data(), chars);
			return result;
		}

		std::string format_u64(const uint64_t value)
		{
			std::array<char, 32> buffer = {};
			const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
			return result.ec == std::errc{} ? std::string(buffer.data(), result.ptr) : std::string();
		}

		std::string format_u32(const uint32_t value)
		{
			return format_u64(value);
		}

		std::string format_bytes(const uint64_t bytes)
		{
			static constexpr std::array<std::string_view, 6> units = {
				"B", "KB", "MB", "GB", "TB", "PB"};
			double value = static_cast<double>(bytes);
			size_t unit = 0;
			while (value >= 1024.0 && unit + 1 < units.size())
			{
				value /= 1024.0;
				++unit;
			}
			return unit == 0 ? std::format("{:.0f} {}", value, units[unit])
			                 : std::format("{:.1f} {}", value, units[unit]);
		}

		std::string format_rate(const double bytes_per_second)
		{
			const auto safe = std::max(0.0, bytes_per_second);
			double value = safe;
			static constexpr std::array<std::string_view, 6> units = {
				"B", "KB", "MB", "GB", "TB", "PB"};
			size_t unit = 0;
			while (value >= 1024.0 && unit + 1 < units.size())
			{
				value /= 1024.0;
				++unit;
			}
			return unit == 0 ? std::format("{:.0f} {}/s", value, units[unit])
			                 : std::format("{:.1f} {}/s", value, units[unit]);
		}

		uint64_t filetime_ticks(const FILETIME& time)
		{
			return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
		}

		uint64_t process_creation_time(const HANDLE process)
		{
			FILETIME created = {}, exited = {}, kernel = {}, user = {};
			return GetProcessTimes(process, &created, &exited, &kernel, &user)
				       ? filetime_ticks(created) : 0;
		}

		int folded_compare(const std::string_view left, const std::string_view right)
		{
			const auto fold = [](const unsigned char c) -> unsigned char
			{
				return c >= 'A' && c <= 'Z'
				           ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
			};
			const auto count = std::min(left.size(), right.size());
			for (size_t i = 0; i < count; ++i)
			{
				const auto l = fold(static_cast<unsigned char>(left[i]));
				const auto r = fold(static_cast<unsigned char>(right[i]));
				if (l < r) return -1;
				if (l > r) return 1;
			}
			return left.size() < right.size() ? -1 : left.size() > right.size() ? 1 : 0;
		}

		void copy_text(const HWND hwnd, WCHAR* buffer, const DWORD capacity)
		{
			if (capacity == 0) return;
			buffer[0] = L'\0';
			GetWindowTextW(hwnd, buffer, capacity);
		}

		struct pid_name
		{
			uint32_t pid = 0;
			std::string name;
		};

		std::vector<pid_name> process_names()
		{
			std::vector<pid_name> result;
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (snapshot == INVALID_HANDLE_VALUE) return result;

			PROCESSENTRY32W entry = {sizeof(entry)};
			if (Process32FirstW(snapshot, &entry))
			{
				do
				{
					result.push_back({entry.th32ProcessID, utf8(entry.szExeFile)});
				}
				while (Process32NextW(snapshot, &entry));
			}
			CloseHandle(snapshot);
			std::ranges::sort(result, [](const pid_name& left, const pid_name& right)
			{
				return left.pid < right.pid;
			});
			return result;
		}

		std::string process_name(const std::vector<pid_name>& names, const uint32_t pid)
		{
			const auto it = std::ranges::lower_bound(names, pid, {}, &pid_name::pid);
			return it != names.end() && it->pid == pid ? it->name : std::string();
		}

		const WCHAR* tcp_state(const DWORD state)
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

		void format_address(const int family, const void* address, const DWORD port,
		                    const DWORD scope_id, std::string& output)
		{
			WCHAR ip[INET6_ADDRSTRLEN] = {};
			if (!InetNtopW(family, address, ip, _countof(ip)))
				wcscpy_s(ip, L"?");
			const auto number = ntohs(static_cast<WORD>(port));
			if (family == AF_INET6 && scope_id != 0)
				output = utf8(std::format(L"[{}%{}]:{}", ip, scope_id, number));
			else if (family == AF_INET6)
				output = utf8(std::format(L"[{}]:{}", ip, number));
			else
				output = utf8(std::format(L"{}:{}", ip, number));
		}

		template <class table_type, class query>
		table_type* query_table(query&& fn)
		{
			ULONG size = 0;
			auto status = fn(nullptr, &size);
			table_type* table = nullptr;
			while (status == ERROR_INSUFFICIENT_BUFFER)
			{
				table = static_cast<table_type*>(HeapAlloc(GetProcessHeap(), 0, size));
				if (!table) return nullptr;
				status = fn(table, &size);
				if (status == ERROR_INSUFFICIENT_BUFFER)
				{
					HeapFree(GetProcessHeap(), 0, table);
					table = nullptr;
				}
			}
			if (status != NO_ERROR && table)
			{
				HeapFree(GetProcessHeap(), 0, table);
				table = nullptr;
			}
			return table;
		}

		void free_table(void* table)
		{
			if (table) HeapFree(GetProcessHeap(), 0, table);
		}

		bool launch(const std::wstring_view verb, const std::wstring_view file,
		            const std::wstring_view parameters, std::string& error)
		{
			const auto result = reinterpret_cast<intptr_t>(
				ShellExecuteW(nullptr, verb.empty() ? nullptr : std::wstring(verb).c_str(),
				              std::wstring(file).c_str(),
				              parameters.empty() ? nullptr : std::wstring(parameters).c_str(),
				              nullptr, SW_SHOWNORMAL));
			if (result > 32) return true;
			error = "The requested action could not be started.";
			return false;
		}
	}

	struct performance_monitor::implementation
	{
		PDH_HQUERY query = nullptr;
		PDH_HCOUNTER cpu = nullptr;
		PDH_HCOUNTER disk = nullptr;
		PDH_HCOUNTER network = nullptr;
		PDH_HCOUNTER gpu = nullptr;
		PDH_HCOUNTER handles = nullptr;
		hardware_info hardware;
		bool initialized = false;

		~implementation()
		{
			if (query) PdhCloseQuery(query);
		}

		static bool add_counter(const PDH_HQUERY query, const WCHAR* path, PDH_HCOUNTER& target)
		{
			if (PdhAddEnglishCounterW(query, path, 0, &target) == ERROR_SUCCESS)
				return true;
			target = nullptr;
			return false;
		}

		void collect_hardware()
		{
			HKEY key = nullptr;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
			                  L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			                  0, KEY_READ, &key) == ERROR_SUCCESS)
			{
				WCHAR name[256] = {};
				DWORD size = sizeof(name);
				if (RegQueryValueExW(key, L"ProcessorNameString", nullptr, nullptr,
				                     reinterpret_cast<LPBYTE>(name), &size) == ERROR_SUCCESS)
				{
					hardware.cpu_name = utf8(name);
					const auto first = hardware.cpu_name.find_first_not_of(" \t");
					const auto last = hardware.cpu_name.find_last_not_of(" \t");
					if (first != std::string::npos)
						hardware.cpu_name = hardware.cpu_name.substr(first, last - first + 1);
				}

				DWORD mhz = 0;
				size = sizeof(mhz);
				if (RegQueryValueExW(key, L"~MHz", nullptr, nullptr,
				                     reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS)
					hardware.cpu_mhz = mhz;
				RegCloseKey(key);
			}

			SYSTEM_INFO system = {};
			GetSystemInfo(&system);
			hardware.logical_cores = system.dwNumberOfProcessors;

			DISPLAY_DEVICEW display = {sizeof(display)};
			for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &display, 0); ++index)
			{
				if (display.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
				{
					hardware.gpu_name = utf8(display.DeviceString);
					break;
				}
				display.cb = sizeof(display);
			}
			if (hardware.gpu_name.empty())
			{
				display.cb = sizeof(display);
				if (EnumDisplayDevicesW(nullptr, 0, &display, 0))
					hardware.gpu_name = utf8(display.DeviceString);
			}

			WCHAR system_directory[MAX_PATH] = {};
			if (GetSystemDirectoryW(system_directory, _countof(system_directory)) > 0)
				hardware.system_drive = static_cast<char>(system_directory[0]);
			WCHAR root[] = {static_cast<WCHAR>(hardware.system_drive), L':', L'\\', L'\0'};
			ULARGE_INTEGER free_available = {}, total = {}, total_free = {};
			if (GetDiskFreeSpaceExW(root, &free_available, &total, &total_free))
			{
				hardware.disk_total_bytes = total.QuadPart;
				hardware.disk_free_bytes = total_free.QuadPart;
			}
		}

		bool initialize()
		{
			if (initialized) return true;
			collect_hardware();
			if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS)
			{
				add_counter(query, L"\\Processor(_Total)\\% Processor Time", cpu);
				add_counter(query, L"\\PhysicalDisk(_Total)\\% Disk Time", disk);
				add_counter(query, L"\\Network Interface(*)\\Bytes Total/sec", network);
				add_counter(query, L"\\Process(_Total)\\Handle Count", handles);
				add_counter(query, L"\\GPU Engine(*)\\Utilization Percentage", gpu);
				PdhCollectQueryData(query);
			}
			initialized = true;
			return query != nullptr;
		}

		double read_counter(const PDH_HCOUNTER counter) const
		{
			PDH_FMT_COUNTERVALUE value = {};
			return counter &&
				PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS &&
				value.CStatus == PDH_CSTATUS_VALID_DATA ? value.doubleValue : 0.0;
		}

		DWORD read_array(const PDH_HCOUNTER counter, std::vector<BYTE>& buffer) const
		{
			if (!counter) return 0;
			DWORD size = 0;
			DWORD count = 0;
			if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, nullptr) !=
				PDH_MORE_DATA || size == 0)
				return 0;
			buffer.resize(size);
			if (PdhGetFormattedCounterArrayW(
				    counter, PDH_FMT_DOUBLE, &size, &count,
				    reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data())) != ERROR_SUCCESS)
				return 0;
			return count;
		}

		double read_network(std::vector<BYTE>& buffer) const
		{
			double result = 0.0;
			const auto count = read_array(network, buffer);
			const auto* items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
			for (DWORD i = 0; i < count; ++i)
				if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA)
					result += items[i].FmtValue.doubleValue;
			return result;
		}

		double read_gpu(std::vector<BYTE>& buffer, bool& available) const
		{
			const auto count = read_array(gpu, buffer);
			available = gpu != nullptr && count > 0;
			if (count == 0) return 0.0;

			const auto* items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
			std::map<std::wstring, double> by_engine;
			for (DWORD i = 0; i < count; ++i)
			{
				if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA) continue;
				const auto name = items[i].szName ? std::wstring(items[i].szName) : std::wstring();
				const auto marker = name.find(L"engtype_");
				const auto engine = marker == std::wstring::npos ? std::wstring() : name.substr(marker + 8);
				by_engine[engine] += items[i].FmtValue.doubleValue;
			}

			double busiest = 0.0;
			for (const auto& [unused, value] : by_engine)
			{
				(void)unused;
				busiest = std::max(busiest, value);
			}
			return std::clamp(busiest, 0.0, 100.0);
		}

		performance_sample sample()
		{
			if (!initialized) initialize();
			if (query) PdhCollectQueryData(query);

			performance_sample result;
			result.cpu_percent = read_counter(cpu);
			result.disk_percent = read_counter(disk);
			result.handle_count = static_cast<uint64_t>(std::max(0.0, read_counter(handles)));
			result.network_bytes_per_second = read_network(_array_buffer);
			result.gpu_percent = read_gpu(_array_buffer, result.gpu_available);

			MEMORYSTATUSEX memory = {sizeof(memory)};
			if (GlobalMemoryStatusEx(&memory))
			{
				result.memory_load = memory.dwMemoryLoad;
				result.memory_total_bytes = memory.ullTotalPhys;
				result.memory_available_bytes = memory.ullAvailPhys;
			}

			WCHAR root[] = {static_cast<WCHAR>(hardware.system_drive), L':', L'\\', L'\0'};
			ULARGE_INTEGER free_available = {}, total = {}, total_free = {};
			if (GetDiskFreeSpaceExW(root, &free_available, &total, &total_free))
			{
				result.disk_total_bytes = total.QuadPart;
				result.disk_free_bytes = total_free.QuadPart;
				hardware.disk_total_bytes = result.disk_total_bytes;
				hardware.disk_free_bytes = result.disk_free_bytes;
			}
			return result;
		}

	private:
		std::vector<BYTE> _array_buffer;
	};

	performance_monitor::performance_monitor()
		: _impl(std::make_unique<implementation>())
	{
	}

	performance_monitor::~performance_monitor() = default;
	performance_monitor::performance_monitor(performance_monitor&&) noexcept = default;
	performance_monitor& performance_monitor::operator=(performance_monitor&&) noexcept = default;

	bool performance_monitor::initialize()
	{
		return _impl->initialize();
	}

	const hardware_info& performance_monitor::hardware() const
	{
		return _impl->hardware;
	}

	performance_sample performance_monitor::sample()
	{
		return _impl->sample();
	}

	std::vector<process_info> snapshot_processes()
	{
		std::vector<process_info> result;
		const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE) return result;

		PROCESSENTRY32W entry = {sizeof(entry)};
		if (Process32FirstW(snapshot, &entry))
		{
			do
			{
				if (entry.th32ProcessID == 0) continue;

				process_info process;
				process.pid = entry.th32ProcessID;
				process.name = utf8(entry.szExeFile);

				const HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid);
				if (handle)
				{
					PROCESS_MEMORY_COUNTERS counters = {};
					if (GetProcessMemoryInfo(handle, &counters, sizeof(counters)))
						process.working_set = counters.WorkingSetSize;

					WCHAR path[MAX_PATH] = {};
					DWORD length = _countof(path);
					if (QueryFullProcessImageNameW(handle, 0, path, &length))
						process.path = utf8(std::wstring_view(path, length));
					process.created = process_creation_time(handle);
					CloseHandle(handle);
				}
				result.push_back(std::move(process));
			}
			while (Process32NextW(snapshot, &entry));
		}
		CloseHandle(snapshot);

		std::ranges::sort(result, [](const process_info& left, const process_info& right)
		{
			const auto name_order = folded_compare(left.name, right.name);
			if (name_order != 0) return name_order < 0;
			return left.pid < right.pid;
		});
		return result;
	}

	std::vector<module_info> snapshot_modules(const uint32_t pid)
	{
		std::vector<module_info> result;
		const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
		if (snapshot == INVALID_HANDLE_VALUE) return result;

		MODULEENTRY32W entry = {sizeof(entry)};
		if (Module32FirstW(snapshot, &entry))
		{
			do
			{
				result.push_back({utf8(entry.szModule), utf8(entry.szExePath), entry.modBaseSize});
			}
			while (Module32NextW(snapshot, &entry));
		}
		CloseHandle(snapshot);
		return result;
	}

	std::vector<network_connection> snapshot_network()
	{
		const auto names = process_names();
		std::vector<network_connection> result;

		auto add = [&](const std::string_view protocol, const std::string& local,
		               const std::string& remote, const std::string_view state,
		               const uint32_t pid)
		{
			result.push_back({std::string(protocol), local, remote, std::string(state),
			                  pid, process_name(names, pid)});
		};

		auto* tcp4 = query_table<MIB_TCPTABLE_OWNER_PID>(
			[](void* buffer, ULONG* size)
			{
				return GetExtendedTcpTable(buffer, size, TRUE, AF_INET,
				                           TCP_TABLE_OWNER_PID_ALL, 0);
			});
		if (tcp4)
		{
			for (DWORD i = 0; i < tcp4->dwNumEntries; ++i)
			{
				const auto& row = tcp4->table[i];
				std::string local, remote;
				format_address(AF_INET, &row.dwLocalAddr, row.dwLocalPort, 0, local);
				format_address(AF_INET, &row.dwRemoteAddr, row.dwRemotePort, 0, remote);
				add("TCP", local, remote, utf8(tcp_state(row.dwState)), row.dwOwningPid);
			}
		}
		free_table(tcp4);

		auto* tcp6 = query_table<MIB_TCP6TABLE_OWNER_PID>(
			[](void* buffer, ULONG* size)
			{
				return GetExtendedTcpTable(buffer, size, TRUE, AF_INET6,
				                           TCP_TABLE_OWNER_PID_ALL, 0);
			});
		if (tcp6)
		{
			for (DWORD i = 0; i < tcp6->dwNumEntries; ++i)
			{
				const auto& row = tcp6->table[i];
				std::string local, remote;
				format_address(AF_INET6, row.ucLocalAddr, row.dwLocalPort,
				               row.dwLocalScopeId, local);
				format_address(AF_INET6, row.ucRemoteAddr, row.dwRemotePort,
				               row.dwRemoteScopeId, remote);
				add("TCPv6", local, remote, utf8(tcp_state(row.dwState)), row.dwOwningPid);
			}
		}
		free_table(tcp6);

		auto* udp4 = query_table<MIB_UDPTABLE_OWNER_PID>(
			[](void* buffer, ULONG* size)
			{
				return GetExtendedUdpTable(buffer, size, TRUE, AF_INET,
				                           UDP_TABLE_OWNER_PID, 0);
			});
		if (udp4)
		{
			for (DWORD i = 0; i < udp4->dwNumEntries; ++i)
			{
				const auto& row = udp4->table[i];
				std::string local;
				format_address(AF_INET, &row.dwLocalAddr, row.dwLocalPort, 0, local);
				add("UDP", local, "*:*", {}, row.dwOwningPid);
			}
		}
		free_table(udp4);

		auto* udp6 = query_table<MIB_UDP6TABLE_OWNER_PID>(
			[](void* buffer, ULONG* size)
			{
				return GetExtendedUdpTable(buffer, size, TRUE, AF_INET6,
				                           UDP_TABLE_OWNER_PID, 0);
			});
		if (udp6)
		{
			for (DWORD i = 0; i < udp6->dwNumEntries; ++i)
			{
				const auto& row = udp6->table[i];
				std::string local;
				format_address(AF_INET6, row.ucLocalAddr, row.dwLocalPort,
				               row.dwLocalScopeId, local);
				add("UDPv6", local, "*:*", {}, row.dwOwningPid);
			}
		}
		free_table(udp6);

		return result;
	}

	namespace
	{
		BOOL CALLBACK enum_windows_callback(const HWND hwnd, const LPARAM parameter)
		{
			if (!IsWindowVisible(hwnd)) return TRUE;
			auto* windows = reinterpret_cast<std::vector<window_info>*>(parameter);

			window_info window;
			window.window_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd));
			WCHAR text[512] = {};
			copy_text(hwnd, text, _countof(text));
			window.title = utf8(text);
			copy_text(hwnd, text, _countof(text));
			GetClassNameW(hwnd, text, _countof(text));
			window.class_name = utf8(text);
			DWORD pid = 0;
			window.tid = GetWindowThreadProcessId(hwnd, &pid);
			window.pid = pid;
			RECT bounds = {};
			GetWindowRect(hwnd, &bounds);
			window.bounds = {bounds.left, bounds.top, bounds.right, bounds.bottom};
			window.style = static_cast<uint32_t>(GetWindowLongW(hwnd, GWL_STYLE));
			window.extended_style = static_cast<uint32_t>(GetWindowLongW(hwnd, GWL_EXSTYLE));
			windows->push_back(std::move(window));
			return TRUE;
		}
	}

	std::vector<window_info> snapshot_windows()
	{
		std::vector<window_info> result;
		EnumWindows(enum_windows_callback, reinterpret_cast<LPARAM>(&result));
		return result;
	}

	bool show_process_in_explorer(const process_info& process, std::string& error)
	{
		if (process.path.empty())
		{
			error = "The process path is unavailable.";
			return false;
		}
		const auto path = wide(process.path);
		const auto parameters = std::format(L"/select,\"{}\"", path);
		return launch(L"open", L"explorer.exe", parameters, error);
	}

	bool terminate_process(const process_info& process, std::string& error)
	{
		const HANDLE handle = OpenProcess(
			PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid);
		if (!handle)
		{
			error = GetLastError() == ERROR_INVALID_PARAMETER
				        ? "The process has already exited."
				        : "Could not open the process. Access may be denied.";
			return false;
		}

		DWORD exit_code = 0;
		const bool running = GetExitCodeProcess(handle, &exit_code) && exit_code == STILL_ACTIVE;
		const bool same_process = process.created != 0 &&
			process_creation_time(handle) == process.created;
		if (!running || !same_process)
		{
			CloseHandle(handle);
			error = "The process has exited since the list was refreshed.";
			return false;
		}

		const bool terminated = TerminateProcess(handle, 1) != FALSE;
		CloseHandle(handle);
		if (!terminated) error = "Failed to terminate the process.";
		return terminated;
	}

	bool show_window_process_in_explorer(const window_info& window, std::string& error)
	{
		const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, window.pid);
		if (!process)
		{
			error = "Could not open the owning process. Access may be denied.";
			return false;
		}
		WCHAR path[MAX_PATH] = {};
		DWORD length = _countof(path);
		const bool read = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
		CloseHandle(process);
		if (!read)
		{
			error = "The owning process path is unavailable.";
			return false;
		}
		return launch(L"open", L"explorer.exe",
		              std::format(L"/select,\"{}\"", std::wstring_view(path, length)), error);
	}

	bool close_window(const window_info& window, std::string& error)
	{
		const auto hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window.window_id));
		DWORD pid = 0;
		if (!IsWindow(hwnd) || GetWindowThreadProcessId(hwnd, &pid) != window.tid ||
			pid != window.pid)
		{
			error = "The window has closed since the list was refreshed.";
			return false;
		}
		if (!PostMessageW(hwnd, WM_CLOSE, 0, 0))
		{
			error = "The close request could not be posted.";
			return false;
		}
		return true;
	}

	bool open_issue_url(std::string& error)
	{
		return launch(L"open", L"https://github.com/ZacWalk/sys-mate/issues", {}, error);
	}

	std::vector<std::pair<std::string, std::string>>
	collect_system_summary(const hardware_info& hardware)
	{
		std::vector<std::pair<std::string, std::string>> rows;
		WCHAR buffer[256] = {};
		DWORD length = _countof(buffer);
		if (GetComputerNameW(buffer, &length))
			rows.emplace_back("Computer Name", utf8(std::wstring_view(buffer, length)));
		length = _countof(buffer);
		if (GetUserNameW(buffer, &length))
			rows.emplace_back("User Name", utf8(std::wstring_view(buffer, length)));

		OSVERSIONINFOW version = {sizeof(version)};
		if (const auto ntdll = GetModuleHandleW(L"ntdll.dll"))
		{
			using rtl_get_version = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
			const auto get_version = reinterpret_cast<rtl_get_version>(
				GetProcAddress(ntdll, "RtlGetVersion"));
			if (get_version) get_version(&version);
		}
		rows.emplace_back("OS Version", std::format("{}.{}.{}",
			version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber));

		SYSTEM_INFO system = {};
		GetNativeSystemInfo(&system);
		const char* architecture = "Unknown";
		switch (system.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_AMD64: architecture = "x64"; break;
		case PROCESSOR_ARCHITECTURE_ARM64: architecture = "ARM64"; break;
		case PROCESSOR_ARCHITECTURE_INTEL: architecture = "x86"; break;
		default: break;
		}
		rows.emplace_back("Architecture", architecture);
		if (!hardware.cpu_name.empty()) rows.emplace_back("Processor", hardware.cpu_name);
		rows.emplace_back("Logical Cores", format_u32(hardware.logical_cores));
		if (hardware.cpu_mhz > 0)
			rows.emplace_back("CPU Speed", std::format("{:.2f} GHz", hardware.cpu_mhz / 1000.0));
		if (!hardware.gpu_name.empty()) rows.emplace_back("Graphics", hardware.gpu_name);

		MEMORYSTATUSEX memory = {sizeof(memory)};
		if (GlobalMemoryStatusEx(&memory))
		{
			rows.emplace_back("Total RAM", format_bytes(memory.ullTotalPhys));
			rows.emplace_back("Available RAM", format_bytes(memory.ullAvailPhys));
			rows.emplace_back("Memory Load", std::format("{}%", memory.dwMemoryLoad));
		}
		if (hardware.disk_total_bytes > 0)
		{
			rows.emplace_back(std::format("Disk {}: Total", hardware.system_drive),
			                  format_bytes(hardware.disk_total_bytes));
			rows.emplace_back(std::format("Disk {}: Free", hardware.system_drive),
			                  format_bytes(hardware.disk_free_bytes));
		}

		const auto uptime = GetTickCount64();
		const auto hours = uptime / 3600000;
		const auto minutes = uptime % 3600000 / 60000;
		rows.emplace_back("System Uptime",
		                  std::format("{} hours, {} minutes", hours, minutes));
		return rows;
	}
}
