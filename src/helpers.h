#pragma once
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <algorithm>
#include <initializer_list>
#include "layout.h"
#include "dark_theme.h"

// ============================================================
// CRTP self-pointer thunk for window classes
// Subclass T must implement:
//   LRESULT HandleMsg(HWND, UINT, WPARAM, LPARAM);
// ============================================================
template <class T>
struct OwnedWnd
{
	static LRESULT CALLBACK Thunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		T* self = nullptr;
		if (msg == WM_NCCREATE)
		{
			auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
			self = static_cast<T*>(cs->lpCreateParams);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		else
		{
			self = reinterpret_cast<T*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		}
		if (!self) return DefWindowProc(hWnd, msg, wParam, lParam);
		return self->HandleMsg(hWnd, msg, wParam, lParam);
	}
};

// ============================================================
// Window class registration helper (idempotent)
// ============================================================
inline void RegisterSimpleClass(HINSTANCE hInst, LPCWSTR name, WNDPROC proc, UINT style = 0)
{
	WNDCLASSW wc = {};
	if (GetClassInfoW(hInst, name, &wc)) return;
	wc.style = style;
	wc.lpfnWndProc = proc;
	wc.hInstance = hInst;
	wc.lpszClassName = name;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	RegisterClassW(&wc);
}

// ============================================================
// UI font helper (Segoe UI, DPI-scaled point size)
// ============================================================
inline HFONT MakeUiFont(int sizePt, int weight = FW_NORMAL)
{
	return CreateFontW(Dpi::Scale(-sizePt), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
	                   0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

// ============================================================
// Double-buffered paint RAII
// Usage:
//   case WM_PAINT: { DoubleBuffer db(hWnd); ... draw into db.dc ...; return 0; }
// ============================================================
struct DoubleBuffer
{
	HWND hWnd;
	PAINTSTRUCT ps;
	HDC winDC;
	HDC dc;          // off-screen DC for caller to draw into
	HBITMAP bmp;
	HBITMAP oldBmp;
	RECT rc;

	explicit DoubleBuffer(HWND h) : hWnd(h)
	{
		winDC = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rc);
		dc = CreateCompatibleDC(winDC);
		bmp = CreateCompatibleBitmap(winDC, rc.right, rc.bottom);
		oldBmp = static_cast<HBITMAP>(SelectObject(dc, bmp));
	}

	~DoubleBuffer()
	{
		BitBlt(winDC, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
		SelectObject(dc, oldBmp);
		DeleteObject(bmp);
		DeleteDC(dc);
		EndPaint(hWnd, &ps);
	}

	DoubleBuffer(const DoubleBuffer&) = delete;
	DoubleBuffer& operator=(const DoubleBuffer&) = delete;
};

// ============================================================
// Vertical scrollbar state.
// pos/viewH/contentH share a unit (pixels OR row indices).
// The owner sets trackTop/trackBottom/sbX/sbRight in client coords.
// ============================================================
struct VScroll
{
	int pos = 0;
	int viewH = 0;
	int contentH = 0;
	bool dragging = false;
	int dragOffset = 0;
	int trackTop = 0;
	int trackBottom = 0;
	int sbX = 0;
	int sbRight = 0;
	int wheelAccum = 0;

	int MaxScroll() const { return std::max(0, contentH - viewH); }
	bool Needed() const { return contentH > viewH; }

	int ThumbHeight() const
	{
		const int trackH = std::max(1, trackBottom - trackTop);
		return std::max(Dpi::Scale(Layout::ScrollbarThumbMin),
		                trackH * viewH / std::max(1, contentH));
	}

	RECT ThumbRect() const
	{
		if (!Needed()) return {sbX, trackTop, sbRight, trackBottom};
		const int thumbH = ThumbHeight();
		const int trackH = (trackBottom - trackTop) - thumbH;
		const int ms = MaxScroll();
		const int thumbY = trackTop + (ms > 0 ? pos * trackH / ms : 0);
		return {sbX, thumbY, sbRight, thumbY + thumbH};
	}

	void Clamp() { pos = std::clamp(pos, 0, MaxScroll()); }

	// Returns true if input was inside the scrollbar area and was consumed.
	bool OnLButtonDown(HWND hWnd, int x, int y)
	{
		if (!Needed() || x < sbX || x >= sbRight) return false;
		const RECT thumb = ThumbRect();
		if (y >= thumb.top && y <= thumb.bottom)
		{
			dragging = true;
			dragOffset = y - thumb.top;
			SetCapture(hWnd);
		}
		else
		{
			if (y < thumb.top) pos = std::max(0, pos - viewH);
			else pos = std::min(MaxScroll(), pos + viewH);
		}
		return true;
	}

	bool OnLButtonUp()
	{
		if (!dragging) return false;
		dragging = false;
		ReleaseCapture();
		return true;
	}

	bool OnMouseMove(int y)
	{
		if (!dragging) return false;
		const int thumbH = ThumbHeight();
		const int trackH = (trackBottom - trackTop) - thumbH;
		if (trackH > 0)
		{
			const int thumbY = y - dragOffset - trackTop;
			pos = std::clamp(thumbY * MaxScroll() / trackH, 0, MaxScroll());
		}
		return true;
	}

	// `unitPerLine` converts a scroll line into this scrollbar's unit (rows or pixels).
	// Accumulates sub-notch deltas so high-resolution wheels/touchpads still scroll.
	bool OnMouseWheel(int delta, int unitPerLine)
	{
		// SPI_GETWHEELSCROLLLINES reports UINT_MAX to mean "one screen per notch".
		UINT lines = 3;
		SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
		const int unitPerNotch = (lines == UINT_MAX)
			                         ? std::max(1, viewH - 1)
			                         : std::max(1, static_cast<int>(lines) * unitPerLine);

		wheelAccum += delta;
		const int notches = wheelAccum / WHEEL_DELTA;
		if (notches == 0) return false;
		wheelAccum -= notches * WHEEL_DELTA;

		const int before = pos;
		pos -= notches * unitPerNotch;
		Clamp();
		return pos != before;
	}

	void Draw(HDC dc) const
	{
		if (!Needed()) return;
		RECT bgRc = {sbX, trackTop, sbRight, trackBottom};
		FillRect(dc, &bgRc, Dark::BrushScrollTrack());
		RECT t = ThumbRect();
		FillRect(dc, &t, Dark::BrushScrollThumb());
	}
};

// ============================================================
// Popup menu helper
// ============================================================
struct PopupItem
{
	UINT id;
	const WCHAR* label;
	bool enabled = true;
};

inline UINT RunPopupMenu(HWND hWnd, int x, int y, std::initializer_list<PopupItem> items)
{
	HMENU menu = CreatePopupMenu();
	for (const auto& it : items)
		AppendMenuW(menu, MF_STRING | (it.enabled ? 0u : MF_GRAYED), it.id, it.label);
	const UINT cmd = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
		x, y, 0, hWnd, nullptr));
	DestroyMenu(menu);
	return cmd;
}

// ============================================================
// Extended TCP/UDP table query (probe + alloc + retry).
// Caller owns the returned pointer; free with FreeExtTable.
// ============================================================
template <class T, class Fn>
inline T* QueryExtTable(Fn fn)
{
	ULONG size = 0;
	DWORD ret = fn(nullptr, &size);
	T* table = nullptr;
	while (ret == ERROR_INSUFFICIENT_BUFFER)
	{
		table = static_cast<T*>(HeapAlloc(GetProcessHeap(), 0, size));
		if (!table) return nullptr;
		ret = fn(table, &size);
		if (ret == ERROR_INSUFFICIENT_BUFFER)
		{
			HeapFree(GetProcessHeap(), 0, table);
			table = nullptr;
		}
	}
	if (ret != NO_ERROR && table)
	{
		HeapFree(GetProcessHeap(), 0, table);
		table = nullptr;
	}
	return table;
}

inline void FreeExtTable(void* p) { if (p) HeapFree(GetProcessHeap(), 0, p); }

// ============================================================
// Number / value formatters
// ============================================================
namespace Format
{
	inline std::wstring Gb(ULONGLONG bytes)
	{
		WCHAR buf[32];
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.1f GB",
			bytes / (1024.0 * 1024.0 * 1024.0));
		return buf;
	}

	inline std::wstring Ghz(DWORD mhz)
	{
		WCHAR buf[32];
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.2f GHz", mhz / 1000.0);
		return buf;
	}

	// Auto-scaled byte count, e.g. "512 KB", "1.4 GB".
	inline std::wstring Bytes(double bytes)
	{
		static const WCHAR* kUnits[] = {L"B", L"KB", L"MB", L"GB", L"TB", L"PB"};
		int unit = 0;
		while (bytes >= 1024.0 && unit + 1 < static_cast<int>(_countof(kUnits)))
		{
			bytes /= 1024.0;
			unit++;
		}
		WCHAR buf[32];
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, unit == 0 ? L"%.0f %s" : L"%.1f %s", bytes, kUnits[unit]);
		return buf;
	}

	inline std::wstring Rate(double bytesPerSec)
	{
		return Bytes(bytesPerSec) + L"/s";
	}

	inline std::wstring U(unsigned v)
	{
		WCHAR buf[16];
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%u", v);
		return buf;
	}

	inline std::wstring Hex32(unsigned v)
	{
		WCHAR buf[16];
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"0x%08X", v);
		return buf;
	}

	inline std::wstring Ptr(void* p)
	{
		WCHAR buf[20];
		_snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%p", p);
		return buf;
	}
}

// ============================================================
// Confirmation dialog helper
// ============================================================
inline bool ConfirmAction(HWND owner, const WCHAR* msg)
{
	return MessageBoxW(owner, msg, L"System Stuff", MB_OKCANCEL | MB_ICONWARNING) == IDOK;
}
