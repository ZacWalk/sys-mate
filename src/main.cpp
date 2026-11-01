// SysMate application shell.
//
// The platform boundary is deliberately visible: pf:: owns the window,
// reactor, message loop, menus, drawing, input and DPI; native_monitoring owns
// PDH and the Windows inspection APIs.  The code between them is a portable UI
// model and a set of pf reactors.

#include "platform.h"
#include "ui/view_list.h"
#include "ui/widgets.h"
#include "ui/test_support.h"

#include "chart.h"
#include "native_monitoring.h"
#include "ui_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	using sysmate::native::network_connection;
	using sysmate::native::process_info;
	using sysmate::native::window_info;
	using sysmate::ui::tab_id;

	constexpr int chart_count = 6;
	constexpr uint32_t performance_timer = 1;
	constexpr std::string_view tab_names[] = {
		"Performance", "Processes", "Network", "Windows", "About"};

	enum class chart_id
	{
		cpu,
		memory,
		disk,
		network,
		gpu,
		handles,
	};

	constexpr std::string_view chart_names[] = {
		"CPU", "Memory", "Disk", "Network", "GPU", "Handles"};

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

	std::string format_hex(const uint32_t value)
	{
		return std::format("0x{:08X}", value);
	}

	uint64_t hash_text(const std::string_view text)
	{
		uint64_t hash = 14695981039346656037ull;
		for (const auto byte : text)
		{
			hash ^= static_cast<unsigned char>(byte);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	uint64_t process_identity(const process_info& process)
	{
		return hash_text(std::format("{}:{}", process.pid, process.created));
	}

	uint64_t network_identity(const network_connection& connection)
	{
		return hash_text(std::format("{}|{}|{}|{}|{}|{}",
			connection.protocol, connection.local_address, connection.remote_address,
			connection.state, connection.pid, connection.process_name));
	}

	uint64_t window_identity(const window_info& window)
	{
		return window.window_id;
	}

	uint64_t module_identity(const std::string_view path, const uint64_t size)
	{
		return hash_text(std::format("{}:{}", path, size));
	}

	struct sysmate_theme final : pf::ui::theme
	{
		void set_dpi(const double scale)
		{
			dpi_scale = scale > 0.0 ? scale : 1.0;
			list_font_height = std::max(8, sysmate::ui::scale(13, dpi_scale));
			text_font_height = list_font_height;
			list_font = {list_font_height, pf::font_name::calibri};
			edit_font = {list_font_height, pf::font_name::calibri};
			text_font = {list_font_height, pf::font_name::consolas};
			padding_x = sysmate::ui::scale(6, dpi_scale);
			padding_y = sysmate::ui::scale(3, dpi_scale);
			indent = sysmate::ui::scale(16, dpi_scale);
			list_top_pad = sysmate::ui::scale(2, dpi_scale);
			list_scroll_pad = sysmate::ui::scale(4, dpi_scale);

			window_background = {30, 30, 30};
			tool_background = {42, 42, 46};
			text = {225, 225, 225};
			dim_text = {165, 165, 170};
			group_text = {225, 195, 80};
			line = {80, 120, 180};
			match_highlight = {0, 100, 180};
			header_background = {50, 50, 56};
			handle = {55, 55, 62};
			handle_hover = {70, 70, 78};
			handle_tracking = {0, 100, 180};
			focus_handle = {0, 120, 210};
		}
	};

	void draw_text(pf::draw_context& dc, const std::string_view text,
	               const pf::font& font, const pf::color_t color,
	               const pf::color_t background, const sysmate::ui::rect bounds)
	{
		if (text.empty() || bounds.width() <= 0 || bounds.height() <= 0) return;
		const auto size = dc.measure_text(text, font);
		auto clipped = bounds;
		clipped.right = std::min(clipped.right, clipped.left + size.cx);
		clipped.bottom = std::min(clipped.bottom, clipped.top + size.cy);
		if (clipped.width() <= 0 || clipped.height() <= 0) return;
		dc.draw_text(bounds.left, bounds.top, {bounds.left, bounds.top,
		                                      bounds.right, bounds.bottom},
		             text, font, color, background);
	}

	void draw_centered_text(pf::draw_context& dc, const std::string_view text,
	                        const pf::font& font, const pf::color_t color,
	                        const pf::color_t background, const sysmate::ui::rect bounds)
	{
		if (text.empty()) return;
		const auto size = dc.measure_text(text, font);
		const auto x = bounds.left + std::max(0, (bounds.width() - size.cx) / 2);
		const auto y = bounds.top + std::max(0, (bounds.height() - size.cy) / 2);
		dc.draw_text(x, y, {bounds.left, bounds.top, bounds.right, bounds.bottom},
		             text, font, color, background);
	}

	class table_view final : public pf::ui::list_view
	{
		struct row_tag
		{
			uint64_t id = 0;
			std::shared_ptr<void> payload;
		};

		sysmate::ui::table_model _model;
		std::string _empty_text = "Empty";
		pf::window_frame_ptr _window;
		sysmate::ui::header_drag_state _drag;
		std::function<void(std::shared_ptr<void>)> _on_selection;
		std::function<std::vector<pf::menu_command>(std::shared_ptr<void>)> _popup;
		std::function<void(bool)> _on_tab;

		void rebuild_items()
		{
			const auto selected = _model.selected_id();
			_items.clear();
			_items.reserve(_model.rows().size());

			for (const auto& model_row : _model.rows())
			{
				auto item = std::make_shared<pf::ui::list_item>();
				item->text = model_row.cells.empty() ? std::string() : model_row.cells.front();
				for (const auto& cell : model_row.cells)
					item->cells.emplace_back(cell);
				item->data = std::make_shared<row_tag>(row_tag{model_row.id, model_row.data});
				_items.push_back(std::move(item));
			}

			_selected_item.reset();
			_selected_index = -1;
			_anchor_index = -1;
			for (int i = 0; i < static_cast<int>(_items.size()); ++i)
			{
				const auto tag = std::static_pointer_cast<row_tag>(_items[i]->data);
				if (tag && tag->id == selected)
				{
					set_selected(i);
					break;
				}
			}

			layout_list();
		}

		void select_position(const pf::window_frame_ptr& window, int index)
		{
			if (_items.empty()) return;
			index = std::clamp(index, 0, static_cast<int>(_items.size()) - 1);
			set_selected(index);
			const auto tag = std::static_pointer_cast<row_tag>(_items[index]->data);
			if (tag)
			{
				_model.select_id(tag->id);
				if (_on_selection) _on_selection(tag->payload);
			}
			ensure_visible(window, _items[index]);
			window->invalidate();
		}

	public:
		table_view(sysmate_theme& theme, std::vector<pf::ui::list_column> columns)
			: pf::ui::list_view(theme)
		{
			set_columns(std::move(columns));
		}

		void set_rows(std::vector<sysmate::ui::row> rows)
		{
			_model.set_rows(std::move(rows));
			rebuild_items();
			// A refresh can happen before the first WM_SIZE.  The next size
			// message will lay out the rows again using real font metrics.
			if (_view_extent.cx > 0) layout_list();
			if (_window) _window->invalidate();
		}

		void set_filter(std::string filter)
		{
			_model.set_filter(std::move(filter));
			rebuild_items();
			if (_view_extent.cx > 0) layout_list();
			if (_window) _window->invalidate();
		}

		void set_empty_text(std::string text) { _empty_text = std::move(text); }

		[[nodiscard]] std::shared_ptr<void> selected_data() const
		{
			const auto* row = _model.selected_row();
			return row ? row->data : nullptr;
		}

		[[nodiscard]] uint64_t selected_id() const { return _model.selected_id(); }
		[[nodiscard]] const std::string& filter() const { return _model.filter(); }

		bool select_first_for_test()
		{
			if (!_window || _items.empty()) return false;
			select_position(_window, 0);
			return true;
		}

		[[nodiscard]] int row_count_for_test() const
		{
			return static_cast<int>(_items.size());
		}

		[[nodiscard]] std::shared_ptr<void> payload_for_test(const int index) const
		{
			if (index < 0 || index >= static_cast<int>(_items.size())) return nullptr;
			const auto tag = std::static_pointer_cast<row_tag>(_items[index]->data);
			return tag ? tag->payload : nullptr;
		}

		[[nodiscard]] bool header_drag_active_for_test() const
		{
			return _drag.active();
		}

		uint32_t handle_message(pf::window_frame_ptr window,
		                       const pf::message_type message,
		                       const pf::message_params& params) override
		{
			_window = window;
			if (message == pf::message_type::capture_changed)
			{
				if (_drag.active())
				{
					_drag.capture_changed();
					window->invalidate();
				}
				return 0;
			}
			return pf::ui::list_view::handle_message(window, message, params);
		}

		void set_on_selection(std::function<void(std::shared_ptr<void>)> callback)
		{
			_on_selection = std::move(callback);
		}

		void set_popup(std::function<std::vector<pf::menu_command>(std::shared_ptr<void>)> callback)
		{
			_popup = std::move(callback);
		}

		void set_on_tab(std::function<void(bool)> callback)
		{
			_on_tab = std::move(callback);
		}

		std::vector<pf::menu_command> default_popup_menu() override
		{
			if (_popup) return _popup(selected_data());
			return pf::ui::list_view::default_popup_menu();
		}

		uint32_t handle_mouse(pf::window_frame_ptr window,
		                      const pf::mouse_message_type message,
		                      const pf::mouse_params& params) override
		{
			const auto edge_column = [&](const int x)
			{
				const auto rects = column_rects(window->get_client_rect().width());
				const auto hit_zone = sysmate::ui::scale(4, _theme.dpi_scale);
				for (int i = 0; i + 1 < static_cast<int>(rects.size()); ++i)
					if (std::abs(x - rects[i].right) <= hit_zone) return i;
				return -1;
			};

			if (message == pf::mouse_message_type::set_cursor &&
				params.point.y < _header_height)
			{
				if (edge_column(params.point.x) >= 0)
				{
					window->set_cursor_shape(pf::cursor_shape::size_we);
					return 1;
				}
			}
			if (message == pf::mouse_message_type::left_button_down &&
				params.point.y < _header_height)
			{
				if (const auto column = edge_column(params.point.x); column >= 0)
				{
					_drag.begin(column, params.point.x);
					window->set_capture();
					return 1;
				}
			}
			if (message == pf::mouse_message_type::mouse_move && _drag.active())
			{
				const auto delta = _drag.move(params.point.x);
				if (delta != 0)
				{
					_columns[_drag.column].weight = std::max(
						sysmate::ui::scale(30, _theme.dpi_scale),
						_columns[_drag.column].weight + delta);
					window->invalidate();
				}
				return 1;
			}
			if (message == pf::mouse_message_type::left_button_up && _drag.active())
			{
				_drag.capture_changed();
				window->release_capture();
				return 1;
			}
			return pf::ui::list_view::handle_mouse(window, message, params);
		}

		void handle_paint(pf::window_frame_ptr& window, pf::draw_context& dc) override
		{
			pf::ui::list_view::handle_paint(window, dc);
			if (!_items.empty()) return;
			const auto client = window->get_client_rect();
			const auto size = dc.measure_text(_empty_text, _theme.list_font);
			const auto top = _header_height + std::max(0, (client.height() - _header_height - size.cy) / 2);
			dc.draw_text(std::max(0, (client.width() - size.cx) / 2), top,
			             {0, _header_height, client.right, client.bottom},
			             _empty_text, _theme.list_font, _theme.dim_text,
			             _theme.tool_background);
		}

		uint32_t handle_keyboard(pf::window_frame_ptr window,
		                         const pf::keyboard_message_type message,
		                         const pf::keyboard_params& params) override
		{
			if (message != pf::keyboard_message_type::key_down)
				return pf::ui::list_view::handle_keyboard(window, message, params);

			if (params.vk == pf::platform_key::Tab)
			{
				if (_on_tab) _on_tab(window->is_key_down(pf::platform_key::Shift));
				return 1;
			}

			if (params.vk == pf::platform_key::Home && !_items.empty())
			{
				select_position(window, 0);
				return 1;
			}
			if (params.vk == pf::platform_key::End && !_items.empty())
			{
				select_position(window, static_cast<int>(_items.size()) - 1);
				return 1;
			}
			if ((params.vk == pf::platform_key::Prior || params.vk == pf::platform_key::Next) &&
				!_items.empty())
			{
				const auto row_height = std::max(1, _font_extent.cy + _theme.padding_y * 2);
				const auto page = std::max(1, _view_extent.cy / row_height);
				const auto delta = params.vk == pf::platform_key::Prior ? -page : page;
				const auto current = selected_index();
				select_position(window, current < 0 ? (delta < 0 ? 0 : page) : current + delta);
				return 1;
			}

			return pf::ui::list_view::handle_keyboard(window, message, params);
		}

	protected:
		void on_sort_changed(const int column, const bool ascending) override
		{
			_model.sort_by(column, ascending);
			rebuild_items();
			if (_view_extent.cx > 0) layout_list();
		}

		void on_item_selected(const pf::window_frame_ptr&,
		                      const pf::ui::list_item_ptr& item, const bool) override
		{
			const auto tag = item ? std::static_pointer_cast<row_tag>(item->data) : nullptr;
			_model.select_id(tag ? tag->id : 0);
			if (_on_selection) _on_selection(tag ? tag->payload : nullptr);
		}
	};

	class filter_view final : public pf::frame_reactor
	{
		sysmate_theme& _theme;
		pf::ui::edit_box_widget _input;
		std::function<void(std::string_view)> _on_change;
		std::function<void(bool)> _on_tab;

		void changed(const bool modified)
		{
			if (modified && _on_change) _on_change(_input.edit.text);
		}

		void draw_segment(pf::draw_context& dc, const std::string_view text,
		                 const int x, const pf::color_t background,
		                 const pf::irect& bounds)
		{
			if (text.empty()) return;
			const auto size = dc.measure_text(text, _theme.edit_font);
			const auto right = std::min(bounds.right, x + size.cx);
			if (right <= x) return;
			dc.draw_text(x, bounds.top, {x, bounds.top, right, bounds.bottom},
			             text, _theme.edit_font, _theme.text, background);
		}

	public:
		explicit filter_view(sysmate_theme& theme) : _theme(theme)
		{
		}

		void set_on_change(std::function<void(std::string_view)> callback)
		{
			_on_change = std::move(callback);
		}

		void set_on_tab(std::function<void(bool)> callback)
		{
			_on_tab = std::move(callback);
		}

		void set_text(const std::string_view text)
		{
			_input.edit.text = std::string(text);
			_input.edit.cursor_pos = static_cast<int>(_input.edit.text.size());
			_input.edit.sel_anchor = _input.edit.cursor_pos;
		}

		[[nodiscard]] const std::string& text() const { return _input.edit.text; }

		[[nodiscard]] std::string selected_text_for_test() const
		{
			return std::string(_input.edit.selected_text());
		}

		void focus_select_all(const pf::window_frame_ptr& window)
		{
			window->set_focus();
			_input.edit.select_all();
			_input.reset_caret(window);
			window->invalidate();
		}

		uint32_t handle_message(pf::window_frame_ptr window,
		                       const pf::message_type message,
		                       const pf::message_params& params) override
		{
			if (message == pf::message_type::erase_background) return 1;
			if (message == pf::message_type::set_focus)
			{
				_input.update_focus(window, true);
				window->invalidate();
			}
			if (message == pf::message_type::kill_focus)
			{
				_input.update_focus(window, false);
				window->invalidate();
			}
			if (message == pf::message_type::timer && _input.on_timer(params.timer_id))
				window->invalidate();
			return 0;
		}

		uint32_t handle_mouse(pf::window_frame_ptr window,
		                      const pf::mouse_message_type message,
		                      const pf::mouse_params& params) override
		{
			if (message == pf::mouse_message_type::set_cursor)
			{
				window->set_cursor_shape(pf::cursor_shape::ibeam);
				return 1;
			}
			if (message != pf::mouse_message_type::left_button_down) return 0;
			window->set_focus();
			const auto measure = window->create_measure_context();
			if (measure)
			{
				const auto target_x = std::max(0, params.point.x -
					sysmate::ui::scale(6, _theme.dpi_scale));
				int best = 0;
				int position = 0;
				while (position < static_cast<int>(_input.edit.text.size()))
				{
					const auto next = pf::utf8_next(_input.edit.text, position);
					const auto width = measure->measure_text(
						std::string_view(_input.edit.text).substr(0, next), _theme.edit_font).cx;
					if (target_x < width)
					{
						const auto previous_width = measure->measure_text(
							std::string_view(_input.edit.text).substr(0, position),
							_theme.edit_font).cx;
						best = target_x - previous_width < width - target_x ? position : next;
						break;
					}
					best = next;
					position = next;
				}
				_input.edit.cursor_pos = best;
			}
			else
			{
				_input.edit.cursor_pos = static_cast<int>(_input.edit.text.size());
			}
			if (!window->is_key_down(pf::platform_key::Shift))
				_input.edit.sel_anchor = _input.edit.cursor_pos;
			_input.reset_caret(window);
			window->invalidate();
			return 1;
		}

		uint32_t handle_keyboard(pf::window_frame_ptr window,
		                         const pf::keyboard_message_type message,
		                         const pf::keyboard_params& params) override
		{
			if (message == pf::keyboard_message_type::char_input)
			{
				const auto modified = _input.on_char(window, params.ch);
				changed(modified);
				window->invalidate();
				return 1;
			}
			if (message != pf::keyboard_message_type::key_down) return 0;

			if (params.vk == pf::platform_key::Tab)
			{
				if (_on_tab) _on_tab(window->is_key_down(pf::platform_key::Shift));
				return 1;
			}

			if (window->is_key_down(pf::platform_key::Control))
			{
				switch (params.vk)
				{
				case 'A':
					_input.edit.select_all();
					window->invalidate();
					return 1;
				case 'C':
					(void)_input.edit.copy_to_clipboard();
					return 1;
				case 'X':
					changed(_input.edit.cut_to_clipboard());
					_input.reset_caret(window);
					window->invalidate();
					return 1;
				case 'V':
					changed(_input.edit.paste_from_clipboard());
					_input.reset_caret(window);
					window->invalidate();
					return 1;
				default:
					break;
				}
			}

			if (params.vk == pf::platform_key::Back)
			{
				changed(_input.edit.delete_back());
				_input.reset_caret(window);
				window->invalidate();
				return 1;
			}

			bool modified = false;
			if (_input.on_key_down(window, params.vk, modified))
			{
				changed(modified);
				window->invalidate();
				return 1;
			}
			return 0;
		}

		void handle_paint(pf::window_frame_ptr& window, pf::draw_context& dc) override
		{
			const auto client = window->get_client_rect();
			const auto background = _theme.tool_background;
			dc.fill_solid_rect(client, background);
			pf::ui::edit_box::draw_border(dc, {0, 0, client.right, client.bottom},
			                              window->has_focus(), _theme.dpi_scale);

			const auto bounds = pf::irect(sysmate::ui::scale(6, _theme.dpi_scale),
			                              sysmate::ui::scale(3, _theme.dpi_scale),
			                              client.right - sysmate::ui::scale(6, _theme.dpi_scale),
			                              client.bottom - sysmate::ui::scale(3, _theme.dpi_scale));
			const auto before = std::string_view(_input.edit.text).substr(0, _input.edit.sel_start());
			const auto selected = std::string_view(_input.edit.text).substr(
				_input.edit.sel_start(), _input.edit.sel_end() - _input.edit.sel_start());
			const auto after = std::string_view(_input.edit.text).substr(_input.edit.sel_end());
			auto x = bounds.left;
			draw_segment(dc, before, x, background, bounds);
			x += dc.measure_text(before, _theme.edit_font).cx;
			draw_segment(dc, selected, x, _theme.match_highlight, bounds);
			x += dc.measure_text(selected, _theme.edit_font).cx;
			draw_segment(dc, after, x, background, bounds);
			if (_input.caret.visible && window->has_focus())
				_input.edit.draw_caret(dc, bounds.left, bounds.top, bounds.height(), _theme.edit_font,
				                 _theme.dpi_scale);
		}

		void handle_size(pf::window_frame_ptr&, pf::isize, pf::measure_context&) override
		{
		}
	};

	class performance_view final : public pf::frame_reactor
	{
		std::function<void(pf::draw_context&, pf::irect)> _paint;

	public:
		explicit performance_view(std::function<void(pf::draw_context&, pf::irect)> paint)
			: _paint(std::move(paint))
		{
		}

		uint32_t handle_message(pf::window_frame_ptr,
		                       const pf::message_type message,
		                       const pf::message_params&) override
		{
			return message == pf::message_type::erase_background ? 1 : 0;
		}

		void handle_paint(pf::window_frame_ptr& window, pf::draw_context& dc) override
		{
			if (_paint) _paint(dc, window->get_client_rect());
		}

		void handle_size(pf::window_frame_ptr&, pf::isize, pf::measure_context&) override
		{
		}
	};

	class about_view final : public pf::frame_reactor
	{
		sysmate_theme& _theme;
		std::vector<std::pair<std::string, std::string>> _rows;
		pf::window_frame_ptr _window;
		pf::ui::custom_scrollbar _scroll{pf::ui::custom_scrollbar::orientation::vertical};
		pf::bitmap_ptr _icon;
		pf::isize _extent;
		int _content_height = 0;
		std::function<void(bool)> _on_tab;

		int scale(const int value) const
		{
			return sysmate::ui::scale(value, _theme.dpi_scale);
		}

		void update_scroll()
		{
			_scroll.set_dpi_scale(_theme.dpi_scale);
			_scroll.update(_content_height, _extent.cy, _scroll_position);
		}

		int _scroll_position = 0;

		void scroll_to(const int position)
		{
			const auto next = std::clamp(position, 0, std::max(0, _content_height - _extent.cy));
			if (next != _scroll_position)
			{
				_scroll_position = next;
				update_scroll();
			}
		}

	public:
		explicit about_view(sysmate_theme& theme) : _theme(theme)
		{
			const auto data = pf::embedded_resource_data("sys-mate.ico");
			if (!data.empty())
				_icon = pf::load_bitmap_memory(data.data(), data.size());
		}

		void set_rows(std::vector<std::pair<std::string, std::string>> rows)
		{
			_rows = std::move(rows);
			_content_height = scale(28) * 2 + scale(46 + 28 + 36 + 20) +
				static_cast<int>(_rows.size()) * scale(26);
			update_scroll();
			if (_window) _window->invalidate();
		}

		void set_on_tab(std::function<void(bool)> callback)
		{
			_on_tab = std::move(callback);
		}

		uint32_t handle_message(pf::window_frame_ptr window,
		                       const pf::message_type message,
		                       const pf::message_params& params) override
		{
			_window = window;
			if (message == pf::message_type::erase_background) return 1;
			if (message == pf::message_type::dpi_changed)
			{
				_theme.set_dpi(params.dpi_scale);
				_content_height = scale(28) * 2 + scale(46 + 28 + 36 + 20) +
					static_cast<int>(_rows.size()) * scale(26);
				update_scroll();
				window->invalidate();
			}
			return 0;
		}

		uint32_t handle_mouse(pf::window_frame_ptr window,
		                      const pf::mouse_message_type message,
		                      const pf::mouse_params& params) override
		{
			const auto client = window->get_client_rect();
			const auto scrollbar_client = pf::irect(0, 0, client.right, client.bottom);
			if (message == pf::mouse_message_type::mouse_wheel)
			{
				scroll_to(_scroll_position - params.wheel_delta / 120 * scale(26));
				window->invalidate();
				return 1;
			}

			if (_scroll.handle_mouse(message, params.point, scrollbar_client, window,
			                         [this, &window](const int position)
			                         {
				                         scroll_to(position);
				                         window->invalidate();
			                         }))
			{
				window->invalidate();
				return 1;
			}
			if (message == pf::mouse_message_type::left_button_down)
			{
				window->set_focus();
				return 1;
			}
			if (message == pf::mouse_message_type::set_cursor)
			{
				window->set_cursor_shape(pf::cursor_shape::arrow);
				return 1;
			}
			return 0;
		}

		uint32_t handle_keyboard(pf::window_frame_ptr window,
		                         const pf::keyboard_message_type message,
		                         const pf::keyboard_params& params) override
		{
			if (message != pf::keyboard_message_type::key_down) return 0;
			if (params.vk == pf::platform_key::Tab)
			{
				if (_on_tab) _on_tab(window->is_key_down(pf::platform_key::Shift));
				return 1;
			}

			const auto line = scale(26);
			switch (params.vk)
			{
			case pf::platform_key::Up: scroll_to(_scroll_position - line); break;
			case pf::platform_key::Down: scroll_to(_scroll_position + line); break;
			case pf::platform_key::Prior: scroll_to(_scroll_position - _extent.cy); break;
			case pf::platform_key::Next: scroll_to(_scroll_position + _extent.cy); break;
			case pf::platform_key::Home: scroll_to(0); break;
			case pf::platform_key::End: scroll_to(_content_height); break;
			default: return 0;
			}
			window->invalidate();
			return 1;
		}

		void handle_paint(pf::window_frame_ptr& window, pf::draw_context& dc) override
		{
			const auto client = window->get_client_rect();
			dc.fill_solid_rect(client, _theme.window_background);
			update_scroll();

			const auto content_right = _scroll.can_scroll()
			                               ? client.right - scale(8) : client.right;
			const auto left = scale(28);
			const auto right = content_right - scale(28);
			int y = scale(28) - _scroll_position;

			if (_icon)
				dc.draw_bitmap({left, y, left + scale(64), y + scale(64)}, *_icon);

			const auto text_left = left + scale(64 + 16);
			const auto title_font = pf::font{scale(32), pf::font_name::calibri};
			const auto subtitle_font = pf::font{scale(18), pf::font_name::calibri};
			const auto body_font = _theme.list_font;
			draw_text(dc, "SysMate", title_font, _theme.text, _theme.window_background,
			          {text_left, y, right, y + scale(42)});
			y += scale(46);
			draw_text(dc, "Version 2.0 - System Information Utility", subtitle_font,
			          _theme.dim_text, _theme.window_background,
			          {text_left, y, right, y + scale(24)});
			y += scale(28);
			draw_text(dc, "Copyright (C) 2002 Zac Walker", body_font,
			          _theme.dim_text, _theme.window_background,
			          {text_left, y, right, y + scale(20)});
			y += scale(36);
			dc.fill_solid_rect(left, y, right - left, scale(1), _theme.handle);
			y += scale(20);

			const auto label_width = scale(170);
			const auto value_left = left + scale(180);
			for (const auto& [label, value] : _rows)
			{
				draw_text(dc, label, body_font, _theme.dim_text, _theme.window_background,
				          {left, y, left + label_width, y + scale(22)});
				draw_text(dc, value, body_font, _theme.text, _theme.window_background,
				          {value_left, y, right, y + scale(22)});
				y += scale(26);
			}
			_scroll.draw(dc, {0, 0, client.right, client.bottom});
		}

		void handle_size(pf::window_frame_ptr&, const pf::isize extent,
		                 pf::measure_context&) override
		{
			_extent = extent;
			update_scroll();
		}
	};

	class main_reactor final : public pf::frame_reactor,
	                           public std::enable_shared_from_this<main_reactor>
	{
		pf::window_frame_ptr _frame;
		sysmate_theme _theme;
		sysmate::native::performance_monitor _monitor;
		sysmate::native::hardware_info _hardware;
		sysmate::native::performance_sample _sample;
		std::array<std::deque<float>, chart_count> _history;
		std::array<Chart, chart_count> _charts;
		double _network_peak = 1024.0 * 1024.0;
		uint64_t _handle_peak = 10000;

		tab_id _tab = tab_id::performance;
		std::array<std::string, sysmate::ui::tab_count> _filters;
		sysmate::ui::focus_model _focus;
		int _action_focus = 0;
		int _hot_tab = -1;
		int _hot_button = -1;

		pf::window_frame_ptr _performance_frame;
		pf::window_frame_ptr _process_frame;
		pf::window_frame_ptr _module_frame;
		pf::window_frame_ptr _network_frame;
		pf::window_frame_ptr _window_frame;
		pf::window_frame_ptr _property_frame;
		pf::window_frame_ptr _about_frame;
		pf::window_frame_ptr _filter_frame;

		std::shared_ptr<performance_view> _performance_view;
		std::shared_ptr<table_view> _process_view;
		std::shared_ptr<table_view> _module_view;
		std::shared_ptr<table_view> _network_view;
		std::shared_ptr<table_view> _window_view;
		std::shared_ptr<table_view> _property_view;
		std::shared_ptr<about_view> _about_view;
		std::shared_ptr<filter_view> _filter_view;
		bool _offscreen_test = false;
		bool _ui_test = false;
		int _offscreen_step = 0;
		uint64_t _offscreen_selected = 0;
		bool _offscreen_ok = true;
		std::vector<std::shared_ptr<process_info>> _ui_process_fixture;
		std::vector<std::shared_ptr<network_connection>> _ui_network_fixture;
		std::vector<std::shared_ptr<window_info>> _ui_window_fixture;

		static constexpr pf::color_t accent{0, 150, 230};
		static constexpr uint32_t offscreen_timer = 2;
		static constexpr uint32_t ui_test_timer = 3;

		[[nodiscard]] sysmate::ui::extent client_extent() const
		{
			const auto client = _frame->get_client_rect();
			return {client.width(), client.height()};
		}

		[[nodiscard]] bool has_filter() const
		{
			return _tab == tab_id::processes || _tab == tab_id::network ||
				_tab == tab_id::windows;
		}

		[[nodiscard]] std::vector<pf::menu_command> popup_for_process(
			const std::shared_ptr<void>& payload)
		{
			const auto process = std::static_pointer_cast<process_info>(payload);
			if (!process) return {};
			std::weak_ptr<main_reactor> weak = shared_from_this();
			return {
				pf::menu_command("Show in Explorer", 1,
				                 [weak, process]
				                 {
					                 if (const auto self = weak.lock())
						                 self->show_process(process);
				                 },
				                 [process] { return !process->path.empty(); }),
				pf::menu_command("Kill Process", 2,
				                 [weak, process]
				                 {
					                 if (const auto self = weak.lock())
						                 self->kill_process(process);
				                 })};
		}

		[[nodiscard]] std::vector<pf::menu_command> popup_for_module(
			const std::shared_ptr<void>& payload)
		{
			const auto module = std::static_pointer_cast<sysmate::native::module_info>(payload);
			if (!module) return {};
			std::weak_ptr<main_reactor> weak = shared_from_this();
			return {pf::menu_command("Show in Explorer", 1,
				[weak, module]
				{
					if (const auto self = weak.lock()) self->show_module(module);
				})};
		}

		[[nodiscard]] std::vector<pf::menu_command> popup_for_window(
			const std::shared_ptr<void>& payload)
		{
			const auto window = std::static_pointer_cast<window_info>(payload);
			if (!window) return {};
			std::weak_ptr<main_reactor> weak = shared_from_this();
			return {
				pf::menu_command("Show in Explorer", 1,
				                 [weak, window]
				                 {
					                 if (const auto self = weak.lock())
						                 self->show_window_process(window);
				                 }),
				pf::menu_command("Close Window", 2,
				                 [weak, window]
				                 {
					                 if (const auto self = weak.lock())
						                 self->close_window(window);
				                 })};
		}

		void create_children(const pf::window_frame_ptr& window)
		{
			const std::weak_ptr<main_reactor> weak = shared_from_this();

			_performance_frame = window->create_child("SysMatePerformance",
				pf::window_style::child | pf::window_style::visible |
				pf::window_style::clip_children, _theme.window_background);
			_performance_view = std::make_shared<performance_view>(
				[weak](pf::draw_context& dc, const pf::irect bounds)
				{
					if (const auto self = weak.lock()) self->paint_performance(dc, bounds);
				});
			_performance_frame->set_reactor(_performance_view);

			_process_frame = window->create_child("SysMateProcesses",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.tool_background);
			_process_view = std::make_shared<table_view>(_theme,
				std::vector<pf::ui::list_column>{
					{"Name", 200}, {"PID", 70, true}, {"Memory", 100, true}});
			_process_view->set_on_selection([weak](const std::shared_ptr<void>& data)
			{
				if (const auto self = weak.lock()) self->process_selected(data);
			});
			_process_view->set_popup([weak](const std::shared_ptr<void>& data)
			{
				return weak.expired() ? std::vector<pf::menu_command>{}
				                      : weak.lock()->popup_for_process(data);
			});
			_process_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_table(reverse);
			});
			_process_frame->set_reactor(_process_view);

			_module_frame = window->create_child("SysMateModules",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.tool_background);
			_module_view = std::make_shared<table_view>(_theme,
				std::vector<pf::ui::list_column>{
					{"Module", 180}, {"Path", 300}, {"Size", 80, true}});
			_module_view->set_empty_text("Modules unavailable (access denied)");
			_module_view->set_popup([weak](const std::shared_ptr<void>& data)
			{
				return weak.expired() ? std::vector<pf::menu_command>{}
				                      : weak.lock()->popup_for_module(data);
			});
			_module_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_table(reverse);
			});
			_module_frame->set_reactor(_module_view);

			_network_frame = window->create_child("SysMateNetwork",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.tool_background);
			_network_view = std::make_shared<table_view>(_theme,
				std::vector<pf::ui::list_column>{
					{"Proto", 60}, {"Local Address", 190}, {"Remote Address", 190},
					{"State", 110}, {"PID", 70, true}, {"Process", 160}});
			_network_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_table(reverse);
			});
			_network_frame->set_reactor(_network_view);

			_window_frame = window->create_child("SysMateWindows",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.tool_background);
			_window_view = std::make_shared<table_view>(_theme,
				std::vector<pf::ui::list_column>{
					{"Title", 250}, {"Class", 150}, {"PID", 70, true}, {"Handle", 100}});
			_window_view->set_on_selection([weak](const std::shared_ptr<void>& data)
			{
				if (const auto self = weak.lock()) self->window_selected(data);
			});
			_window_view->set_popup([weak](const std::shared_ptr<void>& data)
			{
				return weak.expired() ? std::vector<pf::menu_command>{}
				                      : weak.lock()->popup_for_window(data);
			});
			_window_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_table(reverse);
			});
			_window_frame->set_reactor(_window_view);

			_property_frame = window->create_child("SysMateWindowProperties",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.tool_background);
			_property_view = std::make_shared<table_view>(_theme,
				std::vector<pf::ui::list_column>{{"Property", 120}, {"Value", 300}});
			_property_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_table(reverse);
			});
			_property_frame->set_reactor(_property_view);

			_about_frame = window->create_child("SysMateAbout",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.window_background);
			_about_view = std::make_shared<about_view>(_theme);
			_about_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_table(reverse);
			});
			_about_frame->set_reactor(_about_view);

			_filter_frame = window->create_child("SysMateFilter",
				pf::window_style::child | pf::window_style::clip_children,
				_theme.tool_background);
			_filter_view = std::make_shared<filter_view>(_theme);
			_filter_view->set_on_change([weak](const std::string_view text)
			{
				if (const auto self = weak.lock()) self->filter_changed(text);
			});
			_filter_view->set_on_tab([weak](const bool reverse)
			{
				if (const auto self = weak.lock()) self->focus_from_filter(reverse);
			});
			_filter_frame->set_reactor(_filter_view);

			_module_view->set_rows({});
			_property_view->set_rows({});
			_show_active_tab();
		}

		void _show_active_tab()
		{
			if (!_frame) return;
			_performance_frame->show(_tab == tab_id::performance);
			_process_frame->show(_tab == tab_id::processes);
			_module_frame->show(_tab == tab_id::processes);
			_network_frame->show(_tab == tab_id::network);
			_window_frame->show(_tab == tab_id::windows);
			_property_frame->show(_tab == tab_id::windows);
			_about_frame->show(_tab == tab_id::about);
			_filter_frame->show(has_filter());
		}

		void layout_children()
		{
			if (!_frame) return;
			const auto client = _frame->get_client_rect();
			const auto l = sysmate::ui::make_layout(
				{client.width(), client.height()}, _frame->get_dpi_scale(), _tab);

			if (_performance_frame) _performance_frame->move_window(
				{l.page.left, l.page.top, l.page.right, l.page.bottom});
			if (_network_frame) _network_frame->move_window(
				{l.page.left, l.page.top, l.page.right, l.page.bottom});
			if (_about_frame) _about_frame->move_window(
				{l.page.left, l.page.top, l.page.right, l.page.bottom});
			if (_process_frame) _process_frame->move_window(
				{l.primary_pane.left, l.primary_pane.top,
				 l.primary_pane.right, l.primary_pane.bottom});
			if (_module_frame) _module_frame->move_window(
				{l.secondary_pane.left, l.secondary_pane.top,
				 l.secondary_pane.right, l.secondary_pane.bottom});
			if (_window_frame) _window_frame->move_window(
				{l.primary_pane.left, l.primary_pane.top,
				 l.primary_pane.right, l.primary_pane.bottom});
			if (_property_frame) _property_frame->move_window(
				{l.secondary_pane.left, l.secondary_pane.top,
				 l.secondary_pane.right, l.secondary_pane.bottom});
			if (_filter_frame)
			{
				_filter_frame->move_window({l.filter.left, l.filter.top,
				                            l.filter.right, l.filter.bottom});
				_filter_frame->show(l.filter_visible);
			}
			_show_active_tab();
		}

		void focus_current_page()
		{
			switch (_tab)
			{
			case tab_id::processes: if (_process_frame) _process_frame->set_focus(); break;
			case tab_id::network: if (_network_frame) _network_frame->set_focus(); break;
			case tab_id::windows: if (_window_frame) _window_frame->set_focus(); break;
			case tab_id::about: if (_about_frame) _about_frame->set_focus(); break;
			case tab_id::performance: if (_performance_frame) _performance_frame->set_focus(); break;
			}
		}

		void focus_from_filter(const bool reverse)
		{
			if (reverse) _frame->set_focus();
			else focus_current_page();
			_focus.target = reverse ? sysmate::ui::focus_target::tabs
			                        : sysmate::ui::focus_target::table;
		}

		void focus_from_table(const bool reverse)
		{
			if (reverse)
			{
				if (has_filter()) _filter_frame->set_focus();
				else _frame->set_focus();
				_focus.target = has_filter() ? sysmate::ui::focus_target::filter
				                             : sysmate::ui::focus_target::tabs;
			}
			else
			{
				_frame->set_focus();
				_focus.target = _tab == tab_id::performance
				                    ? sysmate::ui::focus_target::tabs
				                    : sysmate::ui::focus_target::actions;
				_action_focus = 0;
			}
		}

		void focus_by_tab()
		{
			switch (_focus.target)
			{
			case sysmate::ui::focus_target::tabs: _frame->set_focus(); break;
			case sysmate::ui::focus_target::filter:
				if (has_filter()) _filter_view->focus_select_all(_filter_frame);
				else focus_current_page();
				break;
			case sysmate::ui::focus_target::table: focus_current_page(); break;
			case sysmate::ui::focus_target::actions: _frame->set_focus(); break;
			}
		}

		void filter_changed(const std::string_view text)
		{
			_filters[static_cast<size_t>(_tab)] = std::string(text);
			refresh_current();
		}

		void initialize_ui_fixture()
		{
			if (!_ui_test) return;

			auto first = std::make_shared<process_info>();
			first->pid = 41001;
			first->created = 1001;
			first->name = "zeta-\xC3\xA9.exe";
			first->path = "C:\\fixture\\zeta.exe";
			first->working_set = 20 * 1024 * 1024;
			auto second = std::make_shared<process_info>();
			second->pid = 41002;
			second->created = 1002;
			second->name = "alpha-\xE7\x8C\xAB.exe";
			second->path = "C:\\fixture\\alpha.exe";
			second->working_set = 5 * 1024 * 1024;
			_ui_process_fixture = {first, second};

			auto connection = std::make_shared<network_connection>();
			connection->protocol = "TCP";
			connection->local_address = "127.0.0.1:41001";
			connection->remote_address = "127.0.0.1:443";
			connection->state = "ESTABLISHED";
			connection->pid = first->pid;
			connection->process_name = first->name;
			_ui_network_fixture = {connection};

			auto window = std::make_shared<window_info>();
			window->window_id = 0x1234;
			window->title = "Fixture Window";
			window->class_name = "SysMateFixture";
			window->pid = first->pid;
			window->tid = 41003;
			window->bounds = {10, 20, 410, 320};
			window->style = 0x10;
			window->extended_style = 0x20;
			_ui_window_fixture = {window};
		}

		void refresh_processes()
		{
			std::vector<sysmate::ui::row> rows;
			const auto add = [&rows](const std::shared_ptr<process_info>& process)
			{
				rows.push_back({process_identity(*process),
					{process->name, std::format("{}", process->pid), format_bytes(process->working_set)},
					process});
			};
			if (_ui_test)
			{
				for (const auto& process : _ui_process_fixture) add(process);
			}
			else
			{
				for (const auto& process : sysmate::native::snapshot_processes())
					add(std::make_shared<process_info>(process));
			}
			_process_view->set_empty_text(_filters[1].empty() ? "Empty" : "No matching items");
			_process_view->set_filter(_filters[1]);
			_process_view->set_rows(std::move(rows));

			if (const auto process = std::static_pointer_cast<process_info>(
				    _process_view->selected_data()))
				refresh_modules(process->pid);
			else
				_module_view->set_rows({});
		}

		void refresh_modules(const uint32_t pid)
		{
			std::vector<sysmate::ui::row> rows;
			for (const auto& module : sysmate::native::snapshot_modules(pid))
			{
				auto payload = std::make_shared<sysmate::native::module_info>(module);
				rows.push_back({module_identity(module.path, module.size),
					{module.name, module.path, format_bytes(module.size)}, payload});
			}
			_module_view->set_empty_text("Modules unavailable (access denied)");
			_module_view->set_rows(std::move(rows));
		}

		void refresh_network()
		{
			std::vector<sysmate::ui::row> rows;
			const auto add = [&rows](const std::shared_ptr<network_connection>& connection)
			{
				rows.push_back({network_identity(*connection),
					{connection->protocol, connection->local_address, connection->remote_address,
					 connection->state, std::format("{}", connection->pid), connection->process_name},
					connection});
			};
			if (_ui_test)
			{
				for (const auto& connection : _ui_network_fixture) add(connection);
			}
			else
			{
				for (const auto& connection : sysmate::native::snapshot_network())
					add(std::make_shared<network_connection>(connection));
			}
			_network_view->set_empty_text(_filters[2].empty() ? "Empty" : "No matching items");
			_network_view->set_filter(_filters[2]);
			_network_view->set_rows(std::move(rows));
		}

		void refresh_windows()
		{
			std::vector<sysmate::ui::row> rows;
			const auto add = [&rows](const std::shared_ptr<window_info>& window)
			{
				rows.push_back({window_identity(*window),
					{window->title.empty() ? window->class_name : window->title,
					 window->class_name, std::format("{}", window->pid),
					 std::format("0x{:X}", window->window_id)},
					window});
			};
			if (_ui_test)
			{
				for (const auto& window : _ui_window_fixture) add(window);
			}
			else
			{
				for (const auto& window : sysmate::native::snapshot_windows())
					add(std::make_shared<window_info>(window));
			}
			_window_view->set_empty_text(_filters[3].empty() ? "Empty" : "No matching items");
			_window_view->set_filter(_filters[3]);
			_window_view->set_rows(std::move(rows));
			window_selected(_window_view->selected_data());
		}

		void refresh_about()
		{
			_about_view->set_rows(sysmate::native::collect_system_summary(_hardware));
		}

		void refresh_current()
		{
			switch (_tab)
			{
			case tab_id::performance:
				if (_performance_frame) _performance_frame->invalidate();
				break;
			case tab_id::processes: refresh_processes(); break;
			case tab_id::network: refresh_network(); break;
			case tab_id::windows: refresh_windows(); break;
			case tab_id::about: refresh_about(); break;
			}
		}

		void process_selected(const std::shared_ptr<void>& payload)
		{
			const auto process = std::static_pointer_cast<process_info>(payload);
			if (process) refresh_modules(process->pid);
			else _module_view->set_rows({});
		}

		void window_selected(const std::shared_ptr<void>& payload)
		{
			std::vector<sysmate::ui::row> rows;
			const auto window = std::static_pointer_cast<window_info>(payload);
			if (window)
			{
				const auto add = [&rows](std::string name, std::string value)
				{
					rows.push_back({hash_text(name), {std::move(name), std::move(value)}, {}});
				};
				add("Title", window->title);
				add("Class", window->class_name);
				add("PID", std::format("{}", window->pid));
				add("Thread ID", std::format("{}", window->tid));
				add("Handle", std::format("0x{:X}", window->window_id));
				add("Rect", std::format("{}, {}, {}, {}", window->bounds.left,
					window->bounds.top, window->bounds.right, window->bounds.bottom));
				add("Size", std::format("{} x {}", window->bounds.right - window->bounds.left,
					window->bounds.bottom - window->bounds.top));
				add("Style", format_hex(window->style));
				add("ExStyle", format_hex(window->extended_style));
			}
			_property_view->set_rows(std::move(rows));
		}

		void show_error(const std::string_view text)
		{
			if (_frame) _frame->message_box(text, "SysMate",
				pf::msg_box_style::ok | pf::msg_box_style::icon_warning);
		}

		void show_process(const std::shared_ptr<process_info>& process)
		{
			if (!process) return;
			std::string error;
			if (!sysmate::native::show_process_in_explorer(*process, error)) show_error(error);
		}

		void kill_process(const std::shared_ptr<process_info>& process)
		{
			if (!process) return;
			const auto answer = _frame->message_box(
				std::format("Terminate {} (PID {})?\n\nUnsaved work in this process will be lost.",
				            process->name, process->pid),
				"SysMate", pf::msg_box_style::ok_cancel | pf::msg_box_style::icon_warning);
			if (answer != pf::msg_box_result::ok) return;
			std::string error;
			if (!sysmate::native::terminate_process(*process, error)) show_error(error);
			refresh_processes();
		}

		void show_module(const std::shared_ptr<sysmate::native::module_info>& module)
		{
			if (!module || module->path.empty()) return;
			const auto process = std::make_shared<process_info>();
			process->path = module->path;
			std::string error;
			if (!sysmate::native::show_process_in_explorer(*process, error)) show_error(error);
		}

		void show_window_process(const std::shared_ptr<window_info>& window)
		{
			if (!window) return;
			std::string error;
			if (!sysmate::native::show_window_process_in_explorer(*window, error))
				show_error(error);
		}

		void close_window(const std::shared_ptr<window_info>& window)
		{
			if (!window) return;
			const auto title = window->title.empty() ? window->class_name : window->title;
			const auto answer = _frame->message_box(
				std::format("Close \"{}\"?", title), "SysMate",
				pf::msg_box_style::ok_cancel | pf::msg_box_style::icon_warning);
			if (answer != pf::msg_box_result::ok) return;
			std::string error;
			if (!sysmate::native::close_window(*window, error)) show_error(error);
		}

		void copy_about()
		{
			const auto rows = sysmate::native::collect_system_summary(_hardware);
			std::string text = "SysMate - System Information\r\n";
			text += "================================\r\n";
			for (const auto& [label, value] : rows)
				text += std::format("{}: {}\r\n", label, value);
			if (!_frame->text_to_clipboard(text)) show_error("The clipboard is unavailable.");
		}

		void report_issue()
		{
			std::string error;
			if (!sysmate::native::open_issue_url(error)) show_error(error);
		}

		void action(const int index)
		{
			switch (_tab)
			{
			case tab_id::processes:
				if (index == 0) refresh_processes();
				else if (index == 1) show_process(std::static_pointer_cast<process_info>(
					_process_view->selected_data()));
				else if (index == 2) kill_process(std::static_pointer_cast<process_info>(
					_process_view->selected_data()));
				break;
			case tab_id::network:
				if (index == 0) refresh_network();
				break;
			case tab_id::windows:
				if (index == 0) refresh_windows();
				else if (index == 1) show_window_process(std::static_pointer_cast<window_info>(
					_window_view->selected_data()));
				else if (index == 2) close_window(std::static_pointer_cast<window_info>(
					_window_view->selected_data()));
				break;
			case tab_id::about:
				if (index == 0) report_issue();
				else if (index == 1) copy_about();
				else if (index == 2) refresh_about();
				break;
			case tab_id::performance:
				break;
			}
		}

		void switch_tab(const int index)
		{
			if (index < 0 || index >= sysmate::ui::tab_count) return;
			const auto next = static_cast<tab_id>(index);
			if (next == _tab) return;
			if (_filter_view && has_filter())
				_filters[static_cast<size_t>(_tab)] = _filter_view->text();
			_tab = next;
			if (_filter_view)
			{
				_filter_view->set_text(_filters[static_cast<size_t>(_tab)]);
				_filter_frame->show(has_filter());
			}
			_focus.target = has_filter() ? sysmate::ui::focus_target::filter
			                             : sysmate::ui::focus_target::table;
			_action_focus = 0;
			layout_children();
			refresh_current();
			focus_current_page();
		}

		[[nodiscard]] std::vector<pf::menu_command> menu()
		{
			const auto weak = weak_from_this();
			std::vector<pf::menu_command> view;
			view.emplace_back("Performance", 1100, [weak] { if (auto s = weak.lock()) s->switch_tab(0); },
			                  nullptr, nullptr, pf::key_binding{'1', pf::key_mod::ctrl});
			view.emplace_back("Processes", 1101, [weak] { if (auto s = weak.lock()) s->switch_tab(1); },
			                  nullptr, nullptr, pf::key_binding{'2', pf::key_mod::ctrl});
			view.emplace_back("Network", 1102, [weak] { if (auto s = weak.lock()) s->switch_tab(2); },
			                  nullptr, nullptr, pf::key_binding{'3', pf::key_mod::ctrl});
			view.emplace_back("Windows", 1103, [weak] { if (auto s = weak.lock()) s->switch_tab(3); },
			                  nullptr, nullptr, pf::key_binding{'4', pf::key_mod::ctrl});
			view.emplace_back("About", 1104, [weak] { if (auto s = weak.lock()) s->switch_tab(4); },
			                  nullptr, nullptr, pf::key_binding{'5', pf::key_mod::ctrl});

			pf::menu_command file;
			file.text = "File";
			file.children.emplace_back("Refresh", 1000,
				[weak] { if (auto s = weak.lock()) s->refresh_current(); },
				nullptr, nullptr,
				pf::key_binding{pf::platform_key::F5, pf::key_mod::none},
				pf::key_binding{'R', pf::key_mod::ctrl});
			file.children.emplace_back();
			file.children.emplace_back("Exit", 1001,
				[weak]
				{
					if (const auto s = weak.lock(); s && s->_frame) s->_frame->close();
				});

			pf::menu_command tabs;
			tabs.text = "View";
			tabs.children = std::move(view);
			tabs.children.emplace_back();
			tabs.children.emplace_back("Next Tab", 1010,
				[weak] { if (auto s = weak.lock()) s->switch_tab((static_cast<int>(s->_tab) + 1) % 5); },
				nullptr, nullptr,
				pf::key_binding{pf::platform_key::Tab, pf::key_mod::ctrl},
				pf::key_binding{pf::platform_key::Next, pf::key_mod::ctrl});
			tabs.children.emplace_back("Previous Tab", 1011,
				[weak] { if (auto s = weak.lock()) s->switch_tab((static_cast<int>(s->_tab) + 4) % 5); },
				nullptr, nullptr,
				pf::key_binding{pf::platform_key::Tab, pf::key_mod::ctrl | pf::key_mod::shift},
				pf::key_binding{pf::platform_key::Prior, pf::key_mod::ctrl});
			tabs.children.emplace_back("Focus Filter", 1012,
				[weak]
				{
					if (const auto s = weak.lock(); s && s->has_filter())
					{
						s->_focus.target = sysmate::ui::focus_target::filter;
						s->_filter_view->focus_select_all(s->_filter_frame);
					}
				},
				[weak] { return !weak.expired() && weak.lock()->has_filter(); },
				nullptr, pf::key_binding{'F', pf::key_mod::ctrl});

			pf::menu_command help;
			help.text = "Help";
			help.children.emplace_back("Report an Issue", 1020,
				[weak] { if (auto s = weak.lock()) s->report_issue(); });
			return {std::move(file), std::move(tabs), std::move(help)};
		}

		void paint_tabs_and_bar(pf::draw_context& dc, const pf::irect client)
		{
			const auto l = sysmate::ui::make_layout(
				{client.width(), client.height()}, _theme.dpi_scale, _tab);
			dc.fill_solid_rect(client, _theme.window_background);

			const auto tab_width = l.tabs.width() / sysmate::ui::tab_count;
			for (int i = 0; i < sysmate::ui::tab_count; ++i)
			{
				const auto tab_rect = sysmate::ui::rect{
					i * tab_width, l.tabs.top,
					i == sysmate::ui::tab_count - 1 ? l.tabs.right : (i + 1) * tab_width,
					l.tabs.bottom};
				const auto selected = i == static_cast<int>(_tab);
				const auto hot = i == _hot_tab && !selected;
				if (selected) dc.fill_solid_rect(tab_rect.left, tab_rect.top,
					tab_rect.width(), tab_rect.height(), _theme.handle);
				else if (hot) dc.fill_solid_rect(tab_rect.left, tab_rect.top,
					tab_rect.width(), tab_rect.height(), _theme.handle_hover);
				if (selected) dc.fill_solid_rect(tab_rect.left, tab_rect.top,
					tab_rect.width(), sysmate::ui::scale(2, _theme.dpi_scale), accent);
				draw_centered_text(dc, tab_names[i], _theme.list_font,
				                   selected ? _theme.text : _theme.dim_text,
				                   selected ? _theme.handle : _theme.window_background,
				                   tab_rect);
			}
			dc.fill_solid_rect(0, l.tabs.bottom - 1, client.right, 1, _theme.handle);

			if (l.bottom_bar.height() <= 0) return;
			dc.fill_solid_rect(l.bottom_bar.left, l.bottom_bar.top,
			                   l.bottom_bar.width(), l.bottom_bar.height(),
			                   _theme.tool_background);

			std::vector<std::string_view> labels;
			switch (_tab)
			{
			case tab_id::processes: labels = {"Refresh", "Open in Explorer", "Kill Process"}; break;
			case tab_id::network: labels = {"Refresh"}; break;
			case tab_id::windows: labels = {"Refresh", "Open in Explorer", "Close Window"}; break;
			case tab_id::about: labels = {"Report an Issue", "Copy to Clipboard", "Refresh"}; break;
			case tab_id::performance: break;
			}

			for (size_t i = 0; i < l.buttons.size() && i < labels.size(); ++i)
			{
				const auto& button = l.buttons[i];
				const auto selected = _hot_button == static_cast<int>(i);
				const auto focused = _focus.target == sysmate::ui::focus_target::actions &&
					_action_focus == static_cast<int>(i) && _frame && _frame->has_focus();
				const auto background = selected ? _theme.handle_hover : _theme.handle;
				dc.fill_solid_rect(button.left, button.top, button.width(), button.height(), background);
				dc.draw_solid_line({button.left, button.top},
				                   {button.right - 1, button.top}, _theme.line);
				dc.draw_solid_line({button.left, button.bottom - 1},
				                   {button.right - 1, button.bottom - 1}, _theme.line);
				dc.draw_solid_line({button.left, button.top},
				                   {button.left, button.bottom - 1}, _theme.line);
				dc.draw_solid_line({button.right - 1, button.top},
				                   {button.right - 1, button.bottom - 1}, _theme.line);
				draw_centered_text(dc, labels[i], _theme.list_font, _theme.text, background, button);
				if (focused)
				{
					dc.draw_solid_line({button.left + 2, button.top + 2},
					                   {button.right - 3, button.top + 2}, accent);
					dc.draw_solid_line({button.left + 2, button.bottom - 3},
					                   {button.right - 3, button.bottom - 3}, accent);
					dc.draw_solid_line({button.left + 2, button.top + 2},
					                   {button.left + 2, button.bottom - 3}, accent);
					dc.draw_solid_line({button.right - 3, button.top + 2},
					                   {button.right - 3, button.bottom - 3}, accent);
				}
			}
		}

		void paint_performance(pf::draw_context& dc, const pf::irect bounds)
		{
			dc.fill_solid_rect(bounds, _theme.window_background);
			const auto scale = _theme.dpi_scale;
			const auto pad = sysmate::ui::scale(4, scale);
			const auto spacing = sysmate::ui::scale(8, scale);
			const auto columns = bounds.width() > sysmate::ui::scale(500, scale) ? 2 : 1;
			const auto rows = (chart_count + columns - 1) / columns;
			const auto chart_width = bounds.width() / columns - spacing;
			const auto chart_height = bounds.height() / rows - spacing;
			if (chart_width < sysmate::ui::scale(50, scale) ||
				chart_height < sysmate::ui::scale(30, scale))
				return;

			const auto text_margin = sysmate::ui::scale(16, scale);
			const auto text_top = sysmate::ui::scale(8, scale);
			const auto text_bottom = sysmate::ui::scale(34, scale);
			const auto detail_bottom = sysmate::ui::scale(50, scale);
			const auto value_margin = sysmate::ui::scale(160, scale);

			static constexpr ChartColor palette[chart_count][2] = {
				{{0, 180, 255}, {0, 120, 200}},
				{{180, 0, 255}, {140, 0, 200}},
				{{0, 200, 100}, {0, 150, 80}},
				{{255, 180, 0}, {200, 140, 0}},
				{{255, 80, 80}, {200, 50, 50}},
				{{100, 220, 220}, {60, 170, 170}}};

			for (int i = 0; i < chart_count; ++i)
			{
				const auto column = i % columns;
				const auto row = i / columns;
				const auto x = pad + column * (chart_width + spacing);
				const auto y = pad + row * (chart_height + spacing);

				auto& chart = _charts[i];
				chart.resize(chart_width, chart_height);
				ChartStyle style;
				style.lineColor = palette[i][0];
				style.fillColor = palette[i][1];
				chart.set_style(style);
				chart.set_margin(sysmate::ui::scale(8, scale));
				chart.set_data(_history[i]);
				chart.render();

				pf::bitmap bitmap(chart.width(), chart.height(), chart.pixels());
				dc.draw_bitmap(x, y, bitmap);

				draw_text(dc, chart_names[i], _theme.list_font, _theme.text,
				          _theme.window_background,
				          {x + text_margin, y + text_top, x + chart_width - spacing, y + text_bottom});
				std::string value;
				if (i == static_cast<int>(chart_id::network))
					value = format_rate(_sample.network_bytes_per_second);
				else if (i == static_cast<int>(chart_id::handles))
					value = std::format("{}", _sample.handle_count);
				else if (i == static_cast<int>(chart_id::gpu) && !_sample.gpu_available)
					value = "n/a";
				else
				{
					const auto latest = _history[i].empty() ? 0.0f : _history[i].back();
					value = std::format("{:.1f}%", latest);
				}
				const auto value_size = dc.measure_text(value, _theme.list_font);
				draw_text(dc, value, _theme.list_font, _theme.text,
				          _theme.window_background,
				          {x + chart_width - value_margin, y + text_top,
				           x + chart_width - text_margin, y + text_bottom});
				(void)value_size;

				std::string detail;
				switch (static_cast<chart_id>(i))
				{
				case chart_id::cpu:
					detail = _hardware.cpu_name.empty()
					             ? std::format("{} logical cores @ {:.2f} GHz",
					                           _hardware.logical_cores, _hardware.cpu_mhz / 1000.0)
					             : std::format("{}  ({} cores @ {:.2f} GHz)",
					                           _hardware.cpu_name, _hardware.logical_cores,
					                           _hardware.cpu_mhz / 1000.0);
					break;
				case chart_id::memory:
					detail = std::format("Used {} / {}  (Available {})",
						format_bytes(_sample.memory_total_bytes - _sample.memory_available_bytes),
						format_bytes(_sample.memory_total_bytes),
						format_bytes(_sample.memory_available_bytes));
					break;
				case chart_id::disk:
					detail = std::format("{}:  Used {} / {}  (Free {})",
						_hardware.system_drive,
						format_bytes(_sample.disk_total_bytes - _sample.disk_free_bytes),
						format_bytes(_sample.disk_total_bytes),
						format_bytes(_sample.disk_free_bytes));
					break;
				case chart_id::network:
					detail = "Scale " + format_rate(_network_peak) + " (peak seen)";
					break;
				case chart_id::gpu:
					detail = _sample.gpu_available
					             ? (_hardware.gpu_name.empty() ? "Busiest engine" : _hardware.gpu_name)
					             : (_hardware.gpu_name.empty() ? "GPU counters unavailable" : _hardware.gpu_name);
					break;
				case chart_id::handles:
					detail = "Total system handles";
					break;
				}
				if (!detail.empty())
					draw_text(dc, detail, _theme.list_font, _theme.dim_text,
					          _theme.window_background,
					          {x + text_margin, y + text_bottom,
					           x + chart_width - spacing, y + detail_bottom});
			}
		}

		void sample_performance()
		{
			_sample = _monitor.sample();
			if (_sample.network_bytes_per_second > _network_peak)
				_network_peak = _sample.network_bytes_per_second * 1.2;
			if (_sample.handle_count > _handle_peak)
				_handle_peak = static_cast<uint64_t>(_sample.handle_count * 1.2);

			const std::array<float, chart_count> values = {
				static_cast<float>(std::clamp(_sample.cpu_percent, 0.0, 100.0)),
				static_cast<float>(_sample.memory_load),
				static_cast<float>(std::clamp(_sample.disk_percent, 0.0, 100.0)),
				static_cast<float>(std::clamp(_sample.network_bytes_per_second /
				                              _network_peak * 100.0, 0.0, 100.0)),
				static_cast<float>(std::clamp(_sample.gpu_percent, 0.0, 100.0)),
				static_cast<float>(std::clamp(static_cast<double>(_sample.handle_count) /
				                              std::max<uint64_t>(1, _handle_peak) * 100.0,
				                              0.0, 100.0))};
			for (int i = 0; i < chart_count; ++i)
			{
				_history[i].push_back(values[i]);
				while (_history[i].size() > 60) _history[i].pop_front();
			}
			if (_performance_frame) _performance_frame->invalidate();
		}

		void offscreen_check(const bool condition, const std::string_view message)
		{
			if (!condition) _offscreen_ok = false;
			pf::write_stdout(std::format("offscreen: {} {}\n",
				condition ? "PASS" : "FAIL", message));
		}

		void run_offscreen_step(const pf::window_frame_ptr& window)
		{
			switch (_offscreen_step++)
			{
			case 0:
				switch_tab(static_cast<int>(tab_id::processes));
				offscreen_check(_process_view->select_first_for_test(),
				                "process selection is reachable");
				_offscreen_selected = _process_view->selected_id();
				break;
			case 1:
				refresh_processes();
				offscreen_check(_offscreen_selected != 0 &&
				                _process_view->selected_id() == _offscreen_selected,
				                "selection identity survives refresh");
				break;
			case 2:
				switch_tab(static_cast<int>(tab_id::network));
				offscreen_check(_network_view != nullptr, "network reactor survives tab switch");
				break;
			case 3:
				switch_tab(static_cast<int>(tab_id::windows));
				offscreen_check(_window_view != nullptr, "window reactor survives tab switch");
				break;
			case 4:
				switch_tab(static_cast<int>(tab_id::about));
				offscreen_check(_about_view != nullptr, "about reactor survives tab switch");
				break;
			case 5:
				switch_tab(static_cast<int>(tab_id::performance));
				offscreen_check(_performance_view != nullptr, "chart reactor survives tab switch");
				break;
			default:
				window->kill_timer(offscreen_timer);
				pf::write_stdout(std::format("offscreen: lifecycle {}\n",
					_offscreen_ok ? "passed" : "FAILED"));
				window->close();
				break;
			}
		}

		void ui_test_check(const bool condition, const std::string_view message)
		{
			if (!condition) _offscreen_ok = false;
			pf::write_stdout(std::format("ui-test: {} {}\n",
				condition ? "PASS" : "FAIL", message));
		}

		void run_ui_test(pf::window_frame_ptr window)
		{
			switch_tab(static_cast<int>(tab_id::processes));
			const auto client = _process_frame->get_client_rect();
			ui_test_check(client.width() > 0 && client.height() > 0,
			              "real process client geometry");
			ui_test_check(_process_view->row_count_for_test() == 2,
			              "deterministic fixture rows");

			pf::ui::test::fake_draw_context draw;
			_process_view->handle_paint(_process_frame, draw);
			ui_test_check(!draw.texts.empty() && !draw.fills.empty(),
			              "actual list reactor records drawing");

			pf::mouse_params header_click;
			header_click.point = {10, 2};
			_process_view->handle_mouse(_process_frame,
				pf::mouse_message_type::left_button_down, header_click);
			const auto sorted = std::static_pointer_cast<process_info>(
				_process_view->payload_for_test(0));
			ui_test_check(sorted && sorted->pid == 41002,
			              "actual header click sorts fixture rows");

			ui_test_check(_process_view->select_first_for_test(),
			              "actual list selection callback");
			const auto selected_payload = _process_view->selected_data();
			const auto selected_id = _process_view->selected_id();
			refresh_processes();
			ui_test_check(selected_id != 0 && _process_view->selected_id() == selected_id &&
			              _process_view->selected_data() == selected_payload,
			              "refresh preserves payload identity");

			const auto columns = _process_view->column_rects(client.width());
			pf::mouse_params drag_start;
			drag_start.point = {columns.front().right, 2};
			_process_view->handle_mouse(_process_frame,
				pf::mouse_message_type::left_button_down, drag_start);
			ui_test_check(_process_view->header_drag_active_for_test(),
			              "header drag captures");
			_process_view->handle_message(_process_frame,
				pf::message_type::capture_changed, {});
			ui_test_check(!_process_view->header_drag_active_for_test(),
			              "capture loss cancels header drag");

			if (auto measure = window->create_measure_context())
				handle_size(window, {0, 0}, *measure);
			ui_test_check(true, "minimized zero-extent layout is handled");

			auto fake_window = std::make_shared<pf::ui::test::fake_window_frame>();
			fake_window->set_focus();
			switch_tab(static_cast<int>(tab_id::processes));
			_filter_view->set_text({});
			_filter_view->focus_select_all(fake_window);
			pf::keyboard_params character;
			character.ch = U'\u00e9';
			_filter_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::char_input, character);
			character.ch = U'\u732b';
			_filter_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::char_input, character);
			ui_test_check(_filter_view->text() == "\xC3\xA9\xE7\x8C\xAB",
			              "filter accepts UTF-8 input");

			fake_window->held_keys.insert(pf::platform_key::Control);
			pf::keyboard_params key;
			key.vk = 'A';
			_filter_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::key_down, key);
			ui_test_check(_filter_view->selected_text_for_test() == _filter_view->text(),
			              "filter Ctrl+A selects UTF-8 text");
			key.vk = 'C';
			_filter_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::key_down, key);
			const auto clipboard_available = pf::platform_clipboard_has_text();
			_filter_view->set_text({});
			key.vk = 'V';
			_filter_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::key_down, key);
			ui_test_check(!clipboard_available ||
			              _filter_view->text() == "\xC3\xA9\xE7\x8C\xAB",
			              "filter clipboard round trip");
			fake_window->held_keys.clear();

			key.vk = pf::platform_key::Tab;
			_filter_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::key_down, key);
			ui_test_check(_focus.target == sysmate::ui::focus_target::table,
			              "filter Tab routes focus to table");
			_process_view->handle_keyboard(fake_window,
				pf::keyboard_message_type::key_down, key);
			ui_test_check(_focus.target == sysmate::ui::focus_target::actions,
			              "table Tab routes focus to actions");

			window->kill_timer(ui_test_timer);
			pf::write_stdout(std::format("ui-test: lifecycle {}\n",
				_offscreen_ok ? "passed" : "FAILED"));
			window->close();
		}

	public:
		void release_reactors()
		{
			for (const auto& child : {_performance_frame, _process_frame, _module_frame,
				_network_frame, _window_frame, _property_frame, _about_frame, _filter_frame})
				if (child) child->set_reactor({});

			_performance_view.reset();
			_process_view.reset();
			_module_view.reset();
			_network_view.reset();
			_window_view.reset();
			_property_view.reset();
			_about_view.reset();
			_filter_view.reset();

			if (_frame) _frame->set_reactor({});
			_performance_frame.reset();
			_process_frame.reset();
			_module_frame.reset();
			_network_frame.reset();
			_window_frame.reset();
			_property_frame.reset();
			_about_frame.reset();
			_filter_frame.reset();
			_frame.reset();
		}

		explicit main_reactor(const bool offscreen_test = false,
		                      const bool ui_test = false)
			: _offscreen_test(offscreen_test), _ui_test(ui_test)
		{
			_theme.set_dpi(1.0);
			for (auto& history : _history) history.assign(60, 0.0f);
		}

		[[nodiscard]] std::vector<pf::menu_command> make_menu() { return menu(); }

		uint32_t handle_message(pf::window_frame_ptr window,
		                       const pf::message_type message,
		                       const pf::message_params& params) override
		{
			if (message == pf::message_type::create)
			{
				_frame = window;
				_theme.set_dpi(window->get_dpi_scale());
				window->set_minimum_client_size(
					{sysmate::ui::scale(560, _theme.dpi_scale),
					 sysmate::ui::scale(400, _theme.dpi_scale)});
				_monitor.initialize();
				_hardware = _monitor.hardware();
				const auto summary = sysmate::native::collect_system_summary(_hardware);
				auto computer = std::find_if(summary.begin(), summary.end(),
					[](const auto& item) { return item.first == "Computer Name"; });
				window->set_text(computer == summary.end() ? "SysMate"
				                                             : "SysMate - " + computer->second);
				create_children(window);
				initialize_ui_fixture();
				window->set_timer(performance_timer, 1000);
				if (_offscreen_test) window->set_timer(offscreen_timer, 50);
				if (_ui_test) window->set_timer(ui_test_timer, 50);
				refresh_current();
				layout_children();
				return 0;
			}
			if (message == pf::message_type::erase_background) return 1;
			if (message == pf::message_type::timer && params.timer_id == performance_timer)
			{
				sample_performance();
				return 0;
			}
			if (message == pf::message_type::timer && params.timer_id == offscreen_timer)
			{
				run_offscreen_step(window);
				return 0;
			}
			if (message == pf::message_type::timer && params.timer_id == ui_test_timer)
			{
				run_ui_test(window);
				return 0;
			}
			if (message == pf::message_type::dpi_changed)
			{
				_theme.set_dpi(params.dpi_scale);
				window->set_minimum_client_size(
					{sysmate::ui::scale(560, _theme.dpi_scale),
					 sysmate::ui::scale(400, _theme.dpi_scale)});
				layout_children();
				for (const auto& child : {_performance_frame, _process_frame, _module_frame,
					_network_frame, _window_frame, _property_frame, _about_frame, _filter_frame})
					if (child) child->notify_size();
				if (_frame) _frame->invalidate();
				return 0;
			}
			if (message == pf::message_type::set_focus)
			{
				focus_by_tab();
				return 0;
			}
			if (message == pf::message_type::close)
			{
				window->close();
				return 0;
			}
			if (message == pf::message_type::destroy)
			{
				window->kill_timer(performance_timer);
				return 0;
			}
			return 0;
		}

		uint32_t handle_mouse(pf::window_frame_ptr window,
		                      const pf::mouse_message_type message,
		                      const pf::mouse_params& params) override
		{
			const auto client = window->get_client_rect();
			const auto l = sysmate::ui::make_layout(
				{client.width(), client.height()}, _theme.dpi_scale, _tab);
			if (message == pf::mouse_message_type::set_cursor)
			{
				window->set_cursor_shape(pf::cursor_shape::arrow);
				return 1;
			}
			if (message == pf::mouse_message_type::mouse_leave)
			{
				_hot_tab = -1;
				_hot_button = -1;
				window->invalidate();
				return 1;
			}
			if (message == pf::mouse_message_type::mouse_move)
			{
				const auto new_tab = sysmate::ui::tab_at(l, {params.point.x, params.point.y});
				const auto new_button = sysmate::ui::button_at(l, {params.point.x, params.point.y});
				if (new_tab != _hot_tab || new_button != _hot_button)
				{
					_hot_tab = new_tab;
					_hot_button = new_button;
					window->track_mouse_leave();
					window->invalidate();
				}
				return 1;
			}
			if (message != pf::mouse_message_type::left_button_down) return 0;

			if (const auto tab = sysmate::ui::tab_at(l, {params.point.x, params.point.y});
				tab >= 0)
			{
				_focus.target = sysmate::ui::focus_target::tabs;
				window->set_focus();
				switch_tab(tab);
				return 1;
			}
			if (const auto button = sysmate::ui::button_at(l, {params.point.x, params.point.y});
				button >= 0)
			{
				_focus.target = sysmate::ui::focus_target::actions;
				_action_focus = button;
				window->set_focus();
				action(button);
				return 1;
			}
			return 0;
		}

		uint32_t handle_keyboard(pf::window_frame_ptr window,
		                         const pf::keyboard_message_type message,
		                         const pf::keyboard_params& params) override
		{
			if (message != pf::keyboard_message_type::key_down) return 0;
			if (params.vk == pf::platform_key::Tab)
			{
				const auto reverse = window->is_key_down(pf::platform_key::Shift);
				const auto action_count = _tab == tab_id::performance ? 0 :
					_tab == tab_id::network ? 1 :
					_tab == tab_id::about ? 3 : 3;
				if (_focus.target == sysmate::ui::focus_target::actions &&
					action_count > 0 &&
					((!reverse && _action_focus + 1 < action_count) ||
					 (reverse && _action_focus > 0)))
				{
					_action_focus += reverse ? -1 : 1;
					focus_by_tab();
					return 1;
				}
				_focus.advance(reverse, has_filter(), action_count > 0);
				focus_by_tab();
				return 1;
			}
			if (_focus.target == sysmate::ui::focus_target::tabs)
			{
				if (params.vk == pf::platform_key::Left)
				{
					switch_tab((static_cast<int>(_tab) + 4) % 5);
					return 1;
				}
				if (params.vk == pf::platform_key::Right)
				{
					switch_tab((static_cast<int>(_tab) + 1) % 5);
					return 1;
				}
			}
			if (_focus.target == sysmate::ui::focus_target::actions &&
				(params.vk == pf::platform_key::Return || params.vk == pf::platform_key::Space))
			{
				action(_action_focus);
				return 1;
			}
			if (_focus.target == sysmate::ui::focus_target::actions)
			{
				const auto action_count = _tab == tab_id::performance ? 0 :
					_tab == tab_id::network ? 1 :
					_tab == tab_id::about ? 3 : 3;
				if (params.vk == pf::platform_key::Left && _action_focus > 0)
				{
					--_action_focus;
					return 1;
				}
				if (params.vk == pf::platform_key::Right &&
					_action_focus + 1 < action_count)
				{
					++_action_focus;
					return 1;
				}
			}
			return 0;
		}

		void handle_paint(pf::window_frame_ptr& window, pf::draw_context& dc) override
		{
			paint_tabs_and_bar(dc, window->get_client_rect());
		}

		void handle_size(pf::window_frame_ptr&, const pf::isize,
		                 pf::measure_context&) override
		{
			layout_children();
		}
	};

	std::shared_ptr<main_reactor> g_main;
	bool g_ui_test_run = false;
}

app_init_result app_init(const pf::window_frame_ptr& main_frame,
                         const std::span<const std::string_view> params)
{
	pf::config_set_app_name("sys-mate");
	const auto offscreen_test = std::ranges::any_of(params,
		[](const std::string_view param)
		{
			return param == "--test-offscreen" || param == "/test-offscreen";
		});
	const auto ui_test = std::ranges::any_of(params,
		[](const std::string_view param)
		{
			return param == "--test-ui" || param == "/test-ui";
		});
	g_main = std::make_shared<main_reactor>(offscreen_test || ui_test, ui_test);
	g_ui_test_run = ui_test;
	main_frame->set_reactor(g_main);
	main_frame->set_menu(g_main->make_menu());
	app_init_result result;
	result.window.initial_client_size = {1000, 650};
	result.window.minimum_client_size = {560, 400};
	result.offscreen_gui = offscreen_test || ui_test;
	return result;
}

void app_idle()
{
}

void app_destroy()
{
	const auto weak = std::weak_ptr<main_reactor>(g_main);
	if (g_main) g_main->release_reactors();
	g_main.reset();
	if (g_ui_test_run)
		pf::write_stdout(std::format("ui-test: weak expired {}\n",
			weak.expired() ? "passed" : "FAILED"));
	g_ui_test_run = false;
}
