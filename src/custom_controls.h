#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include "dark_theme.h"
#include "layout.h"
#include "helpers.h"

// ============================================================
// Custom Tab Control
// ============================================================
class CustomTabControl : public OwnedWnd<CustomTabControl>
{
public:
	struct Tab { std::wstring text; };

	void Create(HWND hParent, HINSTANCE hInst, int height, HFONT hFont)
	{
		m_height = height;
		m_hFont = hFont;
		RegisterSimpleClass(hInst, L"SystemStuffCustomTab", &Thunk);
		m_hWnd = CreateWindowExW(0, L"SystemStuffCustomTab", L"",
			WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
			0, 0, 100, height, hParent, nullptr, hInst, this);
	}

	void AddTab(const WCHAR* text) { m_tabs.push_back({text}); InvalidateRect(m_hWnd, nullptr, FALSE); }
	int GetCurSel() const { return m_selected; }

	void SetCurSel(int idx)
	{
		if (idx >= 0 && idx < (int)m_tabs.size())
		{
			m_selected = idx;
			InvalidateRect(m_hWnd, nullptr, FALSE);
		}
	}

	HWND GetHWND() const { return m_hWnd; }
	void SetChangeCallback(std::function<void(int)> cb) { m_onChange = std::move(cb); }

	LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_PAINT: OnPaint(); return 0;
		case WM_ERASEBKGND: return 1;
		case WM_LBUTTONDOWN:
		{
			int idx = HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (idx >= 0 && idx != m_selected)
			{
				m_selected = idx;
				InvalidateRect(hWnd, nullptr, FALSE);
				if (m_onChange) m_onChange(idx);
			}
			return 0;
		}
		case WM_MOUSEMOVE:
		{
			int idx = HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (idx != m_hot)
			{
				m_hot = idx;
				InvalidateRect(hWnd, nullptr, FALSE);
				TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
				TrackMouseEvent(&tme);
			}
			return 0;
		}
		case WM_MOUSELEAVE:
			m_hot = -1;
			InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

private:
	HWND m_hWnd = nullptr;
	HFONT m_hFont = nullptr;
	int m_height = 30;
	int m_selected = 0;
	int m_hot = -1;
	std::vector<Tab> m_tabs;
	std::function<void(int)> m_onChange;

	int HitTest(int x, int y) const
	{
		if (m_tabs.empty()) return -1;
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		int tabW = rc.right / (int)m_tabs.size();
		if (tabW < 60) tabW = 60;
		for (int i = 0; i < (int)m_tabs.size(); i++)
		{
			RECT tr = {i * tabW, 0, (i + 1) * tabW, rc.bottom};
			POINT pt = {x, y};
			if (PtInRect(&tr, pt)) return i;
		}
		return -1;
	}

	void OnPaint()
	{
		DoubleBuffer db(m_hWnd);
		const RECT& rc = db.rc;
		HDC memDC = db.dc;

		FillRect(memDC, &rc, Dark::BrushTab());

		if (!m_tabs.empty())
		{
			int tabW = rc.right / (int)m_tabs.size();
			if (tabW < 60) tabW = 60;

			SelectObject(memDC, m_hFont);
			SetBkMode(memDC, TRANSPARENT);

			for (int i = 0; i < (int)m_tabs.size(); i++)
			{
				RECT tr = {i * tabW, 0, (i + 1) * tabW, rc.bottom};
				bool sel = (i == m_selected);
				bool hot = (i == m_hot && !sel);

				if (sel)
					FillRect(memDC, &tr, Dark::BrushTabSel());
				else if (hot)
				{
					HBRUSH hbr = CreateSolidBrush(Dark::BgHot);
					FillRect(memDC, &tr, hbr);
					DeleteObject(hbr);
				}

				if (sel)
				{
					RECT accent = tr;
					accent.bottom = accent.top + 2;
					HBRUSH acBr = CreateSolidBrush(RGB(0, 150, 255));
					FillRect(memDC, &accent, acBr);
					DeleteObject(acBr);
				}

				SetTextColor(memDC, sel ? Dark::Text : Dark::TextDim);
				DrawTextExt(memDC, m_tabs[i].text.c_str(), -1, tr, TextAlign::Center | TextAlign::VCenter);
			}
		}

		// Bottom border
		HPEN pen = CreatePen(PS_SOLID, 1, Dark::Border);
		HPEN oldPen = (HPEN)SelectObject(memDC, pen);
		MoveToEx(memDC, 0, rc.bottom - 1, nullptr);
		LineTo(memDC, rc.right, rc.bottom - 1);
		SelectObject(memDC, oldPen);
		DeleteObject(pen);
	}
};

// ============================================================
// Custom ListView (detail/report view with vertical scrollbar)
// ============================================================
class CustomListView : public OwnedWnd<CustomListView>
{
public:
	struct Column { std::wstring text; int width; };

	void Create(HWND hParent, HINSTANCE hInst, HFONT hFont, HFONT hFontBold)
	{
		m_hFont = hFont;
		m_hFontBold = hFontBold;
		m_hInst = hInst;
		m_rowHeight = Dpi::Scale(Layout::ListRowHeight);
		m_headerHeight = Dpi::Scale(Layout::ListHeaderHeight);
		m_scrollbarWidth = Dpi::Scale(Layout::ScrollbarWidth);
		m_cellPadding = Dpi::Scale(Layout::ListCellPadding);

		RegisterSimpleClass(hInst, L"SystemStuffCustomLV", &Thunk, CS_DBLCLKS);

		m_hWnd = CreateWindowExW(0, L"SystemStuffCustomLV", L"",
			WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
			0, 0, 100, 100, hParent, nullptr, hInst, this);
	}

	HWND GetHWND() const { return m_hWnd; }

	void AddColumn(const WCHAR* text, int width) { m_columns.push_back({text, width}); }

	int AddItem(const WCHAR* text)
	{
		std::vector<std::wstring> row;
		row.push_back(text);
		for (size_t i = 1; i < m_columns.size(); i++) row.push_back(L"");
		m_rows.push_back(row);
		m_itemData.push_back(0);
		UpdateScrollInfo();
		return (int)m_rows.size() - 1;
	}

	void SetItemText(int item, int subItem, const WCHAR* text)
	{
		if (item < 0 || item >= (int)m_rows.size()) return;
		if (subItem < 0 || subItem >= (int)m_columns.size()) return;
		while ((int)m_rows[item].size() <= subItem) m_rows[item].push_back(L"");
		m_rows[item][subItem] = text;
	}

	void SetItemText(int item, int subItem, const std::wstring& text)
	{
		SetItemText(item, subItem, text.c_str());
	}

	void SetItemData(int item, LPARAM data)
	{
		if (item >= 0 && item < (int)m_itemData.size()) m_itemData[item] = data;
	}

	LPARAM GetItemData(int item) const
	{
		if (item >= 0 && item < (int)m_itemData.size()) return m_itemData[item];
		return 0;
	}

	std::wstring GetItemText(int item, int subItem) const
	{
		if (item < 0 || item >= (int)m_rows.size()) return L"";
		if (subItem < 0 || subItem >= (int)m_rows[item].size()) return L"";
		return m_rows[item][subItem];
	}

	void DeleteAllItems()
	{
		m_rows.clear();
		m_itemData.clear();
		m_selected = -1;
		m_scroll.pos = 0;
		UpdateScrollInfo();
	}

	int GetItemCount() const { return (int)m_rows.size(); }
	int GetSelected() const { return m_selected; }

	void SetSelected(int idx)
	{
		if (idx != m_selected)
		{
			m_selected = idx;
			EnsureVisible(idx);
			InvalidateRect(m_hWnd, nullptr, FALSE);
		}
	}

	void SetRedraw(bool redraw)
	{
		m_redrawEnabled = redraw;
		if (redraw)
		{
			UpdateScrollInfo();
			InvalidateRect(m_hWnd, nullptr, FALSE);
		}
	}

	void Invalidate() { InvalidateRect(m_hWnd, nullptr, FALSE); }

	using SelectionCallback = std::function<void(int)>;
	using ContextMenuCallback = std::function<void(HWND, int, int)>;
	void SetSelectionCallback(SelectionCallback cb) { m_onSelect = std::move(cb); }
	void SetContextMenuCallback(ContextMenuCallback cb) { m_onContextMenu = std::move(cb); }

	LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_PAINT:
			if (m_redrawEnabled) OnPaint();
			else { PAINTSTRUCT ps; BeginPaint(hWnd, &ps); EndPaint(hWnd, &ps); }
			return 0;
		case WM_ERASEBKGND: return 1;

		case WM_LBUTTONDOWN:
		{
			SetFocus(hWnd);
			int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

			// Header column resize
			if (y < m_headerHeight)
			{
				int col = HitTestHeaderEdge(x);
				if (col >= 0)
				{
					m_headerDragCol = col;
					m_headerDragX = x;
					SetCapture(hWnd);
					return 0;
				}
			}

			// Scrollbar
			SyncScrollGeometry();
			if (m_scroll.OnLButtonDown(hWnd, x, y))
			{
				InvalidateRect(hWnd, nullptr, FALSE);
				return 0;
			}

			// Row selection
			int row = HitTestRow(y);
			if (row >= 0 && row != m_selected)
			{
				m_selected = row;
				InvalidateRect(hWnd, nullptr, FALSE);
				if (m_onSelect) m_onSelect(row);
			}
			return 0;
		}

		case WM_LBUTTONUP:
			if (m_scroll.OnLButtonUp() || m_headerDragCol >= 0)
			{
				m_headerDragCol = -1;
				ReleaseCapture();
			}
			return 0;

		case WM_MOUSEMOVE:
		{
			int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
			if (m_headerDragCol >= 0)
			{
				int delta = x - m_headerDragX;
				int& w = m_columns[m_headerDragCol].width;
				w = std::max(Dpi::Scale(Layout::MinColumnWidth), w + delta);
				m_headerDragX = x;
				InvalidateRect(hWnd, nullptr, FALSE);
				return 0;
			}
			if (m_scroll.OnMouseMove(y))
			{
				InvalidateRect(hWnd, nullptr, FALSE);
				return 0;
			}
			// Hot tracking
			RECT rc;
			GetClientRect(hWnd, &rc);
			if (x < rc.right - m_scrollbarWidth)
			{
				int row = HitTestRow(y);
				if (row != m_hot)
				{
					m_hot = row;
					InvalidateRect(hWnd, nullptr, FALSE);
					TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
					TrackMouseEvent(&tme);
				}
			}
			return 0;
		}

		case WM_SETCURSOR:
		{
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(hWnd, &pt);
			if (pt.y < m_headerHeight && HitTestHeaderEdge(pt.x) >= 0)
			{
				SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
				return TRUE;
			}
			break;
		}

		case WM_MOUSELEAVE:
			m_hot = -1;
			InvalidateRect(hWnd, nullptr, FALSE);
			return 0;

		case WM_MOUSEWHEEL:
		{
			SyncScrollGeometry();
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			m_scroll.pos -= delta / 40;
			m_scroll.Clamp();
			InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}

		case WM_KEYDOWN:
			SyncScrollGeometry();
			if (wParam == VK_UP && m_selected > 0)
			{
				m_selected--;
				EnsureVisible(m_selected);
				InvalidateRect(hWnd, nullptr, FALSE);
				if (m_onSelect) m_onSelect(m_selected);
			}
			else if (wParam == VK_DOWN && m_selected < (int)m_rows.size() - 1)
			{
				m_selected++;
				EnsureVisible(m_selected);
				InvalidateRect(hWnd, nullptr, FALSE);
				if (m_onSelect) m_onSelect(m_selected);
			}
			else if (wParam == VK_PRIOR)
			{
				m_scroll.pos = std::max(0, m_scroll.pos - VisibleRows());
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			else if (wParam == VK_NEXT)
			{
				m_scroll.pos = std::min(m_scroll.MaxScroll(), m_scroll.pos + VisibleRows());
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			else if (wParam == VK_HOME)
			{
				m_selected = 0;
				m_scroll.pos = 0;
				InvalidateRect(hWnd, nullptr, FALSE);
				if (m_onSelect) m_onSelect(m_selected);
			}
			else if (wParam == VK_END && !m_rows.empty())
			{
				m_selected = (int)m_rows.size() - 1;
				EnsureVisible(m_selected);
				InvalidateRect(hWnd, nullptr, FALSE);
				if (m_onSelect) m_onSelect(m_selected);
			}
			else if (wParam == VK_F5)
			{
				SendMessage(GetParent(GetParent(hWnd)), msg, wParam, lParam);
			}
			return 0;

		case WM_CONTEXTMENU:
		{
			if (m_onContextMenu && m_selected >= 0)
			{
				int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
				m_onContextMenu(hWnd, x, y);
			}
			return 0;
		}

		case WM_SIZE:
			UpdateScrollInfo();
			InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

private:
	HWND m_hWnd = nullptr;
	HINSTANCE m_hInst = nullptr;
	HFONT m_hFont = nullptr;
	HFONT m_hFontBold = nullptr;
	std::vector<Column> m_columns;
	std::vector<std::vector<std::wstring>> m_rows;
	std::vector<LPARAM> m_itemData;
	int m_selected = -1;
	VScroll m_scroll;
	int m_rowHeight = 22;
	int m_headerHeight = 24;
	bool m_redrawEnabled = true;
	int m_scrollbarWidth = 14;
	int m_cellPadding = 6;
	int m_hot = -1;
	int m_headerDragCol = -1;
	int m_headerDragX = 0;

	SelectionCallback m_onSelect;
	ContextMenuCallback m_onContextMenu;

	int VisibleRows() const
	{
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		return std::max(1, static_cast<int>(rc.bottom - m_headerHeight) / m_rowHeight);
	}

	// Scroll position in this control is in row units.
	void SyncScrollGeometry()
	{
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		m_scroll.viewH = VisibleRows();
		m_scroll.contentH = (int)m_rows.size();
		m_scroll.trackTop = m_headerHeight;
		m_scroll.trackBottom = rc.bottom;
		m_scroll.sbX = rc.right - m_scrollbarWidth;
		m_scroll.sbRight = rc.right;
	}

	void UpdateScrollInfo()
	{
		SyncScrollGeometry();
		m_scroll.Clamp();
	}

	void EnsureVisible(int idx)
	{
		if (idx < 0) return;
		if (idx < m_scroll.pos) m_scroll.pos = idx;
		int vis = VisibleRows();
		if (idx >= m_scroll.pos + vis) m_scroll.pos = idx - vis + 1;
		UpdateScrollInfo();
	}

	int HitTestRow(int y) const
	{
		if (y < m_headerHeight) return -1;
		int row = (y - m_headerHeight) / m_rowHeight + m_scroll.pos;
		if (row >= (int)m_rows.size()) return -1;
		return row;
	}

	int HitTestHeaderEdge(int x) const
	{
		const int hitZone = Dpi::Scale(Layout::HeaderEdgeHitZone);
		int cx = 0;
		for (int i = 0; i < (int)m_columns.size(); i++)
		{
			cx += m_columns[i].width;
			if (x >= cx - hitZone && x <= cx + hitZone) return i;
		}
		return -1;
	}

	bool NeedsScrollbar() const { return (int)m_rows.size() > VisibleRows(); }

	void OnPaint()
	{
		SyncScrollGeometry();

		DoubleBuffer db(m_hWnd);
		const RECT& rc = db.rc;
		HDC memDC = db.dc;

		// Cached GDI objects (process-lifetime).
		static HPEN s_borderPen = CreatePen(PS_SOLID, 1, Dark::Border);
		static HBRUSH s_hotBrush = CreateSolidBrush(Dark::BgHot);
		static HBRUSH s_altRowBrush = CreateSolidBrush(RGB(35, 35, 38));
		static HBRUSH s_evenRowBrush = CreateSolidBrush(Dark::BgControl);

		FillRect(memDC, &rc, Dark::BrushControl());

		SelectObject(memDC, m_hFont);
		SetBkMode(memDC, TRANSPARENT);

		int contentRight = NeedsScrollbar() ? rc.right - m_scrollbarWidth : rc.right;

		// Header
		{
			RECT hdrRc = {0, 0, contentRight, m_headerHeight};
			FillRect(memDC, &hdrRc, Dark::BrushHeader());
			int cx = 0;
			SelectObject(memDC, m_hFontBold ? m_hFontBold : m_hFont);
			SetTextColor(memDC, Dark::TextDim);
			HPEN oldPen = (HPEN)SelectObject(memDC, s_borderPen);
			for (int i = 0; i < (int)m_columns.size(); i++)
			{
				RECT tr = {cx + m_cellPadding, 0, cx + m_columns[i].width, m_headerHeight};
				DrawTextExt(memDC, m_columns[i].text.c_str(), -1, tr,
					TextAlign::Left | TextAlign::VCenter | TextAlign::Ellipsis);
				cx += m_columns[i].width;
				MoveToEx(memDC, cx - 1, 0, nullptr);
				LineTo(memDC, cx - 1, m_headerHeight);
			}
			MoveToEx(memDC, 0, m_headerHeight - 1, nullptr);
			LineTo(memDC, contentRight, m_headerHeight - 1);
			SelectObject(memDC, oldPen);
		}

		// Rows / empty text
		SelectObject(memDC, m_hFont);
		if (m_rows.empty())
		{
			SetTextColor(memDC, Dark::TextDim);
			RECT emptyRc = {0, m_headerHeight, contentRight, rc.bottom};
			DrawTextExt(memDC, L"Empty", -1, emptyRc, TextAlign::Center | TextAlign::VCenter);
		}
		int vis = VisibleRows() + 1; // +1 to draw partially visible bottom row
		for (int vi = 0; vi < vis; vi++)
		{
			int rowIdx = m_scroll.pos + vi;
			if (rowIdx >= (int)m_rows.size()) break;

			int y = m_headerHeight + vi * m_rowHeight;
			RECT rowRc = {0, y, contentRight, y + m_rowHeight};

			if (rowIdx == m_selected)
			{
				FillRect(memDC, &rowRc, Dark::BrushSelected());
				SetTextColor(memDC, Dark::TextSel);
			}
			else if (rowIdx == m_hot)
			{
				FillRect(memDC, &rowRc, s_hotBrush);
				SetTextColor(memDC, Dark::Text);
			}
			else
			{
				FillRect(memDC, &rowRc, (rowIdx % 2 == 1) ? s_altRowBrush : s_evenRowBrush);
				SetTextColor(memDC, Dark::Text);
			}

			int cx = 0;
			for (int c = 0; c < (int)m_columns.size() && c < (int)m_rows[rowIdx].size(); c++)
			{
				RECT tr = {cx + m_cellPadding, y, cx + m_columns[c].width - m_cellPadding / 2, y + m_rowHeight};
				DrawTextExt(memDC, m_rows[rowIdx][c].c_str(), -1, tr,
					TextAlign::Left | TextAlign::VCenter | TextAlign::Ellipsis);
				cx += m_columns[c].width;
			}
		}

		m_scroll.Draw(memDC);
	}
};
