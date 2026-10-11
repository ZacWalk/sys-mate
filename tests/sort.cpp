// Checks the list sort order. std::sort needs a strict weak ordering; a column whose
// cells broke it (a process named "nan-..." next to one named "7-...") made the
// Debug build assert "invalid comparator" and abort.
#define NOMINMAX  // as main.cpp does, before any Windows header
#define WIN32_LEAN_AND_MEAN
#include "helpers.h"

#include <algorithm>
#include <crtdbg.h>
#include <cstdio>
#include <string>
#include <vector>

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

	bool Less(const std::wstring& a, const std::wstring& b) { return Sort::CompareCells(a, b) < 0; }
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
	Check(Less(L"9", L"10"), "9 < 10");
	Check(Less(L"5.7 MB", L"27.0 MB"), "5.7 MB < 27.0 MB");
	Check(Less(L"512 KB", L"2460 KB"), "512 KB < 2460 KB");
	Check(Sort::CompareCells(L"agency.exe", L"AGENCY.EXE") == 0, "text ignores case");

	// Names that wcstod reads as NaN or infinity are text, and the order is antisymmetric.
	Check(Less(L"7-dummy.exe", L"nan-dummy.exe"), "7-dummy.exe < nan-dummy.exe");
	Check(!Less(L"nan-dummy.exe", L"7-dummy.exe"), "not nan-dummy.exe < 7-dummy.exe");
	Check(Less(L"7-dummy.exe", L"inf.exe") && !Less(L"inf.exe", L"7-dummy.exe"), "inf.exe is text");

	const std::vector<std::wstring> cells = {
		L"nan-dummy.exe", L"NaN", L"7-dummy.exe", L"inf.exe", L"-inf", L"1e999", L"0x1A", L"",
		L"agency.exe", L"Agency.exe", L"10", L"9", L"+5", L"-1", L",", L"1 \u00D7 1", L"0, 0, 1, 1",
		L"0.0.0.0:135", L"[::]:135", L"27.0 MB", L"5.7 MB", L"ThumbnailDeviceHelperWnd",
		L"0000000000060064", L"0x08280088", L"LISTENING", L"TCPv6"};

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
	std::vector<std::wstring> sorted = cells;
	std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(a, b); });
	Check(std::is_sorted(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(a, b); }),
	      "ascending sort is ordered");
	std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(b, a); });
	Check(std::is_sorted(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return Less(b, a); }),
	      "descending sort is ordered");

	std::printf("%s\n", g_failures == 0 ? "sort: all checks passed" : "sort: FAILED");
	return g_failures == 0 ? 0 : 1;
}
