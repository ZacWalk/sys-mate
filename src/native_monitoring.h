#pragma once

// SysMate's only OS-facing surface.  The UI consumes these UTF-8 records and
// never includes a Windows SDK header or holds a native window handle.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sysmate::native
{
	struct rect
	{
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
	};

	struct hardware_info
	{
		std::string cpu_name;
		uint32_t logical_cores = 0;
		uint32_t cpu_mhz = 0;
		std::string gpu_name;
		char system_drive = 'C';
		uint64_t disk_total_bytes = 0;
		uint64_t disk_free_bytes = 0;
	};

	struct performance_sample
	{
		double cpu_percent = 0.0;
		uint32_t memory_load = 0;
		uint64_t memory_total_bytes = 0;
		uint64_t memory_available_bytes = 0;
		double disk_percent = 0.0;
		double network_bytes_per_second = 0.0;
		double gpu_percent = 0.0;
		bool gpu_available = false;
		uint64_t handle_count = 0;
		uint64_t disk_total_bytes = 0;
		uint64_t disk_free_bytes = 0;
	};

	class performance_monitor
	{
		struct implementation;
		std::unique_ptr<implementation> _impl;

	public:
		performance_monitor();
		~performance_monitor();
		performance_monitor(performance_monitor&&) noexcept;
		performance_monitor& operator=(performance_monitor&&) noexcept;

		performance_monitor(const performance_monitor&) = delete;
		performance_monitor& operator=(const performance_monitor&) = delete;

		bool initialize();
		[[nodiscard]] const hardware_info& hardware() const;
		[[nodiscard]] performance_sample sample();
	};

	struct process_info
	{
		uint32_t pid = 0;
		uint64_t created = 0;
		std::string name;
		std::string path;
		uint64_t working_set = 0;
	};

	struct module_info
	{
		std::string name;
		std::string path;
		uint64_t size = 0;
	};

	struct network_connection
	{
		std::string protocol;
		std::string local_address;
		std::string remote_address;
		std::string state;
		uint32_t pid = 0;
		std::string process_name;
	};

	struct window_info
	{
		uint64_t window_id = 0;
		std::string title;
		std::string class_name;
		uint32_t pid = 0;
		uint32_t tid = 0;
		rect bounds;
		uint32_t style = 0;
		uint32_t extended_style = 0;
	};

	[[nodiscard]] std::vector<process_info> snapshot_processes();
	[[nodiscard]] std::vector<module_info> snapshot_modules(uint32_t pid);
	[[nodiscard]] std::vector<network_connection> snapshot_network();
	[[nodiscard]] std::vector<window_info> snapshot_windows();

	bool show_process_in_explorer(const process_info& process, std::string& error);
	bool terminate_process(const process_info& process, std::string& error);
	bool show_window_process_in_explorer(const window_info& window, std::string& error);
	bool close_window(const window_info& window, std::string& error);
	bool open_issue_url(std::string& error);

	[[nodiscard]] std::vector<std::pair<std::string, std::string>>
	collect_system_summary(const hardware_info& hardware);
}
