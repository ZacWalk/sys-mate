#pragma once
#include <windows.h>
#include <string>

// ============================================================
// DPI Scaling
// ============================================================
namespace Dpi
{
	inline float g_scale = 1.0f;

	inline int Scale(const int value)
	{
		const float r = value * g_scale;
		return static_cast<int>(r + (r >= 0 ? 0.5f : -0.5f));
	}
}

// ============================================================
// Layout Constants (base values at 96 DPI)
// ============================================================
namespace Layout
{
	constexpr int TabHeight = 30;
	constexpr int BottomBarHeight = 34;
	constexpr int ButtonWidth = 130;
	constexpr int ButtonHeight = 26;
	constexpr int ButtonSpacing = 6;
	constexpr int FilterWidth = 220;
	constexpr int SplitterWidth = 4;
	constexpr int Padding = 4;
	constexpr int ListCellPadding = 6;
	constexpr int ListRowHeight = 22;
	constexpr int ListHeaderHeight = 24;
	constexpr int ScrollbarWidth = 14;
	constexpr int ScrollbarThumbMin = 20;
	constexpr int HeaderEdgeHitZone = 4;
	constexpr int MinColumnWidth = 30;
	constexpr int ChartGridMargin = 8;
	constexpr int ChartSpacing = 8;
	constexpr int ChartPadding = 4;
	constexpr int ChartTextMargin = 16;
	constexpr int ChartTextTop = 8;
	constexpr int ChartTextBottom = 34;
	constexpr int ChartValueRightMargin = 160;
	constexpr int ChartDetailBottom = 50;
	constexpr int AboutLeftMargin = 28;
	constexpr int AboutTopMargin = 28;
	constexpr int AboutTitleHeight = 42;
	constexpr int AboutTitleSpacing = 46;
	constexpr int AboutSubtitleHeight = 24;
	constexpr int AboutSubtitleSpacing = 28;
	constexpr int AboutCopyrightHeight = 20;
	constexpr int AboutPostCopyrightSpacing = 36;
	constexpr int AboutPostSepSpacing = 20;
	constexpr int AboutRowHeight = 22;
	constexpr int AboutRowSpacing = 26;
	constexpr int AboutLabelWidth = 170;
	constexpr int AboutValueOffset = 180;
	constexpr int AboutRightMargin = 28;
	constexpr int AboutIconSize = 64;
	constexpr int AboutIconGap = 16;
}

// ============================================================
// Text Alignment Flags
// ============================================================
namespace TextAlign
{
	constexpr int Left = 0;
	constexpr int Center = 1;
	constexpr int Right = 2;
	constexpr int VCenter = 4;
	constexpr int Ellipsis = 8;
}

// ============================================================
// Text Drawing Helper (replaces DrawText with ExtTextOutW)
// ============================================================
inline void DrawTextExt(const HDC hdc, const WCHAR* text, int len, const RECT& rc, const int flags)
{
	if (!text) return;
	if (len < 0) len = static_cast<int>(wcslen(text));
	if (len == 0) return;

	SIZE sz;
	GetTextExtentPoint32W(hdc, text, len, &sz);

	const int rcW = rc.right - rc.left;
	const int rcH = rc.bottom - rc.top;

	const WCHAR* drawText = text;
	int drawLen = len;
	std::wstring truncated;

	if ((flags & TextAlign::Ellipsis) && sz.cx > rcW)
	{
		SIZE ellipSz;
		GetTextExtentPoint32W(hdc, L"...", 3, &ellipSz);
		int fitLen = len;
		while (fitLen > 0)
		{
			fitLen--;
			SIZE testSz;
			GetTextExtentPoint32W(hdc, text, fitLen, &testSz);
			if (testSz.cx + ellipSz.cx <= rcW)
			{
				truncated.assign(text, fitLen);
				truncated += L"...";
				break;
			}
		}
		if (truncated.empty()) truncated = L"...";
		drawText = truncated.c_str();
		drawLen = static_cast<int>(truncated.size());
		GetTextExtentPoint32W(hdc, drawText, drawLen, &sz);
	}

	int x, y;

	if (flags & TextAlign::Center)
		x = rc.left + (rcW - sz.cx) / 2;
	else if (flags & TextAlign::Right)
		x = rc.right - sz.cx;
	else
		x = rc.left;

	if (flags & TextAlign::VCenter)
		y = rc.top + (rcH - sz.cy) / 2;
	else
		y = rc.top;

	ExtTextOutW(hdc, x, y, ETO_CLIPPED, &rc, drawText, drawLen, nullptr);
}
