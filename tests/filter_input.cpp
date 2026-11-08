#include "libdesktop/ui/controls.h"
#include "libdesktop/ui/test_support.h"

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
	ui::test::fake_control_host host;
	ui::edit_styles styles;
	auto edit = ui::make_edit(styles, {}, [](const std::string&) {});
	auto input = std::make_shared<ui::input_manager>(&host);
	edit->found(input, {}, true);
	edit->layout(host.measure_context, {0, 0, 320, 30});
	edit->focus();
	input->keyboard_focus(true);

	input->text_input("a");
	input->text_input("\xC3\xA9");
	input->text_input("\xE7\x8C\xAB");
	check(edit->window_text() == "a\xC3\xA9\xE7\x8C\xAB", "UTF-8 char input is preserved");

	input->key_down(platform::keys::LEFT, {false, true, false});
	check(input->key_down(platform::keys::BACK, {}), "selection backspace is handled");
	check(edit->window_text() == "a\xC3\xA9", "selection deletion keeps UTF-8 valid");

	input->key_down('A', {true, false, false});
	input->key_down('C', {true, false, false});
	const auto clipboard = platform::clipboard_text();
	edit->window_text({});
	input->key_down('V', {true, false, false});
	check(!clipboard || edit->window_text() == "a\xC3\xA9", "clipboard paste preserves UTF-8");

	std::printf("filter_input: %s\n", failures == 0 ? "all checks passed" : "FAILED");
	return failures == 0 ? 0 : 1;
}

