// PrintTreeView.cpp : implementation file
//

#include "stdafx.h"
#include "SysMate.h"
#include "PrintTreeView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPrintTreeView

IMPLEMENT_DYNCREATE(CPrintTreeView, CTreeView)

CPrintTreeView::CPrintTreeView()
{
}

CPrintTreeView::~CPrintTreeView()
{
}


BEGIN_MESSAGE_MAP(CPrintTreeView, CTreeView)
	//{{AFX_MSG_MAP(CPrintTreeView)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrintTreeView drawing

void CPrintTreeView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CPrintTreeView diagnostics

#ifdef _DEBUG
void CPrintTreeView::AssertValid() const
{
	CTreeView::AssertValid();
}

void CPrintTreeView::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CPrintTreeView message handlers

/////////////////////////////////////////////////////////////////////////////
// CPrintTreeView printing

#define LEFT_MARGIN 4
#define RIGHT_MARGIN 4
#define TOP_MARGIN 4
#define BOTTOM_MARGIN 4

BOOL CPrintTreeView::OnPreparePrinting(CPrintInfo* pInfo) 
{
	return DoPreparePrinting(pInfo);
}

void CPrintTreeView::OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo)
{
	HTREEITEM hItem=GetTreeCtrl().GetRootItem();
	GetTreeCtrl().GetItemRect(hItem,rcBounds,TRUE);
	m_nRowHeight = rcBounds.Height();

	// Find the total number of visible items & the right most coordinate
	int ItemCount=0;
	do
	{
		ItemCount++;
		CRect rc;
		GetTreeCtrl().GetItemRect(hItem,rc,TRUE);
		if (rc.right>rcBounds.right)
			rcBounds.right=rc.right;
		hItem=GetTreeCtrl().GetNextItem(hItem,TVGN_NEXTVISIBLE);
	}
	while (hItem);

	// Find the entire print boundary
	int ScrollMin,ScrollMax;
	GetScrollRange(SB_HORZ,&ScrollMin,&ScrollMax);
	rcBounds.left=0;
	if (ScrollMax>rcBounds.right)
		rcBounds.right=ScrollMax+1;
	rcBounds.top=0;
	rcBounds.bottom=m_nRowHeight*ItemCount;

	// Get text width
	CDC *pCtlDC = GetTreeCtrl().GetDC();
	if (NULL == pCtlDC) return;
	TEXTMETRIC tm;
	pCtlDC->GetTextMetrics(&tm);
	m_nCharWidth = tm.tmAveCharWidth;
	double d = (double)pDC->GetDeviceCaps(LOGPIXELSY)/(double)pCtlDC->GetDeviceCaps(LOGPIXELSY);
	ReleaseDC(pCtlDC);

	// Find rows per page
	int nPageHeight = pDC->GetDeviceCaps(VERTRES);
	m_nRowsPerPage = (int)((double)nPageHeight/d)/m_nRowHeight-TOP_MARGIN-BOTTOM_MARGIN;

	// Set maximum pages
	int pages=(ItemCount-1)/m_nRowsPerPage+1;
	pInfo->SetMaxPage(pages);

	// Create a memory DC compatible with the paint DC
	CPaintDC dc(this);
	CDC MemDC;
	MemDC.CreateCompatibleDC(&dc);

	// Select a compatible bitmap into the memory DC
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&dc, rcBounds.Width(), rcBounds.Height() );
	MemDC.SelectObject(&bitmap);

	// Enlarge window size to include the whole print area boundary
	GetWindowPlacement(&WndPlace);
	MoveWindow(0,0,::GetSystemMetrics(SM_CXEDGE)*2+rcBounds.Width(),
		::GetSystemMetrics(SM_CYEDGE)*2+rcBounds.Height(),FALSE);
	ShowScrollBar(SB_BOTH,FALSE);

	// Call the default printing
	GetTreeCtrl().EnsureVisible(GetTreeCtrl().GetRootItem());
	CWnd::DefWindowProc( WM_PAINT, (WPARAM)MemDC.m_hDC, 0 );

	// Now create a mask
	CDC MaskDC;
	MaskDC.CreateCompatibleDC(&dc);
	CBitmap maskBitmap;

	// Create monochrome bitmap for the mask
	maskBitmap.CreateBitmap( rcBounds.Width(), rcBounds.Height(), 1, 1, NULL );
	MaskDC.SelectObject( &maskBitmap );
	MemDC.SetBkColor( ::GetSysColor( COLOR_WINDOW ) );

	// Create the mask from the memory DC
	MaskDC.BitBlt( 0, 0, rcBounds.Width(), rcBounds.Height(), &MemDC,
		rcBounds.left, rcBounds.top, SRCCOPY );

	// Copy image to clipboard
	CBitmap clipbitmap;
	clipbitmap.CreateCompatibleBitmap(&dc, rcBounds.Width(), rcBounds.Height() );
	CDC clipDC;
	clipDC.CreateCompatibleDC(&dc);
	CBitmap* pOldBitmap = clipDC.SelectObject(&clipbitmap);
	clipDC.BitBlt( 0, 0, rcBounds.Width(), rcBounds.Height(), &MemDC,
		rcBounds.left, rcBounds.top, SRCCOPY);
	OpenClipboard();
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, clipbitmap.GetSafeHandle());
	CloseClipboard();
	clipDC.SelectObject(pOldBitmap);
	clipbitmap.Detach();

	// Copy the image in MemDC transparently
	MemDC.SetBkColor(RGB(0,0,0));
	MemDC.SetTextColor(RGB(255,255,255));
	MemDC.BitBlt(rcBounds.left, rcBounds.top, rcBounds.Width(), rcBounds.Height(),
		&MaskDC, rcBounds.left, rcBounds.top, MERGEPAINT);

	CPalette pal;
	hDIB=DDBToDIB(bitmap, BI_RGB, &pal );
}

void CPrintTreeView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo) 
{
	CTreeView::OnPrepareDC(pDC, pInfo);

	// Map logical unit of screen to printer unit
	pDC->SetMapMode(MM_ANISOTROPIC);
	CClientDC dcScreen(NULL);
	pDC->SetWindowExt(dcScreen.GetDeviceCaps(LOGPIXELSX),dcScreen.GetDeviceCaps(LOGPIXELSX));
	pDC->SetViewportExt(pDC->GetDeviceCaps(LOGPIXELSX),pDC->GetDeviceCaps(LOGPIXELSX));
}

void CPrintTreeView::OnPrint(CDC* pDC, CPrintInfo* pInfo) 
{
	// Save dc state
	int nSavedDC = pDC->SaveDC();

	// Set font
	CFont Font;
	LOGFONT lf;
	CFont *pOldFont = GetFont();
	pOldFont->GetLogFont(&lf);
	lf.lfHeight=m_nRowHeight-1;
	lf.lfWidth=0;
	Font.CreateFontIndirect(&lf);
	pDC->SelectObject(&Font);

	PrintHeadFoot(pDC,pInfo);
	pDC->SetWindowOrg(-1*(LEFT_MARGIN*m_nCharWidth),-m_nRowHeight*TOP_MARGIN);

	int height;
	if (pInfo->m_nCurPage==pInfo->GetMaxPage())
		height=rcBounds.Height()-((pInfo->m_nCurPage-1)*m_nRowsPerPage*m_nRowHeight);
	else
		height=m_nRowsPerPage*m_nRowHeight;
	int top=(pInfo->m_nCurPage-1)*m_nRowsPerPage*m_nRowHeight;

	// Print tree view from DIB
	LPBITMAPINFOHEADER lpbi;
	lpbi = (LPBITMAPINFOHEADER)hDIB;
	int nColors = lpbi->biClrUsed ? lpbi->biClrUsed : 1 << lpbi->biBitCount;
	BITMAPINFO &bmInfo = *(LPBITMAPINFO)hDIB;
	LPVOID lpDIBBits;
	if( bmInfo.bmiHeader.biBitCount > 8 )
		lpDIBBits = (LPVOID)((LPDWORD)(bmInfo.bmiColors + 
			bmInfo.bmiHeader.biClrUsed) + 
			((bmInfo.bmiHeader.biCompression == BI_BITFIELDS) ? 3 : 0));
	else
		lpDIBBits = (LPVOID)(bmInfo.bmiColors + nColors);
	HDC hDC=pDC->GetSafeHdc();
	StretchDIBits(hDC,				// hDC
		0,							// DestX
		0,							// DestY
		rcBounds.Width(),			// nDestWidth
		height,						// nDestHeight
		rcBounds.left,				// SrcX
		rcBounds.Height()-top-height,	// SrcY
		rcBounds.Width(),			// wSrcWidth
		height,						// wSrcHeight
		lpDIBBits,					// lpBits
		&bmInfo,					// lpBitsInfo
		DIB_RGB_COLORS,				// wUsage
		SRCCOPY);					// dwROP

	pDC->SelectObject(pOldFont);
	pDC->RestoreDC( nSavedDC );
}

void CPrintTreeView::OnEndPrinting(CDC* pDC, CPrintInfo* pInfo) 
{
	GlobalFree(hDIB);
	SetWindowPlacement(&WndPlace);
	RedrawWindow();
}

void CPrintTreeView::PrintHeadFoot(CDC *pDC, CPrintInfo *pInfo)
{
	CClientDC dcScreen(NULL);
	CRect rc;
	rc.top=m_nRowHeight*(TOP_MARGIN-2);
	rc.bottom = (int)((double)(pDC->GetDeviceCaps(VERTRES)*dcScreen.GetDeviceCaps(LOGPIXELSY))
		/(double)pDC->GetDeviceCaps(LOGPIXELSY));
	rc.left = LEFT_MARGIN*m_nCharWidth;
	rc.right = (int)((double)(pDC->GetDeviceCaps(HORZRES)*dcScreen.GetDeviceCaps(LOGPIXELSX))
		/(double)pDC->GetDeviceCaps(LOGPIXELSX))-RIGHT_MARGIN*m_nCharWidth;

	// Print App title on top left corner
	CString sTemp;
	sTemp=GetDocument()->GetTitle();
	sTemp+=" object hierarchy";
	CRect header(rc);
	header.bottom=header.top+m_nRowHeight;
	pDC->DrawText(sTemp, header, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);

	rc.top = rc.bottom - m_nRowHeight*(BOTTOM_MARGIN-1);
	rc.bottom = rc.top + m_nRowHeight;

	// Print draw page number at bottom center
	sTemp.Format("Page %d/%d",pInfo->m_nCurPage,pInfo->GetMaxPage());
	pDC->DrawText(sTemp,rc, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
}

HANDLE CPrintTreeView::DDBToDIB( CBitmap& bitmap, DWORD dwCompression, CPalette* pPal )
{
	BITMAP bm;
	BITMAPINFOHEADER bi;
	LPBITMAPINFOHEADER lpbi;
	DWORD dwLen;
	HANDLE hDIB;
	HANDLE handle;
	HDC hDC;
	HPALETTE hPal;

	ASSERT( bitmap.GetSafeHandle() );

	// The function has no arg for bitfields
	if ( dwCompression == BI_BITFIELDS )
		return NULL;

	// If a palette has not been supplied use defaul palette
	hPal = (HPALETTE) pPal->GetSafeHandle();
	if (hPal==NULL)
		hPal = (HPALETTE) GetStockObject(DEFAULT_PALETTE);

	// Get bitmap information
	bitmap.GetObject(sizeof(bm),(LPSTR)&bm);

	// Initialize the bitmapinfoheader
	bi.biSize               = sizeof(BITMAPINFOHEADER);
	bi.biWidth              = bm.bmWidth;
	bi.biHeight             = bm.bmHeight;
	bi.biPlanes             = 1;
	bi.biBitCount           = bm.bmPlanes * bm.bmBitsPixel;
	bi.biCompression        = dwCompression;
	bi.biSizeImage          = 0;
	bi.biXPelsPerMeter      = 0;
	bi.biYPelsPerMeter      = 0;
	bi.biClrUsed            = 0;
	bi.biClrImportant       = 0;

	// Compute the size of the  infoheader and the color table
	int nColors = (1 << bi.biBitCount);
	if ( nColors > 256 ) 
		nColors = 0;
	dwLen = bi.biSize + nColors * sizeof(RGBQUAD);

	// We need a device context to get the DIB from
	hDC = ::GetDC(NULL);
	hPal = SelectPalette(hDC,hPal,FALSE);
	RealizePalette(hDC);

	// Allocate enough memory to hold bitmapinfoheader and color table
	hDIB = GlobalAlloc(GMEM_FIXED,dwLen);

	if (!hDIB)
	{
		SelectPalette(hDC,hPal,FALSE);
		::ReleaseDC(NULL,hDC);
		return NULL;
	}

	lpbi = (LPBITMAPINFOHEADER)hDIB;

	*lpbi = bi;

	// Call GetDIBits with a NULL lpBits param, so the device driver 
	// will calculate the biSizeImage field 
	GetDIBits(hDC, (HBITMAP)bitmap.GetSafeHandle(), 0L, (DWORD)bi.biHeight,
		(LPBYTE)NULL, (LPBITMAPINFO)lpbi, (DWORD)DIB_RGB_COLORS);

	bi = *lpbi;

	// If the driver did not fill in the biSizeImage field, then compute it
	// Each scan line of the image is aligned on a DWORD (32bit) boundary
	if (bi.biSizeImage == 0)
	{
		bi.biSizeImage = ((((bi.biWidth * bi.biBitCount) + 31) & ~31) / 8) 
			* bi.biHeight;

		// If a compression scheme is used the result may infact be larger
		// Increase the size to account for this.
		if (dwCompression != BI_RGB)
			bi.biSizeImage = (bi.biSizeImage * 3) / 2;
	}

	// Realloc the buffer so that it can hold all the bits
	dwLen += bi.biSizeImage;
	if (handle = GlobalReAlloc(hDIB, dwLen, GMEM_MOVEABLE))
		hDIB = handle;
	else
	{
		GlobalFree(hDIB);

		// Reselect the original palette
		SelectPalette(hDC,hPal,FALSE);
		::ReleaseDC(NULL,hDC);
		return NULL;
	}

	// Get the bitmap bits
	lpbi = (LPBITMAPINFOHEADER)hDIB;

	// FINALLY get the DIB
	BOOL bGotBits = GetDIBits( hDC, (HBITMAP)bitmap.GetSafeHandle(),
							0L,                             // Start scan line
							(DWORD)bi.biHeight,             // # of scan lines
							(LPBYTE)lpbi                    // address for bitmap bits
							+ (bi.biSize + nColors * sizeof(RGBQUAD)),
							(LPBITMAPINFO)lpbi,             // address of bitmapinfo
							(DWORD)DIB_RGB_COLORS);         // Use RGB for color table

	if( !bGotBits )
	{
		GlobalFree(hDIB);

		SelectPalette(hDC,hPal,FALSE);
		::ReleaseDC(NULL,hDC);
		return NULL;
	}

	SelectPalette(hDC,hPal,FALSE);
	::ReleaseDC(NULL,hDC);
	return hDIB;
}


