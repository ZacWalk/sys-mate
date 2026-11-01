#pragma once

// Headless state and geometry shared by SysMate's platform-backed views.
//
// The window layer owns painting and input dispatch.  This file deliberately
// contains only standard C++ so sorting, filtering, identity preservation and
// keyboard/layout rules can be tested without creating a desktop window.

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sysmate::ui
{
	enum class tab_id
	{
		performance,
		processes,
		network,
		windows,
		about,
	};

	inline constexpr int tab_count = 5;

	struct point
	{
		int x = 0;
		int y = 0;
	};

	struct rect
	{
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;

		[[nodiscard]] constexpr int width() const { return right - left; }
		[[nodiscard]] constexpr int height() const { return bottom - top; }
		[[nodiscard]] constexpr bool contains(const point p) const
		{
			return p.x >= left && p.x < right && p.y >= top && p.y < bottom;
		}
	};

	struct extent
	{
		int width = 0;
		int height = 0;
	};

	struct layout
	{
		rect tabs;
		rect page;
		rect bottom_bar;
		rect filter;
		std::vector<rect> buttons;
		rect primary_pane;
		rect secondary_pane;
		bool filter_visible = false;
	};

	inline int scale(const int value, const double dpi)
	{
		const auto result = static_cast<double>(value) * dpi;
		return static_cast<int>(result + (result >= 0.0 ? 0.5 : -0.5));
	}

	inline layout make_layout(const extent client, const double dpi, const tab_id tab)
	{
		const auto tab_height = scale(30, dpi);
		const auto bottom_height = tab == tab_id::performance ? 0 : scale(34, dpi);
		const auto button_width = scale(130, dpi);
		const auto button_height = scale(26, dpi);
		const auto button_spacing = scale(6, dpi);
		const auto filter_max_width = scale(220, dpi);
		const auto splitter = scale(4, dpi);

		layout result;
		result.tabs = {0, 0, client.width, std::min(client.height, tab_height)};
		result.bottom_bar = {0, std::max(0, client.height - bottom_height),
		                    client.width, client.height};
		result.page = {0, tab_height, client.width,
		              std::max(tab_height, client.height - bottom_height)};

		const int button_count =
			tab == tab_id::performance ? 0 :
			tab == tab_id::processes ? 3 :
			tab == tab_id::network ? 1 :
			tab == tab_id::windows ? 3 : 3;
		const auto buttons_left = client.width -
			button_count * (button_width + button_spacing);
		const auto button_y = result.bottom_bar.top +
			(bottom_height - button_height) / 2;

		result.buttons.reserve(button_count);
		for (int i = 0; i < button_count; ++i)
		{
			const auto x = buttons_left + i * (button_width + button_spacing);
			result.buttons.push_back({x, button_y, x + button_width, button_y + button_height});
		}

		result.filter_visible = tab == tab_id::processes || tab == tab_id::network ||
			tab == tab_id::windows;
		if (result.filter_visible)
		{
			const auto filter_width = std::clamp(buttons_left - button_spacing * 2,
			                                    0, filter_max_width);
			result.filter = {button_spacing, button_y,
			                button_spacing + filter_width, button_y + button_height};
		}

		if (tab == tab_id::processes || tab == tab_id::windows)
		{
			const auto split = client.width / 2;
			result.primary_pane = {0, tab_height, split, std::max(tab_height, client.height - bottom_height)};
			result.secondary_pane = {split + splitter, tab_height, client.width,
			                         std::max(tab_height, client.height - bottom_height)};
		}
		else
		{
			result.primary_pane = result.page;
			result.secondary_pane = {};
		}

		return result;
	}

	inline int tab_at(const layout& l, const point p)
	{
		if (!l.tabs.contains(p)) return -1;
		const auto width = l.tabs.width();
		if (width <= 0) return -1;
		return std::clamp(p.x * tab_count / width, 0, tab_count - 1);
	}

	inline int button_at(const layout& l, const point p)
	{
		for (size_t i = 0; i < l.buttons.size(); ++i)
			if (l.buttons[i].contains(p)) return static_cast<int>(i);
		return -1;
	}

	inline int compare_folded(const std::string_view left, const std::string_view right)
	{
		const auto fold = [](const unsigned char c) -> unsigned char
		{
			return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
		};

		const auto count = std::min(left.size(), right.size());
		for (size_t i = 0; i < count; ++i)
		{
			const auto l = fold(static_cast<unsigned char>(left[i]));
			const auto r = fold(static_cast<unsigned char>(right[i]));
			if (l < r) return -1;
			if (l > r) return 1;
		}
		if (left.size() < right.size()) return -1;
		if (left.size() > right.size()) return 1;
		return 0;
	}

	inline bool leading_number(const std::string_view text, double& value)
	{
		if (text.empty()) return false;
		const auto* first = text.data();
		const auto* last = text.data() + text.size();
		const auto result = std::from_chars(first, last, value,
		                                    std::chars_format::general);
		return result.ec == std::errc{} && result.ptr != first && std::isfinite(value);
	}

	// Numeric-aware comparison used by all tables.  A cell is classified on its
	// own, so NaN/Infinity-like names remain ordinary text and the comparator is
	// a strict weak ordering for every mixture of rows.
	inline int compare_cells(const std::string_view left, const std::string_view right)
	{
		double left_number = 0.0;
		double right_number = 0.0;
		const bool left_is_number = leading_number(left, left_number);
		const bool right_is_number = leading_number(right, right_number);

		if (left_is_number != right_is_number)
			return left_is_number ? -1 : 1;
		if (left_is_number && left_number != right_number)
			return left_number < right_number ? -1 : 1;
		return compare_folded(left, right);
	}

	inline bool contains_folded(const std::string_view text, const std::string_view needle)
	{
		if (needle.empty()) return true;
		if (needle.size() > text.size()) return false;

		for (size_t start = 0; start + needle.size() <= text.size(); ++start)
		{
			if (compare_folded(text.substr(start, needle.size()), needle) == 0)
				return true;
		}
		return false;
	}

	struct row
	{
		uint64_t id = 0;
		std::vector<std::string> cells;
		std::shared_ptr<void> data;
	};

	// A table's model is separate from the platform list view.  The latter can
	// repaint or reorder rows, while this object keeps the domain identity alive
	// through refreshes and filters.
	class table_model
	{
		std::vector<row> _all_rows;
		std::vector<row> _visible_rows;
		std::string _filter;
		int _sort_column = -1;
		bool _sort_ascending = true;
		uint64_t _selected_id = 0;

		[[nodiscard]] bool matches(const row& item) const
		{
			if (_filter.empty()) return true;
			for (const auto& cell : item.cells)
				if (contains_folded(cell, _filter)) return true;
			return false;
		}

		void rebuild_visible()
		{
			_visible_rows.clear();
			for (const auto& item : _all_rows)
				if (matches(item)) _visible_rows.push_back(item);

			if (_sort_column >= 0)
			{
				const auto column = _sort_column;
				const auto ascending = _sort_ascending;
				std::stable_sort(_visible_rows.begin(), _visible_rows.end(),
					[column, ascending](const row& left, const row& right)
					{
						const auto& lc = column < static_cast<int>(left.cells.size())
							                 ? left.cells[column] : std::string_view{};
						const auto& rc = column < static_cast<int>(right.cells.size())
							                 ? right.cells[column] : std::string_view{};
						const auto result = compare_cells(lc, rc);
						return ascending ? result < 0 : result > 0;
					});
			}

			if (_selected_id != 0 && std::none_of(_visible_rows.begin(), _visible_rows.end(),
				[this](const row& item) { return item.id == _selected_id; }))
				_selected_id = 0;
		}

	public:
		void set_rows(std::vector<row> rows)
		{
			_all_rows = std::move(rows);
			rebuild_visible();
		}

		void set_filter(std::string filter)
		{
			_filter = std::move(filter);
			rebuild_visible();
		}

		[[nodiscard]] const std::string& filter() const { return _filter; }
		[[nodiscard]] const std::vector<row>& rows() const { return _visible_rows; }
		[[nodiscard]] int sort_column() const { return _sort_column; }
		[[nodiscard]] bool sort_ascending() const { return _sort_ascending; }
		[[nodiscard]] uint64_t selected_id() const { return _selected_id; }

		void sort_by(const int column, const bool ascending)
		{
			_sort_column = column;
			_sort_ascending = ascending;
			rebuild_visible();
		}

		void select_id(const uint64_t id)
		{
			if (id == 0 || std::any_of(_visible_rows.begin(), _visible_rows.end(),
				[id](const row& item) { return item.id == id; }))
				_selected_id = id;
			else
				_selected_id = 0;
		}

		void select_index(const int index)
		{
			_selected_id = index >= 0 && index < static_cast<int>(_visible_rows.size())
			                 ? _visible_rows[index].id : 0;
		}

		void move_selection(const int delta)
		{
			if (_visible_rows.empty()) return;
			int index = 0;
			for (int i = 0; i < static_cast<int>(_visible_rows.size()); ++i)
				if (_visible_rows[i].id == _selected_id) { index = i; break; }
			index = std::clamp(index + delta, 0, static_cast<int>(_visible_rows.size()) - 1);
			select_index(index);
		}

		[[nodiscard]] const row* selected_row() const
		{
			for (const auto& item : _visible_rows)
				if (item.id == _selected_id) return &item;
			return nullptr;
		}
	};

	// Header dragging is a small state machine because native capture can be
	// lost when another window or system gesture takes the pointer.
	struct header_drag_state
	{
		int column = -1;
		int anchor_x = 0;

		[[nodiscard]] bool active() const { return column >= 0; }

		void begin(const int column_index, const int x)
		{
			column = column_index;
			anchor_x = x;
		}

		int move(const int x)
		{
			if (!active()) return 0;
			const auto delta = x - anchor_x;
			anchor_x = x;
			return delta;
		}

		void capture_changed()
		{
			column = -1;
			anchor_x = 0;
		}
	};

	enum class focus_target
	{
		tabs,
		filter,
		table,
		actions,
	};

	struct focus_model
	{
		focus_target target = focus_target::tabs;

		void advance(const bool reverse, const bool filter_visible,
		            const bool actions_visible = true)
		{
			std::vector<focus_target> order = {focus_target::tabs};
			if (filter_visible) order.push_back(focus_target::filter);
			order.push_back(focus_target::table);
			if (actions_visible) order.push_back(focus_target::actions);

			auto it = std::find(order.begin(), order.end(), target);
			auto index = it == order.end() ? 0 : static_cast<int>(it - order.begin());
			index += reverse ? -1 : 1;
			if (index < 0) index = static_cast<int>(order.size()) - 1;
			if (index >= static_cast<int>(order.size())) index = 0;
			target = order[index];
		}
	};
}
