// WindowView.cpp : implementation file
//

#include "stdafx.h"
#include "SysMate.h"
#include "WindowView.h"
#include "SysMateDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWindowView

IMPLEMENT_DYNCREATE(CWindowView, CProcessView)

CWindowView::CWindowView()
{
}

CWindowView::~CWindowView()
{
}


BEGIN_MESSAGE_MAP(CWindowView, CProcessView)
	//{{AFX_MSG_MAP(CWindowView)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWindowView drawing

void CWindowView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CWindowView diagnostics

#ifdef _DEBUG
void CWindowView::AssertValid() const
{
	CProcessView::AssertValid();
}

void CWindowView::Dump(CDumpContext& dc) const
{
	CProcessView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWindowView message handlers




void CWindowView::PopulateChild(HTREEITEM hTreeItem, int nProcId)
{
   // Is already done?
   if (GetTreeCtrl ().GetChildItem (hTreeItem) != NULL)
      return;

   LockWindowUpdate();

   m_mapWindows.RemoveAll( );
   m_nEnumProcId = nProcId;
   m_hEnumTreeRoot = hTreeItem;

   HWND hWnd = ::GetDesktopWindow();
   EnumChildWindows(hWnd, EnumChildProc, (LPARAM) this);

   UnlockWindowUpdate();

   return;
}

BOOL CALLBACK CWindowView::EnumChildProc(HWND hwnd, LPARAM lParam)
{
   CWindowView* pThis = (CWindowView*) lParam;

   DWORD dwProcessId = 0;
   GetWindowThreadProcessId(hwnd, &dwProcessId);

   if (dwProcessId != pThis->m_nEnumProcId)
      return TRUE;

   //CSysMateDoc* pDoc = pThis->GetDocument();
	//ASSERT_VALID(pDoc);

   char szClassName[64];
   szClassName[0] = 0;
   GetClassName(hwnd, szClassName, sizeof(szClassName));

   CWnd *pWnd = CWnd::FromHandle(hwnd);

   CString strWindowText, str;
   pWnd->GetWindowText(str);
   strWindowText.Format("%s (%s)", str ,szClassName);

   HTREEITEM hParent;
   if (!pThis->m_mapWindows.Lookup(::GetParent(hwnd), hParent))
      hParent = pThis->m_hEnumTreeRoot;

   /*CString strExeName = pDoc->m_ProcessAPI.GetProcessExecutableName(dwProcessId);

   HIMAGELIST  himl;
   SHFILEINFO  sfi;

   himl = (HIMAGELIST)SHGetFileInfo(strExeName, 0, &sfi,
   sizeof(SHFILEINFO), SHGFI_SYSICONINDEX);

   int iIcon = (himl) ? sfi.iIcon : 0;*/

   int iIcon = 0;



   TVINSERTSTRUCT tvi; 
   
   tvi.item.mask = TVIF_TEXT | TVIF_IMAGE 
      | TVIF_SELECTEDIMAGE | TVIF_PARAM;// | TVIF_CHILDREN; 
   
   // Set the text of the item. 
   tvi.item.pszText = (LPTSTR)(LPCTSTR)strWindowText; 
   tvi.item.cchTextMax = strWindowText.GetLength();
   //tvi.item.cChildren = 1;
   tvi.item.iImage = iIcon; 
   tvi.item.iSelectedImage = iIcon; 
   tvi.item.lParam = (LPARAM) hwnd; 
   tvi.hInsertAfter = TVI_SORT; 
   tvi.hParent = hParent; 
   
   HTREEITEM h = pThis->GetTreeCtrl().InsertItem(&tvi);

   pThis->m_mapWindows.SetAt(hwnd, h);
   
   return TRUE;
}


