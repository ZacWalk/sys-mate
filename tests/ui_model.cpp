#include "ui_model.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{
	int failures = 0;

	void check(const bool condition, const char* description)
	{
		if (!condition)
		{
			++failures;
			std::printf("FAIL: %s\n", description);
		}
	}
}

int main()
{
	using namespace sysmate::ui;

	const auto process_layout = make_layout({1000, 650}, 1.0, tab_id::processes);
	check(process_layout.tabs.height() == 30, "tab height");
	check(process_layout.bottom_bar.height() == 34, "bottom bar height");
	check(process_layout.page.top == 30 && process_layout.page.bottom == 616, "page bounds");
	check(process_layout.filter_visible, "filter visible for process tab");
	check(process_layout.buttons.size() == 3, "process action count");
	check(tab_at(process_layout, {100, 10}) == 0, "first tab hit");
	check(tab_at(process_layout, {999, 10}) == 4, "last tab hit");
	check(button_at(process_layout, process_layout.buttons[1].contains({0, 0})
		                                 ? point{} : point{process_layout.buttons[1].left + 1,
		                                                   process_layout.buttons[1].top + 1}) == 1,
	      "button hit");

	check(compare_cells("9", "10") < 0, "numeric sort");
	check(compare_cells("5.7 MB", "27.0 MB") < 0, "numeric suffix sort");
	check(compare_cells("7-dummy.exe", "nan-dummy.exe") < 0, "non-finite names are text");
	check(compare_cells("agency.exe", "AGENCY.EXE") == 0, "case-insensitive text sort");

	table_model model;
	auto first = std::make_shared<int>(1);
	auto second = std::make_shared<int>(2);
	model.set_rows({
		{10, {"bravo.exe", "20"}, first},
		{20, {"alpha.exe", "5"}, second},
	});
	model.select_id(10);
	model.sort_by(0, true);
	check(model.rows()[0].id == 20, "sort changes row order");
	check(model.selected_id() == 10, "sort preserves selected identity");
	model.set_filter("alpha");
	check(model.rows().size() == 1 && model.rows()[0].id == 20, "filter matches any row cell");
	check(model.selected_id() == 0, "filter clears hidden selection");
	model.set_filter({});
	model.select_id(20);
	model.move_selection(1);
	check(model.selected_id() == 10, "keyboard navigation follows visible order");

	focus_model focus;
	focus.advance(false, true);
	check(focus.target == focus_target::filter, "tab focuses filter");
	focus.advance(false, true);
	check(focus.target == focus_target::table, "tab focuses table");
	focus.advance(true, true);
	check(focus.target == focus_target::filter, "reverse tab returns to filter");

	header_drag_state drag;
	drag.begin(2, 100);
	check(drag.active() && drag.move(112) == 12, "header drag tracks delta");
	drag.capture_changed();
	check(!drag.active() && drag.move(120) == 0, "capture loss cancels header drag");

	std::printf("ui_model: %s\n", failures == 0 ? "all checks passed" : "FAILED");
	return failures == 0 ? 0 : 1;
}
