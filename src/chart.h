#pragma once

// SysMate's domain chart rasterizer.  It owns no window or device context:
// rendering produces a top-down 32-bit bitmap that pf::draw_context displays.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct ChartColor
{
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;

	bool operator==(const ChartColor&) const = default;
};

struct ChartStyle
{
	ChartColor background = {30, 30, 30};
	ChartColor gridLine = {55, 55, 55};
	ChartColor lineColor = {0, 180, 255};
	ChartColor fillColor = {0, 120, 200};
	ChartColor textColor = {180, 180, 180};
	ChartColor axisColor = {80, 80, 80};
	int fillAlpha = 60;
};

class Chart
{
	int _width = 0;
	int _height = 0;
	std::vector<uint32_t> _pixels;
	std::vector<float> _data;
	float _min_value = 0.0f;
	float _max_value = 100.0f;
	ChartStyle _style;
	int _grid_margin = 8;

	static constexpr uint32_t pack(const ChartColor color)
	{
		return static_cast<uint32_t>(color.r) |
			(static_cast<uint32_t>(color.g) << 8) |
			(static_cast<uint32_t>(color.b) << 16) | 0xff000000u;
	}

	static ChartColor unpack(const uint32_t color)
	{
		return {
			static_cast<uint8_t>(color & 0xff),
			static_cast<uint8_t>((color >> 8) & 0xff),
			static_cast<uint8_t>((color >> 16) & 0xff)};
	}

	void set_pixel(const int x, const int y, const ChartColor color)
	{
		if (x < 0 || x >= _width || y < 0 || y >= _height) return;
		_pixels[static_cast<size_t>(y) * _width + x] = pack(color);
	}

	[[nodiscard]] ChartColor get_pixel(const int x, const int y) const
	{
		if (x < 0 || x >= _width || y < 0 || y >= _height) return {};
		return unpack(_pixels[static_cast<size_t>(y) * _width + x]);
	}

	static ChartColor blend(const ChartColor destination, const ChartColor source, const int alpha)
	{
		return {
			static_cast<uint8_t>((source.r * alpha + destination.r * (255 - alpha)) / 255),
			static_cast<uint8_t>((source.g * alpha + destination.g * (255 - alpha)) / 255),
			static_cast<uint8_t>((source.b * alpha + destination.b * (255 - alpha)) / 255)};
	}

	void set_pixel_aa(const int x, const int y, const ChartColor color, const float alpha)
	{
		if (alpha <= 0.0f) return;
		if (alpha >= 1.0f)
		{
			set_pixel(x, y, color);
			return;
		}
		set_pixel(x, y, blend(get_pixel(x, y), color,
		                     static_cast<int>(alpha * 255.0f)));
	}

	void fill(const ChartColor color)
	{
		std::fill(_pixels.begin(), _pixels.end(), pack(color));
	}

	void draw_grid()
	{
		const auto margin = _grid_margin;
		for (int i = 1; i < 4; ++i)
		{
			const auto y = margin + (_height - 2 * margin) * i / 4;
			for (int x = margin; x < _width - margin; ++x)
				set_pixel(x, y, _style.gridLine);
		}

		const auto graph_width = _width - 2 * margin;
		for (int i = 1; i < 6; ++i)
		{
			const auto x = margin + graph_width * i / 6;
			for (int y = margin; y < _height - margin; ++y)
				set_pixel(x, y, _style.gridLine);
		}

		for (int x = margin; x < _width - margin; ++x)
		{
			set_pixel(x, margin, _style.axisColor);
			set_pixel(x, _height - margin - 1, _style.axisColor);
		}
		for (int y = margin; y < _height - margin; ++y)
		{
			set_pixel(margin, y, _style.axisColor);
			set_pixel(_width - margin - 1, y, _style.axisColor);
		}
	}

	void draw_aa_line(float x0, float y0, float x1, float y1, const ChartColor color)
	{
		const bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
		if (steep)
		{
			std::swap(x0, y0);
			std::swap(x1, y1);
		}
		if (x0 > x1)
		{
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		const auto dx = x1 - x0;
		const auto dy = y1 - y0;
		const auto gradient = dx == 0.0f ? 1.0f : dy / dx;

		auto xend = std::round(x0);
		auto yend = y0 + gradient * (xend - x0);
		auto xgap = 1.0f - (x0 + 0.5f - std::floor(x0 + 0.5f));
		const auto xpxl1 = static_cast<int>(xend);
		const auto ypxl1 = static_cast<int>(std::floor(yend));
		if (steep)
		{
			set_pixel_aa(ypxl1, xpxl1, color, (1.0f - (yend - ypxl1)) * xgap);
			set_pixel_aa(ypxl1 + 1, xpxl1, color, (yend - ypxl1) * xgap);
		}
		else
		{
			set_pixel_aa(xpxl1, ypxl1, color, (1.0f - (yend - ypxl1)) * xgap);
			set_pixel_aa(xpxl1, ypxl1 + 1, color, (yend - ypxl1) * xgap);
		}
		auto intery = yend + gradient;

		xend = std::round(x1);
		yend = y1 + gradient * (xend - x1);
		xgap = x1 + 0.5f - std::floor(x1 + 0.5f);
		const auto xpxl2 = static_cast<int>(xend);
		const auto ypxl2 = static_cast<int>(std::floor(yend));
		if (steep)
		{
			set_pixel_aa(ypxl2, xpxl2, color, (1.0f - (yend - ypxl2)) * xgap);
			set_pixel_aa(ypxl2 + 1, xpxl2, color, (yend - ypxl2) * xgap);
		}
		else
		{
			set_pixel_aa(xpxl2, ypxl2, color, (1.0f - (yend - ypxl2)) * xgap);
			set_pixel_aa(xpxl2, ypxl2 + 1, color, (yend - ypxl2) * xgap);
		}

		for (int x = xpxl1 + 1; x < xpxl2; ++x)
		{
			const auto iy = static_cast<int>(std::floor(intery));
			const auto fraction = intery - iy;
			if (steep)
			{
				set_pixel_aa(iy, x, color, 1.0f - fraction);
				set_pixel_aa(iy + 1, x, color, fraction);
			}
			else
			{
				set_pixel_aa(x, iy, color, 1.0f - fraction);
				set_pixel_aa(x, iy + 1, color, fraction);
			}
			intery += gradient;
		}
	}

	[[nodiscard]] float data_to_y(const float value) const
	{
		const auto margin = _grid_margin;
		const auto range = std::max(1.0f, _max_value - _min_value);
		const auto normal = std::clamp((value - _min_value) / range, 0.0f, 1.0f);
		return (_height - margin - 1) - normal * (_height - 2 * margin - 2);
	}

	[[nodiscard]] float data_to_x(const int index) const
	{
		const auto margin = _grid_margin;
		const auto count = static_cast<int>(_data.size());
		if (count <= 1) return static_cast<float>(_width / 2);
		return margin + static_cast<float>(index) / (count - 1) *
			(_width - 2 * margin - 2);
	}

	void draw_filled_area()
	{
		const auto margin = _grid_margin;
		const auto count = static_cast<int>(_data.size());
		const auto base_y = static_cast<float>(_height - margin - 1);
		for (int i = 0; i + 1 < count; ++i)
		{
			const auto x0 = data_to_x(i);
			const auto x1 = data_to_x(i + 1);
			const auto y0 = data_to_y(_data[i]);
			const auto y1 = data_to_y(_data[i + 1]);
			const auto ix0 = static_cast<int>(x0);
			const auto ix1 = static_cast<int>(x1);
			for (int x = ix0; x <= ix1; ++x)
			{
				const auto t = ix1 == ix0 ? 0.0f :
					static_cast<float>(x - ix0) / (ix1 - ix0);
				const auto y = static_cast<int>(y0 + t * (y1 - y0));
				const auto base = static_cast<int>(base_y);
				for (int pixel_y = y; pixel_y <= base; ++pixel_y)
					set_pixel(x, pixel_y,
					          blend(get_pixel(x, pixel_y), _style.fillColor,
					                _style.fillAlpha));
			}
		}
	}

	void draw_line()
	{
		static constexpr float offsets[] = {0.0f, 0.5f, -0.5f};
		const auto count = static_cast<int>(_data.size());
		for (const auto offset : offsets)
			for (int i = 0; i + 1 < count; ++i)
				draw_aa_line(data_to_x(i), data_to_y(_data[i]) + offset,
				             data_to_x(i + 1), data_to_y(_data[i + 1]) + offset,
				             _style.lineColor);
	}

public:
	void resize(const int width, const int height)
	{
		if (width == _width && height == _height && !_pixels.empty()) return;
		_width = std::max(0, width);
		_height = std::max(0, height);
		_pixels.assign(static_cast<size_t>(_width) * _height, 0);
	}

	template <class Container>
	void set_data(const Container& data, const float min_value = 0.0f,
	              const float max_value = 100.0f)
	{
		_data.assign(data.begin(), data.end());
		_min_value = min_value;
		_max_value = max_value;
	}

	void set_style(const ChartStyle& style) { _style = style; }
	void set_margin(const int margin) { _grid_margin = margin; }

	void render()
	{
		if (_width <= 0 || _height <= 0) return;
		fill(_style.background);
		draw_grid();
		if (!_data.empty())
		{
			draw_filled_area();
			draw_line();
		}
	}

	[[nodiscard]] int width() const { return _width; }
	[[nodiscard]] int height() const { return _height; }
	[[nodiscard]] const std::vector<uint32_t>& pixels() const { return _pixels; }
};
