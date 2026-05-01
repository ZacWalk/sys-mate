#pragma once
#include <windows.h>

// Dark theme colors
namespace Dark
{
	constexpr COLORREF BgWindow = RGB(30, 30, 30);
	constexpr COLORREF BgControl = RGB(40, 40, 40);
	constexpr COLORREF BgHeader = RGB(45, 45, 48);
	constexpr COLORREF BgSelected = RGB(0, 100, 180);
	constexpr COLORREF BgHot = RGB(50, 50, 55);
	constexpr COLORREF BgTab = RGB(45, 45, 48);
	constexpr COLORREF BgTabSel = RGB(30, 30, 30);
	constexpr COLORREF BgEdit = RGB(50, 50, 55);
	constexpr COLORREF Text = RGB(220, 220, 220);
	constexpr COLORREF TextDim = RGB(150, 150, 150);
	constexpr COLORREF TextSel = RGB(255, 255, 255);
	constexpr COLORREF Border = RGB(60, 60, 65);
	constexpr COLORREF Splitter = RGB(55, 55, 60);

	inline HBRUSH BrushWindow()
	{
		static HBRUSH b = CreateSolidBrush(BgWindow);
		return b;
	}

	inline HBRUSH BrushControl()
	{
		static HBRUSH b = CreateSolidBrush(BgControl);
		return b;
	}

	inline HBRUSH BrushTab()
	{
		static HBRUSH b = CreateSolidBrush(BgTab);
		return b;
	}

	inline HBRUSH BrushTabSel()
	{
		static HBRUSH b = CreateSolidBrush(BgTabSel);
		return b;
	}

	inline HBRUSH BrushEdit()
	{
		static HBRUSH b = CreateSolidBrush(BgEdit);
		return b;
	}

	inline HBRUSH BrushSelected()
	{
		static HBRUSH b = CreateSolidBrush(BgSelected);
		return b;
	}

	inline HBRUSH BrushHeader()
	{
		static HBRUSH b = CreateSolidBrush(BgHeader);
		return b;
	}

	// Handle WM_CTLCOLOREDIT for dark edit boxes
	inline LRESULT OnCtlColorEdit(const HDC hdc)
	{
		SetTextColor(hdc, Text);
		SetBkColor(hdc, BgEdit);
		return (LRESULT)BrushEdit();
	}
}
