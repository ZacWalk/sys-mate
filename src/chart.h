#pragma once
#include <windows.h>
#include <cmath>
#include <vector>
#include <deque>
#include <algorithm>

// Anti-aliased chart renderer to an RGB buffer, drawn via SetDIBitsToDevice.
// All rendering is done in software C++ with sub-pixel anti-aliasing.

struct ChartColor
{
	uint8_t r, g, b;
};

struct ChartStyle
{
	ChartColor background = {30, 30, 30};
	ChartColor gridLine = {55, 55, 55};
	ChartColor lineColor = {0, 180, 255};
	ChartColor fillColor = {0, 120, 200};
	ChartColor textColor = {180, 180, 180};
	ChartColor axisColor = {80, 80, 80};
	int fillAlpha = 60; // 0-255
};

class Chart
{
public:
	void Resize(const int w, const int h)
	{
		if (w == m_w && h == m_h && !m_pixels.empty()) return;
		m_w = w;
		m_h = h;
		// BMP rows are bottom-up, each row padded to 4 bytes
		m_stride = ((w * 3 + 3) & ~3);
		m_pixels.assign(static_cast<size_t>(m_stride) * h, 0);
	}

	template <class Container>
	void SetData(const Container& data, const float minVal = 0.f, const float maxVal = 100.f)
	{
		m_data.assign(data.begin(), data.end());
		m_minVal = minVal;
		m_maxVal = maxVal;
	}

	void SetStyle(const ChartStyle& s) { m_style = s; }
	void SetTitle(const wchar_t* t) { m_title = t ? t : L""; }
	void SetMargin(const int m) { m_gridMargin = m; }

	void Render()
	{
		if (m_w <= 0 || m_h <= 0) return;
		Fill(m_style.background);
		DrawGrid();
		if (!m_data.empty())
		{
			DrawFilledArea();
			DrawLine();
		}
	}

	void Paint(const HDC hdc, const int x, const int y) const
	{
		if (m_w <= 0 || m_h <= 0) return;
		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = m_w;
		bmi.bmiHeader.biHeight = m_h; // positive = bottom-up
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biCompression = BI_RGB;
		SetDIBitsToDevice(hdc, x, y, m_w, m_h,
		                  0, 0, 0, m_h, m_pixels.data(), &bmi, DIB_RGB_COLORS);
	}

	int Width() const { return m_w; }
	int Height() const { return m_h; }
	const std::wstring& Title() const { return m_title; }

private:
	int m_w = 0, m_h = 0, m_stride = 0;
	std::vector<uint8_t> m_pixels;
	std::vector<float> m_data;
	float m_minVal = 0, m_maxVal = 100;
	ChartStyle m_style;
	std::wstring m_title;
	int m_gridMargin = 8;

	// Set pixel (bottom-up bitmap: row 0 is bottom)
	void SetPixel(const int x, const int y, const ChartColor c)
	{
		if (x < 0 || x >= m_w || y < 0 || y >= m_h) return;
		const int row = m_h - 1 - y; // flip to bottom-up
		const int off = row * m_stride + x * 3;
		m_pixels[off + 0] = c.b;
		m_pixels[off + 1] = c.g;
		m_pixels[off + 2] = c.r;
	}

	ChartColor GetPixel(const int x, const int y) const
	{
		if (x < 0 || x >= m_w || y < 0 || y >= m_h) return {0, 0, 0};
		const int row = m_h - 1 - y;
		const int off = row * m_stride + x * 3;
		return {m_pixels[off + 2], m_pixels[off + 1], m_pixels[off + 0]};
	}

	static ChartColor Blend(const ChartColor dst, const ChartColor src, const int alpha)
	{
		return {
			static_cast<uint8_t>((src.r * alpha + dst.r * (255 - alpha)) / 255),
			static_cast<uint8_t>((src.g * alpha + dst.g * (255 - alpha)) / 255),
			static_cast<uint8_t>((src.b * alpha + dst.b * (255 - alpha)) / 255)
		};
	}

	void SetPixelAA(const int x, const int y, const ChartColor c, const float alpha)
	{
		if (x < 0 || x >= m_w || y < 0 || y >= m_h) return;
		const int a = static_cast<int>(alpha * 255);
		if (a <= 0) return;
		if (a >= 255)
		{
			SetPixel(x, y, c);
			return;
		}
		const auto bg = GetPixel(x, y);
		SetPixel(x, y, Blend(bg, c, a));
	}

	void Fill(const ChartColor c)
	{
		for (int y = 0; y < m_h; y++)
		{
			const int row = m_h - 1 - y;
			int off = row * m_stride;
			for (int x = 0; x < m_w; x++)
			{
				m_pixels[off + 0] = c.b;
				m_pixels[off + 1] = c.g;
				m_pixels[off + 2] = c.r;
				off += 3;
			}
		}
	}

	void DrawGrid()
	{
		const int margin = m_gridMargin;
		// horizontal grid lines (4 divisions)
		for (int i = 1; i < 4; i++)
		{
			const int y = margin + (m_h - 2 * margin) * i / 4;
			for (int x = margin; x < m_w - margin; x++)
				SetPixel(x, y, m_style.gridLine);
		}
		// vertical grid lines
		const int graphW = m_w - 2 * margin;
		for (int i = 1; i < 6; i++)
		{
			const int x = margin + graphW * i / 6;
			for (int y = margin; y < m_h - margin; y++)
				SetPixel(x, y, m_style.gridLine);
		}
		// border
		for (int x = margin; x < m_w - margin; x++)
		{
			SetPixel(x, margin, m_style.axisColor);
			SetPixel(x, m_h - margin - 1, m_style.axisColor);
		}
		for (int y = margin; y < m_h - margin; y++)
		{
			SetPixel(margin, y, m_style.axisColor);
			SetPixel(m_w - margin - 1, y, m_style.axisColor);
		}
	}

	// Xiaolin Wu anti-aliased line
	void DrawAALine(float x0, float y0, float x1, float y1, const ChartColor c)
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
		const float dx = x1 - x0, dy = y1 - y0;
		const float gradient = dx == 0.f ? 1.f : dy / dx;

		// first endpoint
		float xend = std::round(x0);
		float yend = y0 + gradient * (xend - x0);
		float xgap = 1.f - (x0 + 0.5f - std::floor(x0 + 0.5f));
		const int xpxl1 = static_cast<int>(xend);
		const int ypxl1 = static_cast<int>(std::floor(yend));
		if (steep)
		{
			SetPixelAA(ypxl1, xpxl1, c, (1.f - (yend - ypxl1)) * xgap);
			SetPixelAA(ypxl1 + 1, xpxl1, c, (yend - ypxl1) * xgap);
		}
		else
		{
			SetPixelAA(xpxl1, ypxl1, c, (1.f - (yend - ypxl1)) * xgap);
			SetPixelAA(xpxl1, ypxl1 + 1, c, (yend - ypxl1) * xgap);
		}
		float intery = yend + gradient;

		// second endpoint
		xend = std::round(x1);
		yend = y1 + gradient * (xend - x1);
		xgap = x1 + 0.5f - std::floor(x1 + 0.5f);
		const int xpxl2 = static_cast<int>(xend);
		const int ypxl2 = static_cast<int>(std::floor(yend));
		if (steep)
		{
			SetPixelAA(ypxl2, xpxl2, c, (1.f - (yend - ypxl2)) * xgap);
			SetPixelAA(ypxl2 + 1, xpxl2, c, (yend - ypxl2) * xgap);
		}
		else
		{
			SetPixelAA(xpxl2, ypxl2, c, (1.f - (yend - ypxl2)) * xgap);
			SetPixelAA(xpxl2, ypxl2 + 1, c, (yend - ypxl2) * xgap);
		}

		// main loop
		for (int x = xpxl1 + 1; x < xpxl2; x++)
		{
			const int iy = static_cast<int>(std::floor(intery));
			const float frac = intery - iy;
			if (steep)
			{
				SetPixelAA(iy, x, c, 1.f - frac);
				SetPixelAA(iy + 1, x, c, frac);
			}
			else
			{
				SetPixelAA(x, iy, c, 1.f - frac);
				SetPixelAA(x, iy + 1, c, frac);
			}
			intery += gradient;
		}
	}

	float DataToY(const float val) const
	{
		const int margin = m_gridMargin;
		float range = m_maxVal - m_minVal;
		if (range <= 0) range = 1;
		float norm = (val - m_minVal) / range;
		norm = std::clamp(norm, 0.f, 1.f);
		return (m_h - margin - 1) - norm * (m_h - 2 * margin - 2);
	}

	float DataToX(const int idx) const
	{
		const int margin = m_gridMargin;
		const int n = static_cast<int>(m_data.size());
		if (n <= 1) return static_cast<float>(m_w / 2);
		return margin + static_cast<float>(idx) / (n - 1) * (m_w - 2 * margin - 2);
	}

	void DrawFilledArea()
	{
		const int margin = m_gridMargin;
		const int n = static_cast<int>(m_data.size());
		const float baseY = static_cast<float>(m_h - margin - 1);
		for (int i = 0; i < n - 1; i++)
		{
			const float x0 = DataToX(i), x1 = DataToX(i + 1);
			const float y0 = DataToY(m_data[i]), y1 = DataToY(m_data[i + 1]);
			const int ix0 = static_cast<int>(x0), ix1 = static_cast<int>(x1);
			for (int x = ix0; x <= ix1; x++)
			{
				const float t = (ix1 == ix0) ? 0.f : static_cast<float>(x - ix0) / (ix1 - ix0);
				const float fy = y0 + t * (y1 - y0);
				const int iy = static_cast<int>(fy);
				const int ibase = static_cast<int>(baseY);
				for (int y = iy; y <= ibase; y++)
				{
					const auto bg = GetPixel(x, y);
					SetPixel(x, y, Blend(bg, m_style.fillColor, m_style.fillAlpha));
				}
			}
		}
	}

	void DrawLine()
	{
		const int n = static_cast<int>(m_data.size());
		// Three passes (offsets 0, +0.5, -0.5) approximate a 2-pixel-thick line.
		static constexpr float kOffsets[] = {0.f, 0.5f, -0.5f};
		for (float off : kOffsets)
		{
			for (int i = 0; i < n - 1; i++)
			{
				DrawAALine(DataToX(i), DataToY(m_data[i]) + off,
				           DataToX(i + 1), DataToY(m_data[i + 1]) + off,
				           m_style.lineColor);
			}
		}
	}
};
