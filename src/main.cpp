// SysMate application shell.
//
// SysMate is a ui application: libdesktop owns the native window, input
// dispatch, menus, DPI and frame loop. The monitoring backend remains the
// only Windows-facing part of the application.

#include "libdesktop/ui.h"
#include "libdesktop/ui/controls.h"
#include "libdesktop/ui/elements.h"
#include "libdesktop/ui/menu_bar.h"
#include "libdesktop/ui/test_support.h"
#include "libdesktop/ui/views.h"

#include "chart.h"
#include "native_monitoring.h"
#include "ui_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
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
	constexpr std::string_view tab_names[] = {
		"Performance", "Processes", "Network", "Windows", "About"};
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

	uint64_t window_identity(const window_info& window) { return window.window_id; }

	uint64_t module_identity(const std::string_view path, const uint64_t size)
	{
		return hash_text(std::format("{}:{}", path, size));
	}

	class sysmate_theme final : public ui::view_theme
	{
	public:
		void set_dpi(const double scale)
		{
			dpi_scale = scale > 0.0 ? scale : 1.0;
			list_font_height = std::max(8, sysmate::ui::scale(13, dpi_scale));
			text_font_height = list_font_height;
			list_font = {"Calibri", list_font_height};
			edit_font = {"Calibri", list_font_height};
			text_font = {"Consolas", list_font_height};
			padding_x = sysmate::ui::scale(6, dpi_scale);
			padding_y = sysmate::ui::scale(3, dpi_scale);
			indent = sysmate::ui::scale(16, dpi_scale);
			list_top_pad = sysmate::ui::scale(2, dpi_scale);
			list_scroll_pad = sysmate::ui::scale(4, dpi_scale);

			window_background = core::rgb(30, 30, 30);
			tool_background = core::rgb(42, 42, 46);
			text = core::rgb(225, 225, 225);
			dim_text = core::rgb(165, 165, 170);
			group_text = core::rgb(225, 195, 80);
			line = core::rgb(80, 120, 180);
			match_highlight = core::rgb(0, 100, 180);
			header_background = core::rgb(50, 50, 56);
			handle = core::rgb(55, 55, 62);
			handle_hover = core::rgb(70, 70, 78);
			handle_tracking = core::rgb(0, 100, 180);
			focus_handle = core::rgb(0, 120, 210);
		}
	};

	class dashboard_element final : public ui::element
	{
	public:
		std::vector<ui::element_ptr> children;
		std::function<void(render::measure_context&, core::recti)> layout_children;
		std::function<void(render::draw_context&, core::recti)> paint_decoration;
		core::color32 background = core::rgb(30, 30, 30);

		dashboard_element() { padding(0); margin(0); }

		core::sizei measure(render::measure_context&, const int width_limit) const override
		{
			return {width_limit, 0};
		}

		void layout(render::measure_context& mc, const core::recti bounds_in) override
		{
			bounds = bounds_in;
			if (layout_children) layout_children(mc, bounds);
		}

		void render(render::draw_context& dc, const core::pointi offset) const override
		{
			dc.draw_rect(bounds.offset(offset), core::color(background, dc.colors.alpha));
			if (paint_decoration) paint_decoration(dc, bounds.offset(offset));
			for (const auto& child : children)
				if (child && child->is_visible()) child->render(dc, offset);
		}

		void visit_children(const std::function<void(const ui::element_ptr&)>& visit) const override
		{
			for (const auto& child : children) visit(child);
		}
	};

	class performance_element final : public ui::element
	{
		std::function<void(render::draw_context&, core::recti)> _paint;

	public:
		explicit performance_element(std::function<void(render::draw_context&, core::recti)> paint) :
			_paint(std::move(paint)) { padding(0); margin(0); }

		core::sizei measure(render::measure_context&, const int width_limit) const override
		{
			return {width_limit, 0};
		}

		void render(render::draw_context& dc, const core::pointi offset) const override
		{
			if (_paint) _paint(dc, bounds.offset(offset));
		}
	};

	class about_element final : public ui::element
	{
		sysmate_theme& _theme;
		std::vector<std::pair<std::string, std::string>> _rows;
		core::surface_ptr _icon;
		int _scroll_position = 0;
		int _content_height = 0;
		std::function<void()> _changed;

		int scale(const int value) const
		{
			return sysmate::ui::scale(value, _theme.dpi_scale);
		}

		void update_scroll() const
		{
			// The mutable scroll position is updated by wheel/layout; painting only clamps it.
			const_cast<about_element*>(this)->_scroll_position = std::clamp(
				_scroll_position, 0, std::max(0, _content_height - bounds.height()));
		}

		void draw_text(render::draw_context& dc, const std::string_view text,
		               const render::font_face font, const core::color32 color,
		               const core::recti rect) const
		{
			if (text.empty() || rect.is_empty()) return;
			dc.draw_text(text, rect, font, render::text_style::single_line,
			             core::color(color, dc.colors.alpha), core::color(0, 0, 0, 0));
		}

	public:
		about_element(sysmate_theme& theme, std::function<void()> changed) :
			_theme(theme), _changed(std::move(changed))
		{
			padding(0);
			margin(0);
			const auto data = platform::embedded_resource_data("sys-mate.ico");
			if (!data.empty()) _icon = platform::load_image_memory(data);
		}

		void set_rows(std::vector<std::pair<std::string, std::string>> rows)
		{
			_rows = std::move(rows);
			_content_height = scale(28) * 2 + scale(46 + 28 + 36 + 20) +
				static_cast<int>(_rows.size()) * scale(26);
			update_scroll();
			if (_changed) _changed();
		}

		bool wheel(const core::pointi loc, const core::pointi element_offset, const int delta) override
		{
			const auto area = bounds.offset(element_offset);
			if (!area.contains(loc) || _content_height <= area.height()) return false;
			const auto before = _scroll_position;
			_scroll_position = std::clamp(_scroll_position - delta / 2, 0,
				std::max(0, _content_height - area.height()));
			if (_scroll_position != before && _changed) _changed();
			return true;
		}

		bool key_down(const int key)
		{
			const auto maximum = std::max(0, _content_height - bounds.height());
			if (maximum == 0) return false;
			const auto page = std::max(1, bounds.height() - scale(24));
			int next = _scroll_position;
			const auto k = static_cast<char32_t>(key);
			if (k == platform::keys::UP) next -= scale(26);
			else if (k == platform::keys::DOWN) next += scale(26);
			else if (k == platform::keys::PRIOR) next -= page;
			else if (k == platform::keys::NEXT) next += page;
			else if (k == platform::keys::HOME) next = 0;
			else if (k == platform::keys::END) next = maximum;
			else return false;
			next = std::clamp(next, 0, maximum);
			if (next == _scroll_position) return true;
			_scroll_position = next;
			if (_changed) _changed();
			return true;
		}

		core::sizei measure(render::measure_context&, const int width_limit) const override
		{
			return {width_limit, 0};
		}

		void render(render::draw_context& dc, const core::pointi offset) const override
		{
			const auto area = bounds.offset(offset);
			dc.draw_rect(area, core::color(_theme.window_background, dc.colors.alpha));
			update_scroll();
			render::scoped_clip clip(dc, area);

			const auto left = area.left + scale(28);
			const auto right = area.right - scale(28);
			int y = area.top + scale(28) - _scroll_position;
			if (_icon)
				render::draw_surface(dc, _icon, {left, y, left + scale(64), y + scale(64)},
				                     1.0f, render::texture_sampler::point);

			const auto text_left = left + scale(64 + 16);
			const auto title_font = render::make_font({"Calibri", scale(32)});
			const auto subtitle_font = render::make_font({"Calibri", scale(18)});
			const auto body_font = render::make_font(_theme.list_font);
			draw_text(dc, "SysMate", title_font, _theme.text,
			          {text_left, y, right, y + scale(42)});
			y += scale(46);
			draw_text(dc, "Version 2.0 - System Information Utility", subtitle_font,
			          _theme.dim_text, {text_left, y, right, y + scale(24)});
			y += scale(28);
			draw_text(dc, "Copyright (C) 2002 Zac Walker", body_font,
			          _theme.dim_text, {text_left, y, right, y + scale(20)});
			y += scale(36);
			dc.draw_rect({left, y, right, y + scale(1)}, core::color(_theme.handle, dc.colors.alpha));
			y += scale(20);

			const auto label_width = scale(170);
			const auto value_left = left + scale(180);
			for (const auto& [label, value] : _rows)
			{
				draw_text(dc, label, body_font, _theme.dim_text,

				          {left, y, left + label_width, y + scale(22)});
				draw_text(dc, value, body_font, _theme.text,
				          {value_left, y, right, y + scale(22)});
				y += scale(26);
			}

			if (_content_height > area.height())
			{
				const auto width = std::max(3, scale(8));
				const auto track = core::recti(area.right - width, area.top,
				                              area.right, area.bottom);
				const auto thumb_height = std::max(scale(24),
				                                  track.height() * area.height() / _content_height);
				const auto travel = std::max(0, track.height() - thumb_height);
				const auto thumb_top = track.top + (_scroll_position * travel /
				                                  std::max(1, _content_height - area.height()));
				dc.draw_rect(track, core::color(_theme.handle, dc.colors.alpha * 0.35f));
				dc.draw_rounded_rect({track.left, thumb_top, track.right, thumb_top + thumb_height},
				                    core::color(_theme.handle_hover, dc.colors.alpha), dc.padding1);
			}

		}
	};

	class table_view final : public ui::list_view
	{
		struct row_tag
		{
			uint64_t id = 0;
			std::shared_ptr<void> payload;
		};

		sysmate::ui::table_model _model;
		std::string _empty_text = "Empty";
		sysmate::ui::header_drag_state _drag;
		std::function<void(std::shared_ptr<void>)> _on_selection;
		std::function<std::vector<ui::command_ptr>(std::shared_ptr<void>)> _popup;

		void rebuild_items()
		{
			const auto selected = _model.selected_id();
			_items.clear();
			_items.reserve(_model.rows().size());
			for (const auto& model_row : _model.rows())
			{
				auto item = std::make_shared<ui::list_item>();
				item->text = model_row.cells.empty() ? std::string() : model_row.cells.front();
				for (const auto& cell : model_row.cells) item->cells.emplace_back(cell);
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

		void select_position(const int index)
		{
			if (_items.empty()) return;
			const auto clamped = std::clamp(index, 0, static_cast<int>(_items.size()) - 1);
			set_selected(clamped);
			const auto tag = std::static_pointer_cast<row_tag>(_items[clamped]->data);
			if (tag)
			{
				_model.select_id(tag->id);
				if (_on_selection) _on_selection(tag->payload);
			}
			ensure_visible(_items[clamped]);
			invalidate();
		}

		int edge_column(const int x) const
		{
			const auto rects = column_rects(_view_extent.cx);
			const auto hit_zone = std::max(3, sysmate::ui::scale(4, _theme.dpi_scale));
			for (int i = 0; i + 1 < static_cast<int>(rects.size()); ++i)
				if (std::abs(x - rects[i].right) <= hit_zone) return i;
			return -1;
		}

	public:
		table_view(sysmate_theme& theme, std::vector<ui::list_column> columns) : ui::list_view(theme)
		{
			set_columns(std::move(columns));
		}

		void set_rows(std::vector<sysmate::ui::row> rows)
		{
			_model.set_rows(std::move(rows));
			rebuild_items();
			if (_view_extent.cx > 0) layout_list();
			invalidate();
		}

		void set_filter(std::string filter)
		{
			_model.set_filter(std::move(filter));
			rebuild_items();
			if (_view_extent.cx > 0) layout_list();
			invalidate();
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
			if (_items.empty()) return false;
			select_position(0);
			return true;
		}

		[[nodiscard]] int row_count_for_test() const { return static_cast<int>(_items.size()); }
		[[nodiscard]] std::shared_ptr<void> payload_for_test(const int index) const
		{
			if (index < 0 || index >= static_cast<int>(_items.size())) return nullptr;
			const auto tag = std::static_pointer_cast<row_tag>(_items[index]->data);
			return tag ? tag->payload : nullptr;
		}
		[[nodiscard]] bool header_drag_active_for_test() const { return _drag.active(); }

		void set_on_selection(std::function<void(std::shared_ptr<void>)> callback)
		{
			_on_selection = std::move(callback);
		}
		void set_popup(std::function<std::vector<ui::command_ptr>(std::shared_ptr<void>)> callback)
		{
			_popup = std::move(callback);
		}

		std::vector<ui::command_ptr> default_popup_menu() override
		{
			if (_popup) return _popup(selected_data());
			return ui::list_view::default_popup_menu();
		}

		void paint(render::draw_context& dc, const bool focused) const override
		{
			ui::list_view::paint(dc, focused);
			if (!_items.empty()) return;
			const auto face = render::make_font(_theme.list_font);
			const auto size = dc.measure_text(_empty_text, face, render::text_style::single_line,
			                                 _view_extent.cx, _view_extent.cy);
			const auto top = _header_height + std::max(0, (_view_extent.cy - _header_height - size.cy) / 2);
			dc.draw_text(_empty_text,
			             {std::max(0, (_view_extent.cx - size.cx) / 2), top,
			              _view_extent.cx, top + size.cy},
			             face, render::text_style::single_line,
			             core::color(_theme.dim_text, dc.colors.alpha),
			             core::color(_theme.tool_background, dc.colors.alpha));
		}


		bool on_key(const int key, const platform::key_state keys) override
		{
			const auto k = static_cast<char32_t>(key);
			if (k == platform::keys::HOME || k == platform::keys::END ||
				k == platform::keys::PRIOR || k == platform::keys::NEXT)
			{
				if (_items.empty()) return true;
				const auto row_height = std::max(1, _font_extent.cy + _theme.padding_y * 2);
				const auto page = std::max(1, _view_extent.cy / row_height);
				if (k == platform::keys::HOME) select_position(0);
				else if (k == platform::keys::END) select_position(static_cast<int>(_items.size()) - 1);
				else
				{
					const auto delta = k == platform::keys::PRIOR ? -page : page;
					const auto current = selected_index();
					select_position(current < 0 ? (delta < 0 ? 0 : page) : current + delta);
				}
				return true;
			}
			return ui::list_view::on_key(key, keys);
		}

		void on_mouse_down(const core::pointi loc, const platform::key_state keys) override
		{
			if (loc.y < _header_height)
			{
				if (const auto column = edge_column(loc.x); column >= 0)
				{
					take_focus(false);
					_drag.begin(column, loc.x);
					if (host()) host()->capture_pointer(true);
					return;
				}
			}
			ui::list_view::on_mouse_down(loc, keys);
		}

		void on_mouse_move(const core::pointi loc, const bool captured) override
		{
			if (_drag.active())
			{
				if (!captured)
				{
					_drag.capture_changed();
					return;
				}
				const auto delta = _drag.move(loc.x);
				if (delta != 0)
				{
					_columns[_drag.column].weight = std::max(
						sysmate::ui::scale(30, _theme.dpi_scale),
						_columns[_drag.column].weight + delta);
					layout_list();
				}
				return;
			}
			ui::list_view::on_mouse_move(loc, captured);
		}

		void on_mouse_up(const core::pointi loc, const platform::key_state keys) override
		{
			if (_drag.active())
			{
				_drag.capture_changed();
				if (host()) host()->capture_pointer(false);
				invalidate();
				return;
			}
			ui::list_view::on_mouse_up(loc, keys);
		}

		platform::cursor cursor_at(const core::pointi loc) const override
		{
			if (loc.y < _header_height && edge_column(loc.x) >= 0)
				return platform::cursor::left_right;
			return ui::list_view::cursor_at(loc);
		}

	protected:
		void on_sort_changed(const int column, const bool ascending) override
		{
			_model.sort_by(column, ascending);
			rebuild_items();
			if (_view_extent.cx > 0) layout_list();
		}

		void on_item_selected(const ui::list_item_ptr& item, const bool) override
		{
			const auto tag = item ? std::static_pointer_cast<row_tag>(item->data) : nullptr;
			_model.select_id(tag ? tag->id : 0);
			if (_on_selection) _on_selection(tag ? tag->payload : nullptr);
		}
	};

	class sysmate_app final : public ui::app, public ui::frame_host,
	                           public std::enable_shared_from_this<sysmate_app>
	{
		sysmate_theme _theme;
		platform::setting_file_ptr _settings;
		ui::frame_ptr _frame;
		std::shared_ptr<dashboard_element> _root;
		ui::control_ptr _menu_bar;
		std::vector<ui::command_ptr> _menus;
		ui::command_ptr _focus_filter_command;
		std::array<ui::button_ptr, sysmate::ui::tab_count> _tab_buttons;
		std::array<ui::button_ptr, 3> _action_buttons;
		ui::edit_ptr _filter_edit;

		sysmate::native::performance_monitor _monitor;
		sysmate::native::hardware_info _hardware;
		sysmate::native::performance_sample _sample;
		std::array<std::deque<float>, chart_count> _history;
		std::array<Chart, chart_count> _charts;
		std::array<core::surface_ptr, chart_count> _chart_surfaces;
		double _network_peak = 1024.0 * 1024.0;
		uint64_t _handle_peak = 10000;

		tab_id _tab = tab_id::performance;
		std::array<std::string, sysmate::ui::tab_count> _filters;
		std::shared_ptr<performance_element> _performance;
		std::shared_ptr<table_view> _process_view;
		std::shared_ptr<table_view> _module_view;
		std::shared_ptr<table_view> _network_view;
		std::shared_ptr<table_view> _window_view;
		std::shared_ptr<table_view> _property_view;
		std::shared_ptr<about_element> _about;

		bool _offscreen_test = false;
		bool _ui_test = false;
		bool _ui_test_done = false;
		int _offscreen_step = 0;
		uint64_t _offscreen_selected = 0;
		bool _offscreen_ok = true;
		std::vector<std::shared_ptr<process_info>> _ui_process_fixture;
		std::vector<std::shared_ptr<network_connection>> _ui_network_fixture;
		std::vector<std::shared_ptr<window_info>> _ui_window_fixture;
		double _last_sample = 0.0;
		bool _sampled_once = false;
		double _dpi_scale = 1.0;

		[[nodiscard]] bool has_filter() const
		{
			return _tab == tab_id::processes || _tab == tab_id::network || _tab == tab_id::windows;
		}

		[[nodiscard]] core::recti offset_rect(const sysmate::ui::rect& rect, const int top) const
		{
			return {rect.left, rect.top + top, rect.right, rect.bottom + top};
		}

		ui::command_ptr command(std::string_view text, std::function<void()> invoke,
		                       std::initializer_list<platform::keyboard_accelerator_t> kba = {})
		{
			auto result = std::make_shared<ui::command>();
			result->text = text;
			result->invoke = std::move(invoke);
			result->kba.assign(kba.begin(), kba.end());
			return result;
		}

		std::weak_ptr<sysmate_app> weak_self() { return std::enable_shared_from_this<sysmate_app>::weak_from_this(); }

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
					{process->name, std::format("{}", process->pid), format_bytes(process->working_set)}, process});
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
			if (const auto process = std::static_pointer_cast<process_info>(_process_view->selected_data()))
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
					 connection->state, std::format("{}", connection->pid), connection->process_name}, connection});
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
					{window->title.empty() ? window->class_name : window->title, window->class_name,
					 std::format("{}", window->pid), std::format("0x{:X}", window->window_id)}, window});
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

		void refresh_about() { _about->set_rows(sysmate::native::collect_system_summary(_hardware)); }

		void refresh_current()
		{
			switch (_tab)
			{
			case tab_id::performance: if (_frame) _frame->invalidate(); break;
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
				add("Rect", std::format("{}, {}, {}, {}", window->bounds.left, window->bounds.top,
					window->bounds.right, window->bounds.bottom));
				add("Size", std::format("{} x {}", window->bounds.right - window->bounds.left,
					window->bounds.bottom - window->bounds.top));
				add("Style", format_hex(window->style));
				add("ExStyle", format_hex(window->extended_style));
			}
			_property_view->set_rows(std::move(rows));
		}

		void show_error(const std::string_view text)
		{
			platform::message_box(nullptr, text, "SysMate",
				{platform::message_box_buttons::ok, platform::message_box_icon::warning});
		}

		void show_process(const std::shared_ptr<process_info>& process)
		{
			if (!process) return;
			if (process->path.empty())
			{
				show_error("The process path is unavailable.");
				return;
			}
			std::string error;
			if (!sysmate::native::show_process_in_explorer(*process, error)) show_error(error);
		}

		void kill_process(const std::shared_ptr<process_info>& process)
		{
			if (!process) return;
			const auto answer = platform::message_box(nullptr,
				std::format("Terminate {} (PID {})?\n\nUnsaved work in this process will be lost.",
					process->name, process->pid), "SysMate",
				{platform::message_box_buttons::ok_cancel, platform::message_box_icon::warning});
			if (answer != platform::message_box_result::ok) return;
			std::string error;
			if (!sysmate::native::terminate_process(*process, error)) show_error(error);
			refresh_processes();
		}

		void show_module(const std::shared_ptr<sysmate::native::module_info>& module)
		{
			if (!module || module->path.empty()) return;
			process_info process;
			process.path = module->path;
			std::string error;
			if (!sysmate::native::show_process_in_explorer(process, error)) show_error(error);
		}

		void show_window_process(const std::shared_ptr<window_info>& window)
		{
			if (!window) return;
			std::string error;
			if (!sysmate::native::show_window_process_in_explorer(*window, error)) show_error(error);
		}

		void close_window(const std::shared_ptr<window_info>& window)
		{
			if (!window) return;
			const auto title = window->title.empty() ? window->class_name : window->title;
			const auto answer = platform::message_box(nullptr, std::format("Close \"{}\"?", title), "SysMate",
				{platform::message_box_buttons::ok_cancel, platform::message_box_icon::warning});
			if (answer != platform::message_box_result::ok) return;
			std::string error;
			if (!sysmate::native::close_window(*window, error)) show_error(error);
		}

		void copy_about()
		{
			const auto rows = sysmate::native::collect_system_summary(_hardware);
			std::string text = "SysMate - System Information\r\n================================\r\n";
			for (const auto& [label, value] : rows) text += std::format("{}: {}\r\n", label, value);
			if (!platform::set_clipboard(text)) show_error("The clipboard is unavailable.");
		}

		void report_issue()
		{
			std::string error;
			if (!sysmate::native::open_issue_url(error)) show_error(error);
		}

		std::vector<ui::command_ptr> popup_for_process(const std::shared_ptr<void>& payload)
		{
			const auto process = std::static_pointer_cast<process_info>(payload);
			if (!process) return {};
			const auto weak = weak_self();
			auto show = command("Show in Explorer", [weak, process]
			{
				if (const auto self = weak.lock()) self->show_process(process);
			});
			show->enable = !process->path.empty();
			return {std::move(show), command("Kill Process", [weak, process]
			{
				if (const auto self = weak.lock()) self->kill_process(process);
			})};
		}

		std::vector<ui::command_ptr> popup_for_module(const std::shared_ptr<void>& payload)
		{
			const auto module = std::static_pointer_cast<sysmate::native::module_info>(payload);
			if (!module) return {};
			const auto weak = weak_self();
			return {command("Show in Explorer", [weak, module]
			{
				if (const auto self = weak.lock()) self->show_module(module);
			})};
		}

		std::vector<ui::command_ptr> popup_for_window(const std::shared_ptr<void>& payload)
		{
			const auto window = std::static_pointer_cast<window_info>(payload);
			if (!window) return {};
			const auto weak = weak_self();
			return {command("Show in Explorer", [weak, window]
			{
				if (const auto self = weak.lock()) self->show_window_process(window);
			}), command("Close Window", [weak, window]
			{
				if (const auto self = weak.lock()) self->close_window(window);
			})};
		}


		void action(const int index)
		{
			switch (_tab)
			{
			case tab_id::processes:
				if (index == 0) refresh_processes();
				else if (index == 1) show_process(std::static_pointer_cast<process_info>(_process_view->selected_data()));
				else if (index == 2) kill_process(std::static_pointer_cast<process_info>(_process_view->selected_data()));
				break;
			case tab_id::network: if (index == 0) refresh_network(); break;
			case tab_id::windows:
				if (index == 0) refresh_windows();
				else if (index == 1) show_window_process(std::static_pointer_cast<window_info>(_window_view->selected_data()));
				else if (index == 2) close_window(std::static_pointer_cast<window_info>(_window_view->selected_data()));
				break;
			case tab_id::about:
				if (index == 0) report_issue();
				else if (index == 1) copy_about();
				else if (index == 2) refresh_about();
				break;
			case tab_id::performance: break;
			}
		}

		void update_visibility()
		{
			const auto process = _tab == tab_id::processes;
			const auto network = _tab == tab_id::network;
			const auto windows = _tab == tab_id::windows;
			const auto about = _tab == tab_id::about;
			_performance->is_visible(_tab == tab_id::performance);
			_process_view->is_visible(process);
			_module_view->is_visible(process);
			_network_view->is_visible(network);
			_window_view->is_visible(windows);
			_property_view->is_visible(windows);
			_about->is_visible(about);
			_filter_edit->is_visible(has_filter());
			if (_focus_filter_command) _focus_filter_command->enable = has_filter();
			for (int i = 0; i < sysmate::ui::tab_count; ++i)
				_tab_buttons[i]->set_checked(i == static_cast<int>(_tab));

			static constexpr std::array<std::array<std::string_view, 3>, 5> labels = {{
				{{"", "", ""}},
				{{"Refresh", "Open in Explorer", "Kill Process"}},
				{{"Refresh", "", ""}},
				{{"Refresh", "Open in Explorer", "Close Window"}},
				{{"Report an Issue", "Copy to Clipboard", "Refresh"}}}};
			const auto& current_labels = labels[static_cast<size_t>(_tab)];
			for (size_t i = 0; i < _action_buttons.size(); ++i)
			{
				_action_buttons[i]->window_text(current_labels[i]);
				_action_buttons[i]->is_visible(!current_labels[i].empty());
			}
		}

		void focus_current_page()
		{
			switch (_tab)
			{
			case tab_id::processes: _process_view->focus(); break;
			case tab_id::network: _network_view->focus(); break;
			case tab_id::windows: _window_view->focus(); break;
			case tab_id::performance:
			case tab_id::about:
				if (_frame) _frame->focus();
				break;
			}
		}

		void switch_tab(const int index)
		{
			if (index < 0 || index >= sysmate::ui::tab_count) return;
			const auto next = static_cast<tab_id>(index);
			if (next == _tab) return;
			if (_filter_edit && has_filter()) _filters[static_cast<size_t>(_tab)] = _filter_edit->window_text();
			_tab = next;
			if (_filter_edit) _filter_edit->window_text(_filters[static_cast<size_t>(_tab)]);
			update_visibility();
			if (_frame) _frame->layout();
			refresh_current();
			focus_current_page();
		}

		std::vector<ui::command_ptr> make_menu()
		{
			const auto weak = weak_self();
			auto refresh = command("&Refresh", [weak]
			{
				if (const auto self = weak.lock()) self->refresh_current();
			}, {{platform::keys::F5, 0}, {'R', platform::keyboard_accelerator_t::control}});
			auto quit = command("E&xit", [weak]
			{
				if (const auto self = weak.lock(); self && self->_frame) self->_frame->close();
			});
			auto file = std::make_shared<ui::command>();
			file->text = "&File";
			file->menu = [refresh, quit] { return std::vector<ui::command_ptr>{refresh, nullptr, quit}; };

			std::vector<ui::command_ptr> tabs;
			for (int i = 0; i < sysmate::ui::tab_count; ++i)
			{
				auto item = command(tab_names[i], [weak, i]
				{
					if (const auto self = weak.lock()) self->switch_tab(i);
				}, {{static_cast<char32_t>('1' + i), platform::keyboard_accelerator_t::control}});
				tabs.push_back(std::move(item));
			}
			auto next = command("Next Tab", [weak]
			{
				if (const auto self = weak.lock()) self->switch_tab((static_cast<int>(self->_tab) + 1) % 5);
			}, {{platform::keys::TAB, platform::keyboard_accelerator_t::control}});
			next->kba.push_back({platform::keys::NEXT, platform::keyboard_accelerator_t::control});
			auto previous = command("Previous Tab", [weak]
			{
				if (const auto self = weak.lock()) self->switch_tab((static_cast<int>(self->_tab) + 4) % 5);
			}, {{platform::keys::TAB, platform::keyboard_accelerator_t::control |
				platform::keyboard_accelerator_t::shift}});
			previous->kba.push_back({platform::keys::PRIOR, platform::keyboard_accelerator_t::control});
			auto focus_filter = command("Focus Filter", [weak]
			{
				if (const auto self = weak.lock(); self && self->has_filter() && self->_filter_edit)
				{
					self->_filter_edit->focus();
					self->_filter_edit->select_all();
				}
			}, {{'F', platform::keyboard_accelerator_t::control}});
			_focus_filter_command = focus_filter;
			focus_filter->enable = has_filter();
			tabs.push_back(nullptr);
			tabs.push_back(std::move(next));
			tabs.push_back(std::move(previous));
			tabs.push_back(std::move(focus_filter));
			auto view = std::make_shared<ui::command>();
			view->text = "&View";
			view->menu = [tabs] { return tabs; };

			auto issue = command("Report an Issue", [weak]
			{
				if (const auto self = weak.lock()) self->report_issue();
			});
			auto help = std::make_shared<ui::command>();
			help->text = "&Help";
			help->menu = [issue] { return std::vector<ui::command_ptr>{issue}; };
			return {std::move(file), std::move(view), std::move(help)};
		}

		void layout_root(render::measure_context& mc, const core::recti bounds)
		{
			const auto menu_extent = _menu_bar->measure(mc, bounds.width());
			const auto menu_height = std::max(1, menu_extent.cy);
			_menu_bar->layout(mc, {0, 0, bounds.right, menu_height});
			const auto content_height = std::max(0, bounds.height() - menu_height);
			const auto l = sysmate::ui::make_layout({bounds.width(), content_height}, _theme.dpi_scale, _tab);
			const auto tab_width = l.tabs.width() / sysmate::ui::tab_count;
			for (int i = 0; i < sysmate::ui::tab_count; ++i)
			{
				const auto right = i == sysmate::ui::tab_count - 1 ? l.tabs.right : (i + 1) * tab_width;
				_tab_buttons[i]->layout(mc, offset_rect({i * tab_width, l.tabs.top, right, l.tabs.bottom}, menu_height));
			}

			_performance->layout(mc, offset_rect(l.page, menu_height));
			_process_view->layout(mc, offset_rect(l.primary_pane, menu_height));
			_module_view->layout(mc, offset_rect(l.secondary_pane, menu_height));
			_network_view->layout(mc, offset_rect(l.page, menu_height));
			_window_view->layout(mc, offset_rect(l.primary_pane, menu_height));
			_property_view->layout(mc, offset_rect(l.secondary_pane, menu_height));
			_about->layout(mc, offset_rect(l.page, menu_height));
			_filter_edit->layout(mc, offset_rect(l.filter, menu_height));
			for (size_t i = 0; i < _action_buttons.size() && i < l.buttons.size(); ++i)
				_action_buttons[i]->layout(mc, offset_rect(l.buttons[i], menu_height));
			for (size_t i = l.buttons.size(); i < _action_buttons.size(); ++i)
				_action_buttons[i]->layout(mc, {0, 0, 0, 0});
		}

		void paint_performance(render::draw_context& dc, const core::recti bounds)
		{
			dc.draw_rect(bounds, core::color(_theme.window_background, dc.colors.alpha));
			const auto scale = _theme.dpi_scale;
			const auto pad = sysmate::ui::scale(4, scale);
			const auto spacing = sysmate::ui::scale(8, scale);
			const auto columns = bounds.width() > sysmate::ui::scale(500, scale) ? 2 : 1;
			const auto rows = (chart_count + columns - 1) / columns;
			const auto chart_width = bounds.width() / columns - spacing;
			const auto chart_height = bounds.height() / rows - spacing;
			if (chart_width < sysmate::ui::scale(50, scale) || chart_height < sysmate::ui::scale(30, scale)) return;

			const auto text_margin = sysmate::ui::scale(16, scale);
			const auto text_top = sysmate::ui::scale(8, scale);
			const auto text_bottom = sysmate::ui::scale(34, scale);
			const auto detail_bottom = sysmate::ui::scale(50, scale);
			const auto value_margin = sysmate::ui::scale(160, scale);
			const auto face = render::make_font(_theme.list_font);
			static constexpr ChartColor palette[chart_count][2] = {
				{{0, 180, 255}, {0, 120, 200}}, {{180, 0, 255}, {140, 0, 200}},
				{{0, 200, 100}, {0, 150, 80}}, {{255, 180, 0}, {200, 140, 0}},
				{{255, 80, 80}, {200, 50, 50}}, {{100, 220, 220}, {60, 170, 170}}};

			for (int i = 0; i < chart_count; ++i)
			{
				const auto column = i % columns;
				const auto row = i / columns;
				const auto x = bounds.left + pad + column * (chart_width + spacing);
				const auto y = bounds.top + pad + row * (chart_height + spacing);
				auto& chart = _charts[i];
				chart.resize(chart_width, chart_height);
				ChartStyle style;
				style.lineColor = palette[i][0];
				style.fillColor = palette[i][1];
				chart.set_style(style);
				chart.set_margin(sysmate::ui::scale(8, scale));
				chart.set_data(_history[i]);
				chart.render();
				if (!_chart_surfaces[i] || _chart_surfaces[i]->dimensions() != core::sizei(chart.width(), chart.height()))
				{
					_chart_surfaces[i] = std::make_shared<core::surface>();
					_chart_surfaces[i]->alloc(chart.width(), chart.height(), core::texture_format::ARGB);
				}
				for (int py = 0; py < chart.height(); ++py)
					std::memcpy(_chart_surfaces[i]->pixels_line(py),
					            chart.pixels().data() + static_cast<size_t>(py) * chart.width(),
					            static_cast<size_t>(chart.width()) * sizeof(uint32_t));
				render::draw_surface(dc, _chart_surfaces[i], {x, y, x + chart_width, y + chart_height},
				                     1.0f, render::texture_sampler::point);

				const auto text_color = core::color(_theme.text, dc.colors.alpha);
				const auto dim_color = core::color(_theme.dim_text, dc.colors.alpha);
				dc.draw_text(chart_names[i], {x + text_margin, y + text_top, x + chart_width - spacing, y + text_bottom},
				             face, render::text_style::single_line, text_color, core::color(0, 0, 0, 0));
				std::string value;
				if (i == 3) value = format_rate(_sample.network_bytes_per_second);
				else if (i == 5) value = std::format("{}", _sample.handle_count);
				else if (i == 4 && !_sample.gpu_available) value = "n/a";
				else
				{
					const auto latest = _history[i].empty() ? 0.0f : _history[i].back();
					value = std::format("{:.1f}%", latest);
				}
				dc.draw_text(value, {x + chart_width - value_margin, y + text_top,
				                     x + chart_width - text_margin, y + text_bottom},
				             face, render::text_style::single_line_far, text_color, core::color(0, 0, 0, 0));

				std::string detail;
				switch (i)
				{
				case 0:
					detail = _hardware.cpu_name.empty()
					             ? std::format("{} logical cores @ {:.2f} GHz", _hardware.logical_cores, _hardware.cpu_mhz / 1000.0)
					             : std::format("{}  ({} cores @ {:.2f} GHz)", _hardware.cpu_name, _hardware.logical_cores,
					                           _hardware.cpu_mhz / 1000.0);
					break;
				case 1:
					detail = std::format("Used {} / {}  (Available {})",
						format_bytes(_sample.memory_total_bytes - _sample.memory_available_bytes),
						format_bytes(_sample.memory_total_bytes), format_bytes(_sample.memory_available_bytes));
					break;
				case 2:
					detail = std::format("{}:  Used {} / {}  (Free {})", _hardware.system_drive,
						format_bytes(_sample.disk_total_bytes - _sample.disk_free_bytes),
						format_bytes(_sample.disk_total_bytes), format_bytes(_sample.disk_free_bytes));
					break;
				case 3: detail = "Scale " + format_rate(_network_peak) + " (peak seen)"; break;
				case 4: detail = _sample.gpu_available
					             ? (_hardware.gpu_name.empty() ? "Busiest engine" : _hardware.gpu_name)
					             : (_hardware.gpu_name.empty() ? "GPU counters unavailable" : _hardware.gpu_name);
					break;
				case 5: detail = "Total system handles"; break;
				default: break;
				}
				if (!detail.empty())
					dc.draw_text(detail, {x + text_margin, y + text_bottom, x + chart_width - spacing, y + detail_bottom},
					             face, render::text_style::single_line, dim_color, core::color(0, 0, 0, 0));
			}
		}

		void sample_performance()
		{
			_sample = _monitor.sample();
			if (_sample.network_bytes_per_second > _network_peak) _network_peak = _sample.network_bytes_per_second * 1.2;
			if (_sample.handle_count > _handle_peak) _handle_peak = static_cast<uint64_t>(_sample.handle_count * 1.2);
			const std::array<float, chart_count> values = {
				static_cast<float>(std::clamp(_sample.cpu_percent, 0.0, 100.0)),
				static_cast<float>(_sample.memory_load),
				static_cast<float>(std::clamp(_sample.disk_percent, 0.0, 100.0)),
				static_cast<float>(std::clamp(_sample.network_bytes_per_second / _network_peak * 100.0, 0.0, 100.0)),
				static_cast<float>(std::clamp(_sample.gpu_percent, 0.0, 100.0)),
				static_cast<float>(std::clamp(static_cast<double>(_sample.handle_count) /
				                              std::max<uint64_t>(1, _handle_peak) * 100.0, 0.0, 100.0))};
			for (int i = 0; i < chart_count; ++i)
			{
				_history[i].push_back(values[i]);
				while (_history[i].size() > 60) _history[i].pop_front();
			}
			if (_frame) _frame->invalidate();
		}

		void offscreen_check(const bool condition, const std::string_view message)
		{
			if (!condition) _offscreen_ok = false;
			platform::write_stdout(std::format("offscreen: {} {}\n", condition ? "PASS" : "FAIL", message));
		}

		void run_offscreen_step()
		{
			switch (_offscreen_step++)
			{
			case 0:
				switch_tab(static_cast<int>(tab_id::processes));
				offscreen_check(_process_view->select_first_for_test(), "process selection is reachable");
				_offscreen_selected = _process_view->selected_id();
				break;
			case 1:
				refresh_processes();
				offscreen_check(_offscreen_selected != 0 && _process_view->selected_id() == _offscreen_selected,
				                "selection identity survives refresh");
				break;
			case 2:
				switch_tab(static_cast<int>(tab_id::network));
				offscreen_check(_network_view != nullptr, "network view survives tab switch");
				break;
			case 3:
				switch_tab(static_cast<int>(tab_id::windows));
				offscreen_check(_window_view != nullptr, "window view survives tab switch");
				break;
			case 4:
				switch_tab(static_cast<int>(tab_id::about));
				offscreen_check(_about != nullptr, "about view survives tab switch");
				break;
			case 5:
				switch_tab(static_cast<int>(tab_id::performance));
				offscreen_check(_performance != nullptr, "chart view survives tab switch");
				break;
			default:
				platform::write_stdout(std::format("offscreen: lifecycle {}\n", _offscreen_ok ? "passed" : "FAILED"));
				if (_frame) _frame->close();
				_offscreen_step = -1;
				break;
			}
		}

		void ui_test_check(const bool condition, const std::string_view message)
		{
			if (!condition) _offscreen_ok = false;
			platform::write_stdout(std::format("ui-test: {} {}\n", condition ? "PASS" : "FAIL", message));
		}

		void run_ui_test()
		{
			switch_tab(static_cast<int>(tab_id::processes));
			ui_test_check(_process_view->bounds.width() > 0 && _process_view->bounds.height() > 0,
			              "real process client geometry");
			ui_test_check(_process_view->row_count_for_test() == 2, "deterministic fixture rows");

			ui::test::fake_draw_context draw;
			_process_view->paint(draw, false);
			ui_test_check(!draw.texts.empty() && !draw.fills.empty(), "actual list view records drawing");

			_process_view->on_mouse_down({10, 2}, {});
			const auto sorted = std::static_pointer_cast<process_info>(_process_view->payload_for_test(0));
			ui_test_check(sorted && sorted->pid == 41002, "actual header click sorts fixture rows");

			ui_test_check(_process_view->select_first_for_test(), "actual list selection callback");
			const auto selected_payload = _process_view->selected_data();
			const auto selected_id = _process_view->selected_id();
			refresh_processes();
			ui_test_check(selected_id != 0 && _process_view->selected_id() == selected_id &&
			             _process_view->selected_data() == selected_payload,
			             "refresh preserves payload identity");

			const auto columns = _process_view->column_rects(_process_view->bounds.width());
			if (!columns.empty())
			{
				_process_view->on_mouse_down({columns.front().right, 2}, {});
				ui_test_check(_process_view->header_drag_active_for_test(), "header drag captures");
				_process_view->on_mouse_move({columns.front().right + 10, 2}, false);
				ui_test_check(!_process_view->header_drag_active_for_test(), "capture loss cancels header drag");
			}

			// An offscreen window never takes the keyboard, so the focus is the control its input manager holds.
			const auto input = _filter_edit->input();
			_filter_edit->window_text({});
			_filter_edit->focus();
			_filter_edit->on_text("\xC3\xA9");
			_filter_edit->on_text("\xE7\x8C\xAB");
			ui_test_check(_filter_edit->window_text() == "\xC3\xA9\xE7\x8C\xAB",
			              "real filter accepts UTF-8 input");
			ui_test_check(input && input->focused() == _filter_edit, "filter takes focus");

			_filter_edit->on_key('A', {true, false, false});
			_filter_edit->on_text("x");
			ui_test_check(_filter_edit->window_text() == "x", "filter Ctrl+A selects UTF-8 text");

			_filter_edit->window_text("\xC3\xA9\xE7\x8C\xAB");
			_filter_edit->on_key('A', {true, false, false});
			_filter_edit->on_key('C', {true, false, false});
			const auto clipboard = platform::clipboard_text();
			_filter_edit->window_text({});
			_filter_edit->on_key('V', {true, false, false});
			ui_test_check(!clipboard || _filter_edit->window_text() == "\xC3\xA9\xE7\x8C\xAB",
			              "real filter Ctrl+A clipboard round trip");

			// Tab goes as the frame sends it: to the application, which leaves it, then to the controls.
			const auto tab = [this, &input]
			{
				return !key_down(platform::keys::TAB, {}) && input && input->key_down(platform::keys::TAB, {});
			};
			ui_test_check(tab() && input->focused() == _process_view, "filter Tab routes focus to table");
			ui_test_check(tab() && input->focused() == _module_view, "table Tab routes focus to modules");
			ui_test_check(tab() && input->focused() == _action_buttons[0], "modules Tab routes focus to actions");

			ui::test::fake_measure_context zero_metrics;
			_root->layout(zero_metrics, {0, 0, 0, 0});
			ui_test_check(true, "zero-extent layout is handled");
			if (_frame) _frame->layout();

			platform::write_stdout(std::format("ui-test: lifecycle {}\n", _offscreen_ok ? "passed" : "FAILED"));
			if (_frame) _frame->close();
			_ui_test_done = true;
		}

		void rebuild_filter_edit()
		{
			const auto old = _filter_edit;
			const auto text = old ? old->window_text() : std::string{};
			const auto was_focused = old && old->has_focus();
			ui::edit_styles styles;
			styles.font = render::make_font(_theme.edit_font);
			styles.rounded_corners = false;
			_filter_edit = ui::make_edit(styles, text, [this](const std::string& value)
			{
				_filters[static_cast<size_t>(_tab)] = value;
				refresh_current();
			});
			_filter_edit->set_background(_theme.tool_background);
			_filter_edit->traits.forwards_unhandled_keys = true;
			if (_root && old)
				for (auto& child : _root->children)
					if (child == old) child = _filter_edit;
			if (was_focused) _filter_edit->focus();
		}

		void paint_tabs(render::draw_context& dc, const core::recti) const
		{
			const auto face = render::make_font(_theme.list_font);
			for (int i = 0; i < sysmate::ui::tab_count; ++i)
			{
				const auto rect = _tab_buttons[i]->bounds;
				const auto selected = i == static_cast<int>(_tab);
				dc.draw_rect(rect, core::color(selected ? _theme.handle : _theme.window_background, dc.colors.alpha));
				if (selected)
					dc.draw_rect({rect.left, rect.top, rect.right,
					             rect.top + sysmate::ui::scale(2, _theme.dpi_scale)},
					             core::color(core::rgb(0, 150, 230), dc.colors.alpha));
				dc.draw_text(tab_names[i], rect, face, render::text_style::single_line_center,
				             core::color(selected ? _theme.text : _theme.dim_text, dc.colors.alpha),
				             core::color(0, 0, 0, 0));
			}
		}

		void build_ui()
		{
			_menus = make_menu();
			_menu_bar = ui::make_menu_bar(_menus);
			for (int i = 0; i < sysmate::ui::tab_count; ++i)
				_tab_buttons[i] = ui::make_check_button(i == 0, {}, true, [this, i](const bool checked)
				{
					if (checked) switch_tab(i);
				}, 1);
			for (size_t i = 0; i < _action_buttons.size(); ++i)
				_action_buttons[i] = ui::make_button("", [this, i] { action(static_cast<int>(i)); });

			rebuild_filter_edit();

			_process_view = std::make_shared<table_view>(_theme, std::vector<ui::list_column>{
				{"Name", 200}, {"PID", 70, true}, {"Memory", 100, true}});
			_module_view = std::make_shared<table_view>(_theme, std::vector<ui::list_column>{
				{"Module", 180}, {"Path", 300}, {"Size", 80, true}});
			_network_view = std::make_shared<table_view>(_theme, std::vector<ui::list_column>{
				{"Proto", 60}, {"Local Address", 190}, {"Remote Address", 190},
				{"State", 110}, {"PID", 70, true}, {"Process", 160}});
			_window_view = std::make_shared<table_view>(_theme, std::vector<ui::list_column>{
				{"Title", 250}, {"Class", 150}, {"PID", 70, true}, {"Handle", 100}});
			_property_view = std::make_shared<table_view>(_theme, std::vector<ui::list_column>{{"Property", 120}, {"Value", 300}});
			_module_view->set_empty_text("Modules unavailable (access denied)");

			const auto weak = weak_self();
			_process_view->set_on_selection([weak](std::shared_ptr<void> data)
			{
				if (const auto self = weak.lock()) self->process_selected(data);
			});
			_process_view->set_popup([weak](std::shared_ptr<void> data)
			{
				if (const auto self = weak.lock()) return self->popup_for_process(data);
				return std::vector<ui::command_ptr>{};
			});
			_module_view->set_popup([weak](std::shared_ptr<void> data)
			{
				if (const auto self = weak.lock()) return self->popup_for_module(data);
				return std::vector<ui::command_ptr>{};
			});
			_window_view->set_on_selection([weak](std::shared_ptr<void> data)
			{
				if (const auto self = weak.lock()) self->window_selected(data);
			});
			_window_view->set_popup([weak](std::shared_ptr<void> data)
			{
				if (const auto self = weak.lock()) return self->popup_for_window(data);
				return std::vector<ui::command_ptr>{};
			});

			_performance = std::make_shared<performance_element>([this](render::draw_context& dc, core::recti bounds)
			{
				paint_performance(dc, bounds);
			});
			_about = std::make_shared<about_element>(_theme, [this]
			{
				if (_frame) _frame->invalidate();
			});

			_root = std::make_shared<dashboard_element>();
			_root->children = {_menu_bar};
			for (const auto& button : _tab_buttons) _root->children.push_back(button);
			_root->children.push_back(_filter_edit);
			_root->children.push_back(_performance);
			_root->children.push_back(_process_view);
			_root->children.push_back(_module_view);
			_root->children.push_back(_network_view);
			_root->children.push_back(_window_view);
			_root->children.push_back(_property_view);
			_root->children.push_back(_about);
			for (const auto& button : _action_buttons) _root->children.push_back(button);
			_root->paint_decoration = [this](render::draw_context& dc, const core::recti bounds)
			{
				paint_tabs(dc, bounds);
			};
			_root->layout_children = [this](render::measure_context& mc, const core::recti bounds)
			{
				layout_root(mc, bounds);
			};
			update_visibility();
		}

	public:
		bool pre_init() override { return true; }

		bool parse_command_line(const std::string_view command_line) override
		{
			_offscreen_test = command_line.find("--test-offscreen") != std::string_view::npos ||
			                  command_line.find("/test-offscreen") != std::string_view::npos;
			_ui_test = command_line.find("--test-ui") != std::string_view::npos ||
			           command_line.find("/test-ui") != std::string_view::npos;
			return false;
		}

		int run_console_mode() override { return 0; }

		bool load_settings(const platform::setting_file_ptr& store) override
		{
			_settings = platform::create_ini_file_settings(
				platform::known_path(platform::known_folder::running_app_folder));
			if (!_settings) _settings = store;
			return _settings != nullptr;
		}

		bool wants_gpu() const override { return true; }

		bool init(const std::string_view command_line) override
		{
			parse_command_line(command_line);
			const auto initial_scale = std::max(1.0, platform::primary_screen_dpi() / 96.0);
			_dpi_scale = initial_scale;
			_theme.set_dpi(initial_scale);
			for (auto& history : _history) history.assign(60, 0.0f);
			_monitor.initialize();
			_hardware = _monitor.hardware();

			ui::frame_style style;
			style.drawn_caption = false;
			style.hardware_accelerated = wants_gpu();
			style.client_size = {sysmate::ui::scale(1000, initial_scale),
			                      sysmate::ui::scale(650, initial_scale)};
			// The native window takes physical pixels, so install the minimum at the
			// monitor scale rather than letting high-DPI windows become too small.
			style.minimum_client = {sysmate::ui::scale(560, initial_scale),
			                         sysmate::ui::scale(400, initial_scale)};
			style.offscreen = _offscreen_test || _ui_test;
			style.resizable = true;
			style.colors = {_theme.window_background, _theme.text, _theme.focus_handle};
			_frame = ui::create_app_frame(shared_from_this(), _settings, shared_from_this(), style);
			if (!_frame) return false;

			_dpi_scale = _frame->scale_factor();
			_theme.set_dpi(_dpi_scale);
			build_ui();

			const auto summary = sysmate::native::collect_system_summary(_hardware);
			const auto computer = std::find_if(summary.begin(), summary.end(),
				[](const auto& item) { return item.first == "Computer Name"; });
			_frame->window_text(computer == summary.end() ? "SysMate" : "SysMate - " + computer->second);
			_frame->layout();
			initialize_ui_fixture();
			refresh_current();
			platform::set_frame_delay(50);
			return true;
		}

		ui::focus_mode focus_mode() const override
		{
			return _filter_edit && _filter_edit->has_focus() ? ui::focus_mode::text_edit : ui::focus_mode::view;
		}

		bool text_input(std::string_view) override { return false; }

		void track_menu(const ui::frame_ptr& parent, const core::recti bounds,
		               const std::vector<ui::command_ptr>& commands) override
		{
			if (parent) parent->track_menu(bounds, commands);
		}

		bool key_down(const char32_t key, const platform::key_state keys) override
		{
			if (_tab == tab_id::about && _about && !keys.control && _about->key_down(static_cast<int>(key)))
				return true;
			if (ui::dispatch_accelerator(_menus, key, keys)) return true;
			if (keys.control && !keys.shift && !keys.alt && key == 'F' && has_filter())
			{
				_filter_edit->focus();
				_filter_edit->select_all();
				return true;
			}
			if (_frame && (key == platform::keys::LEFT || key == platform::keys::RIGHT))
			{
				const auto focused = _frame->focused_control();
				for (int i = 0; i < sysmate::ui::tab_count; ++i)
					if (focused == _tab_buttons[i])
					{
						switch_tab((static_cast<int>(_tab) +
							(key == platform::keys::RIGHT ? 1 : 4)) % sysmate::ui::tab_count);
						return true;
					}
			}
			return false;
		}

		bool key_down(const int key, const platform::key_state keys) override
		{
			return key_down(static_cast<char32_t>(key), keys);
		}

		void dpi_changed() override
		{
			if (!_frame) return;
			const auto scale = _frame->scale_factor();
			if (std::abs(scale - _dpi_scale) < 0.001) return;
			_dpi_scale = scale;
			_theme.set_dpi(scale);
			// Edit owns its font face, so it must be recreated; list views refresh
			// their cached face through control::scale_changed before this callback.
			rebuild_filter_edit();
			update_visibility();
			_frame->options_changed();
			_frame->invalidate();
		}

		void on_window_layout(render::measure_context& mc, const core::sizei extent,
		                     const bool) override
		{
			if (_root) _root->layout(mc, core::recti(extent));
		}

		void on_window_paint(render::draw_context& dc) override
		{
			if (_root) _root->render(dc, {});
		}

		void visit_elements(const std::function<void(const ui::element_ptr&, core::pointi)>& visit) override
		{
			if (_root) visit(_root, {});
		}

		void on_mouse_wheel(const core::pointi loc, const int delta, const platform::key_state,
		                   bool& was_handled) override
		{
			if (_root && _root->wheel(loc, {}, delta))
			{
				was_handled = true;
				if (_frame) _frame->invalidate();
			}
		}

		void on_window_destroy() override
		{
			_frame.reset();
			_root.reset();
		}

		void frame_tick() override
		{
			const auto now = platform::monotonic_seconds();
			if (!_sampled_once || now - _last_sample >= 1.0)
			{
				_sampled_once = true;
				_last_sample = now;
				sample_performance();
			}
			if (_ui_test && !_ui_test_done)
				run_ui_test();
			else if (_offscreen_test && _offscreen_step >= 0)
				run_offscreen_step();
		}

		void final_exit() override
		{
			platform::set_frame_delay(0);
			if (_ui_test)
				platform::write_stdout(std::format("ui-test: weak expired {}\n",
				                                  _root == nullptr ? "passed" : "FAILED"));
		}
	};
}

int main(int argc, char** argv)
{
	core::set_app_name("sys-mate");
	auto app = std::make_shared<sysmate_app>();
	return platform::run(app, argc, argv);
}
