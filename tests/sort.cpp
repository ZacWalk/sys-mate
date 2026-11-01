// Checks the list sort order. std::sort needs a strict weak ordering; a column whose
// cells broke it (a process named "nan-..." next to one named "7-...") made the
// Debug build assert "invalid comparator" and abort.
#include <algorithm>
#include <crtdbg.h>
#include <cstdio>
#include <string>
#include <vector>
#include "ui_model.h"

namespace
{
	int g_failures = 0;

	void Check(const bool ok, const char* what)
	{
		if (!ok)
		{
			++g_failures;
			std::printf("FAIL: %s\n", what);
		}
	}

	bool Less(const std::string& a, const std::string& b)
	{
		return sysmate::ui::compare_cells(a, b) < 0;
	}
}

int main()
{
	// A failed Debug STL check (an invalid comparator, say) goes to stderr and fails
	// the test, rather than waiting on a dialog nobody will answer.
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);

	// Magnitude, not text, for the numbers the lists show.
	Check(Less("9", "10"), "9 < 10");
	Check(Less("5.7 MB", "27.0 MB"), "5.7 MB < 27.0 MB");
	Check(Less("512 KB", "2460 KB"), "512 KB < 2460 KB");
	Check(sysmate::ui::compare_cells("agency.exe", "AGENCY.EXE") == 0, "text ignores case");

	// Names that parse as NaN or infinity are text, and the order is antisymmetric.
	Check(Less("7-dummy.exe", "nan-dummy.exe"), "7-dummy.exe < nan-dummy.exe");
	Check(!Less("nan-dummy.exe", "7-dummy.exe"), "not nan-dummy.exe < 7-dummy.exe");
	Check(Less("7-dummy.exe", "inf.exe") && !Less("inf.exe", "7-dummy.exe"), "inf.exe is text");
	double huge = 0.0;
	Check(!sysmate::ui::leading_number("1e999", huge), "positive overflow is text");
	Check(!sysmate::ui::leading_number("-1e999", huge), "negative overflow is text");
	Check(!sysmate::ui::leading_number("1e999-dummy.exe", huge),
	      "overflow mixed text is text");
	Check(!sysmate::ui::leading_number("-1e999-dummy.exe", huge),
	      "negative overflow mixed text is text");

	const std::vector<std::string> cells = {
		"nan-dummy.exe", "NaN", "7-dummy.exe", "inf.exe", "-inf", "1e999", "-1e999",
		"1e999-dummy.exe", "-1e999-dummy.exe", "0x1A", "",
		"agency.exe", "Agency.exe", "10", "9", "+5", "-1", ",", "1 x 1", "0, 0, 1, 1",
		"0.0.0.0:135", "[::]:135", "27.0 MB", "5.7 MB", "ThumbnailDeviceHelperWnd",
		"0000000000060064", "0x08280088", "LISTENING", "TCPv6"};

	// Strict weak ordering over every pair and triple of the sample cells.
	for (const auto& a : cells)
	{
		Check(!Less(a, a), "irreflexive");
		for (const auto& b : cells)
		{
			if (Less(a, b)) Check(!Less(b, a), "asymmetric");
			for (const auto& c : cells)
			{
				if (Less(a, b) && Less(b, c)) Check(Less(a, c), "transitive");
				const bool abEqual = !Less(a, b) && !Less(b, a);
				const bool bcEqual = !Less(b, c) && !Less(c, b);
				if (abEqual && bcEqual) Check(!Less(a, c) && !Less(c, a), "equivalence is transitive");
			}
		}
	}

	// Sort both ways, as a column header does; the Debug STL checks the comparator too.
	std::vector<std::string> sorted = cells;
	std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(a, b); });
	Check(std::is_sorted(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(a, b); }),
	      "ascending sort is ordered");
	std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(b, a); });
	Check(std::is_sorted(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(b, a); }),
	      "descending sort is ordered");

	std::printf("%s\n", g_failures == 0 ? "sort: all checks passed" : "sort: FAILED");
	return g_failures == 0 ? 0 : 1;
}
