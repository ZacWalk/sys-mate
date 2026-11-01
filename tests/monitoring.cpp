#include "native_monitoring.h"

#include <cstdio>

int main()
{
	int failures = 0;
	const auto check = [&failures](const bool condition, const char* text)
	{
		if (!condition)
		{
			++failures;
			std::printf("FAIL: %s\n", text);
		}
	};

	sysmate::native::performance_monitor monitor;
	monitor.initialize();
	check(monitor.hardware().logical_cores > 0, "hardware reports logical cores");
	(void)monitor.sample();

	const auto processes = sysmate::native::snapshot_processes();
	check(!processes.empty(), "process snapshot is available");
	for (const auto& process : processes)
	{
		check(process.pid != 0, "process identities exclude the idle pseudo-process");
		if (process.pid != 0) break;
	}

	const auto windows = sysmate::native::snapshot_windows();
	for (const auto& window : windows)
	{
		check(window.window_id != 0, "window identities are nonzero");
		check(window.pid != 0, "window owners are reported");
		break;
	}

	std::printf("monitoring: %s (%zu processes, %zu windows)\n",
	            failures == 0 ? "all checks passed" : "FAILED",
	            processes.size(), windows.size());
	return failures == 0 ? 0 : 1;
}
