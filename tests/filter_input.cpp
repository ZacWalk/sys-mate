#include "ui/view_host.h"
#include "ui/test_support.h"
#include "ui/widgets.h"

#include <cstdio>
#include <memory>

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
	auto window = std::make_shared<pf::ui::test::fake_window_frame>();
	window->set_focus();

	pf::ui::edit_box_widget input;
	check(input.on_char(window, U'a'), "ASCII input");
	check(input.on_char(window, U'\u00e9'), "two-byte UTF-8 input");
	check(input.on_char(window, U'\u732b'), "three-byte UTF-8 input");
	check(input.edit.text == "a\xc3\xa9\xe7\x8c\xab", "UTF-8 text is preserved");
	check(input.edit.cursor_pos == static_cast<int>(input.edit.text.size()),
	      "caret stays at UTF-8 byte end");

	window->held_keys.insert(pf::platform_key::Shift);
	bool modified = false;
	check(input.on_key_down(window, pf::platform_key::Left, modified), "left navigation");
	check(input.edit.has_selection(), "shift navigation selects a code point");
	check(input.edit.selected_text() == "\xe7\x8c\xab", "selection stays on code-point boundary");
	window->held_keys.clear();

	check(input.on_char(window, U'\b'), "backspace replaces the selection");
	check(input.edit.text == "a\xc3\xa9", "selection deletion keeps UTF-8 valid");

	input.edit.select_all();
	if (input.edit.copy_to_clipboard())
	{
		pf::ui::edit_box_widget pasted;
		check(pasted.edit.paste_from_clipboard(), "clipboard paste");
		check(pasted.edit.text == "a\xc3\xa9", "clipboard paste preserves UTF-8");
	}
	else
	{
		std::printf("clipboard unavailable: skipping clipboard round trip\n");
	}

	std::printf("filter_input: %s\n", failures == 0 ? "all checks passed" : "FAILED");
	return failures == 0 ? 0 : 1;
}

app_init_result app_init(const pf::window_frame_ptr&,
                         const std::span<const std::string_view>)
{
	return {.start_gui = false};
}

void app_idle()
{
}

void app_destroy()
{
}
