// ProcessView.cpp : implementation of the CProcessView class
//

#include "stdafx.h"
#include "SysMate.h"

#include "SysMateDoc.h"
#include "ProcessView.h"
#include "FileVersion.h"
#include "ProcessFindReplaceDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static UINT WM_FINDREPLACE = ::RegisterWindowMessage(FINDMSGSTRING);



/////////////////////////////////////////////////////////////////////////////
// CProcessView

IMPLEMENT_DYNCREATE(CProcessView, CPrintTreeView)

BEGIN_MESSAGE_MAP(CProcessView, CPrintTreeView)
	//{{AFX_MSG_MAP(CProcessView)
	ON_NOTIFY_REFLECT(TVN_GETDISPINFO, OnGetdispinfo)
	ON_NOTIFY_REFLECT(TVN_ITEMEXPANDING, OnItemexpanding)
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	ON_COMMAND(ID_EDIT_KILL, OnEditKill)
	ON_UPDATE_COMMAND_UI(ID_EDIT_KILL, OnUpdateEditKill)
   ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBegindrag)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_FIND, OnEditFind)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FIND, OnUpdateEditFind)
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CPrintTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CPrintTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CPrintTreeView::OnFilePrintPreview)
   ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify)
   ON_REGISTERED_MESSAGE( WM_FINDREPLACE, OnFindReplace )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProcessView construction/destruction

CProcessView::CProcessView()
{
	m_hToolTipItem = NULL;
   m_pFindDialog = NULL;
}

CProcessView::~CProcessView()
{
   EmptyClipboard();

   if (m_pFindDialog)
      m_pFindDialog->DestroyWindow();

}

BOOL CProcessView::PreCreateWindow(CREATESTRUCT& cs)
{
	m_dwDefaultStyle |= TVS_HASLINES | TVS_NOTOOLTIPS;
	m_dwDefaultStyle |= TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;

   
   
	return CPrintTreeView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CProcessView drawing

void CProcessView::OnDraw(CDC* pDC)
{
	CSysMateDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	// TODO: add draw code for native data here
}

void CProcessView::OnInitialUpdate()
{
	CPrintTreeView::OnInitialUpdate();

 	// Create the Image List
	//m_ctlImage.Create(IDB_IMAGELIST,16,0,RGB(255,0,255));
	//m_ctlImage.SetBkColor(GetSysColor(COLOR_WINDOW));

	/// Attach image list to Tree
	CTreeCtrl& ctlTree = (CTreeCtrl&) GetTreeCtrl();
	ctlTree.SetImageList(&theApp.m_SystemImageList, TVSIL_NORMAL);

 	// Tool tips
   if (m_ToolTip.Create(this, TTS_ALWAYSTIP) && m_ToolTip.AddTool(this))
   {
      m_ToolTip.SetMaxTipWidth(600);
   }
   else
   {
      TRACE("Error in creating ToolTip");
      // We can still function without tool tips
      // so do not exit with -1
   }

   PopulateProcessList();
}




/////////////////////////////////////////////////////////////////////////////
// CProcessView diagnostics

#ifdef _DEBUG
void CProcessView::AssertValid() const
{
	CPrintTreeView::AssertValid();
}

void CProcessView::Dump(CDumpContext& dc) const
{
	CPrintTreeView::Dump(dc);
}

CSysMateDoc* CProcessView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSysMateDoc)));
	return (CSysMateDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CProcessView message handlers

void CProcessView::PopulateProcessList()
{
   CWaitCursor wait;

 	CSysMateDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CMapStringToString PIDNameMap;
	CString PIDString;
	DWORD nPID=0;
	DWORD nCurrentPID=::GetCurrentProcessId();
	POSITION pos = NULL;

   if (pDoc->m_ProcessAPI.BuildProcessList(PIDNameMap)) 
   {
      
      pos = PIDNameMap.GetStartPosition();
      while( pos != NULL )
      {
         CString ProcessName;
         CString PIDString;
         HTREEITEM hItem = TVI_ROOT;
         // Get key ( PIDString ) and value ( ProcessName )
         PIDNameMap.GetNextAssoc( pos, PIDString, ProcessName );
         nPID = atol(PIDString);
         
         CString str = pDoc->m_ProcessAPI.GetProcessExecutableName(nPID);

         HIMAGELIST  himl;
         SHFILEINFO  sfi;

         himl = (HIMAGELIST)SHGetFileInfo(str, 0, &sfi,
            sizeof(SHFILEINFO), SHGFI_SYSICONINDEX);
         
         int iIcon = (himl) ? sfi.iIcon : 0;
         
         
         TVINSERTSTRUCT tvi; 
         
         tvi.item.mask = TVIF_TEXT | TVIF_IMAGE 
            | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN; 
         
         // Set the text of the item. 
         tvi.item.pszText = (LPTSTR)(LPCTSTR)ProcessName; 
         tvi.item.cchTextMax = lstrlen(tvi.item.pszText);
         tvi.item.cChildren = 1;
         tvi.item.iImage = iIcon; //(nCurrentPID == nPID) ? 11 : 10; 
         tvi.item.iSelectedImage = tvi.item.iImage; 
         tvi.item.lParam = (LPARAM) nPID; 
         tvi.hInsertAfter = TVI_SORT; 
         tvi.hParent = TVI_ROOT; 
         
         hItem = GetTreeCtrl().InsertItem(&tvi);
      }
      
   } 
   else 
   {
      
      AfxMessageBox("Fatal error: cannot build list of processes", 
         MB_OK|MB_ICONERROR);
      
   }
}

void CProcessView::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult) 
{
	TV_DISPINFO* pTVDispInfo = (TV_DISPINFO*)pNMHDR;
	// TODO: Add your control notification handler code here

   // Set up the count of children 
   /*if (pTVDispInfo->item.mask & TVIF_CHILDREN) 
   {
      pTVDispInfo->item.cChildren = 1; 
   } */
	
	*pResult = 0;
}

void CProcessView::OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	

   TV_ITEM   tvi = pNMTreeView->itemNew;

   if (pNMTreeView->action == TVE_EXPAND)
   {
      PopulateChild(tvi.hItem, tvi.lParam);
   }
   /*else if (pNMTreeView->action == TVE_COLLAPSERESET)
   {
      HTREEITEM hChild = GetTreeCtrl ().GetChildItem (tvi.hItem);

      while (hChild != NULL) 
      {
         HTREEITEM hNextItem = GetTreeCtrl ().GetNextSiblingItem (hChild);

         GetTreeCtrl ().DeleteItem (hChild);
         hChild = hNextItem;
      }
   }*/
        
	
	*pResult = 0;
}

void CProcessView::PopulateChild(HTREEITEM hItemParent, int nPID)
{
   // Is already done?
   if (GetTreeCtrl ().GetChildItem (hItemParent) != NULL)
      return;


  	CSysMateDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CStringList ModuleList;
	int nRecords=0;
	int nFields=0;

	if (pDoc->m_ProcessAPI.BuildModuleList(nPID, ModuleList)) 
   {

		POSITION pos = ModuleList.GetHeadPosition();

		while( pos != NULL ){

			nFields=0;

			// Get file name
			CString FileName = ModuleList.GetNext(pos);

			// Get file version info
		
         HIMAGELIST  himl;
         SHFILEINFO  sfi;

         himl = (HIMAGELIST)SHGetFileInfo(FileName, 
            0, &sfi,
            sizeof(SHFILEINFO), SHGFI_SYSICONINDEX);
         
         int iIcon = (himl) ? sfi.iIcon : 0;



         GetTreeCtrl().InsertItem(FileName, iIcon, iIcon, 
            hItemParent, TVI_SORT);

         nRecords++;
		}

	} else {

			AfxMessageBox("Fatal error: cannot build list of modules", 
						  MB_OK|MB_ICONERROR);

	}


}

BOOL CProcessView::PreTranslateMessage(MSG* pMsg) 
{
	if (::IsWindow(m_ToolTip.m_hWnd) && pMsg->hwnd == m_hWnd)
    {
        switch(pMsg->message)
        {
        case WM_LBUTTONDOWN:    
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:    
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:    
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            m_ToolTip.RelayEvent(pMsg);
            break;
        }
    }
	
	return CPrintTreeView::PreTranslateMessage(pMsg);
}

void CProcessView::OnMouseMove(UINT nFlags, CPoint point) 
{


	 if (::IsWindow(m_ToolTip.m_hWnd))
    {
		CTreeCtrl& Tree = GetTreeCtrl();

		UINT Flags;

		HTREEITEM h = Tree.HitTest(point, &Flags);

      if (TVHT_ONITEMLABEL | Flags)
      {

		   if (m_hToolTipItem != h)//) && (TVHT_ONITEM & Flags))
           {
               // Use Activate() to hide the tooltip.
               m_ToolTip.Activate(FALSE);
			      m_hToolTipItem = h;
			   
  	            if (h != NULL)
			      {
				      m_ToolTip.Activate(TRUE);
			      }
		   }
      }
      else
      {
         m_hToolTipItem = NULL;
         m_ToolTip.Activate(FALSE);
      }
    }
	
	CPrintTreeView::OnMouseMove(nFlags, point);
}

BOOL CProcessView::OnToolTipNotify(UINT id, NMHDR *pNMHDR, LRESULT *pResult)
{


    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
	CTreeCtrl& Tree = GetTreeCtrl();
   CSysMateDoc* pDoc = GetDocument();
   BOOL bHandeled = FALSE;

 	if (m_hToolTipItem)
	{
      LPARAM lParam = Tree.GetItemData(m_hToolTipItem);
      m_strToolTip.Empty();

      if (lParam == 0)
      {
         m_strToolTip = GetModuleDesc(Tree.GetItemText(m_hToolTipItem));
      }
      else
      {
         CString str = pDoc->m_ProcessAPI.GetProcessExecutableName(lParam);

         m_strToolTip = GetModuleDesc(str);
      }

		pTTT->lpszText = (LPSTR)(LPCSTR)m_strToolTip;
      bHandeled = TRUE;
	}

    return bHandeled;
}

CString CProcessView::GetModuleDesc(const CString &FileName)
{
   if (FileName.IsEmpty())
      return "";

   CString str, strOut;
   CFileVersion fileVersion;
   CString FileDescription;
   CString FileVersion;
   CString ProductName;
   CString ProductVersion;
   CString FileModificationDateString;
   CString FileSizeString;


	if (fileVersion.Open(FileName)) 
   {
			FileDescription = fileVersion.GetFileDescription();
			FileVersion = fileVersion.GetFileVersion();
			ProductName = fileVersion.GetProductName();
			ProductVersion = fileVersion.GetProductVersion();
			fileVersion.Close();
		} else {
			FileDescription = "";
			FileVersion = "";
			ProductName = "";
			ProductVersion = "";
		}
		// Get file size and date
		CFileStatus FileStatus;
		CTime FileModificationDate;
		LONG FileSize=0;
		if (CFile::GetStatus(FileName, FileStatus)) {
			FileModificationDate = FileStatus.m_mtime;
			FileModificationDateString = FileModificationDate.Format("%B %d, %Y %H:%M:%S" );
			FileSize = FileStatus.m_size;
			FileSizeString.Format("%d",FileSize);
		} else {
			// Error: can't get file status
			FileModificationDateString = "?";
			FileSizeString = "?";
		}

		// Log it in debug window
		str.Format("Module File Name = \"%s\"\r\n", FileName);
      strOut = str;
		str.Format(" File Version    = \"%s\"\r\n", FileVersion);
      strOut += str;
		str.Format(" File Size       = %s bytes\r\n", FileSizeString);
      strOut += str;
		str.Format(" File Date       = \"%s\"\r\n", FileModificationDateString);
      strOut += str;
		str.Format(" File Descrition = \"%s\"\r\n", FileDescription);
      strOut += str;
		str.Format(" Product Name    = \"%s\"\r\n", ProductName);
      strOut += str;
		str.Format(" Product Version = \"%s\"", ProductVersion);
      strOut += str;

   return strOut;
}

void CProcessView::OnViewRefresh() 
{
   GetTreeCtrl ().LockWindowUpdate();
   GetTreeCtrl ().DeleteAllItems();
   GetTreeCtrl ().UnlockWindowUpdate();
   PopulateProcessList();	
}



void CProcessView::OnEditKill() 
{
   CWaitCursor wait;

	HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();

   if (hItem != NULL) 
   {

      DWORD dwProcessId = GetTreeCtrl().GetItemData(hItem);

      HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE,
                                    dwProcessId);
      if (hProcess != NULL) 
      {
         TerminateProcess(hProcess, 1);
         CloseHandle(hProcess);

         ::WaitForSingleObject(hProcess, 500);

         OnViewRefresh();
      }

   }
}

void CProcessView::OnUpdateEditKill(CCmdUI* pCmdUI) 
{
	HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();

   pCmdUI->Enable((hItem != NULL) 
      && GetTreeCtrl().GetItemData(hItem));
	
}

void CProcessView::OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult) 
{
   NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
   HGLOBAL hMem = GetSelectedFile();
      
   if (hMem)
   {   
      COleDataSource source;
      
      source.CacheGlobalData(CF_HDROP, hMem);

      source.DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_LINK);
      
   }
   
   *pResult = 0;
}



void CProcessView::OnEditCopy() 
{
   HGLOBAL hMem = GetSelectedFile();
   HGLOBAL hMemText = GetSelectedFileText();
      
   if (hMem && hMemText)
   { 
	   COleDataSource	*pSource = new COleDataSource();

	   pSource->CacheGlobalData(CF_HDROP, hMem);
      pSource->CacheGlobalData(CF_TEXT, hMemText);

	   pSource->SetClipboard();
   }
	
}

HGLOBAL CProcessView::GetSelectedFile()
{
   HGLOBAL hMem = NULL;
   
   HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();
   
   if (hItem != NULL) 
   {
      CSysMateDoc* pDoc = GetDocument();
      
      DWORD dwProcessId = GetTreeCtrl().GetItemData(hItem);
      CString str = pDoc->m_ProcessAPI.GetProcessExecutableName(dwProcessId);

      if (str.GetLength())
      {
      
         CSharedFile	sf(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT);
      
         DROPFILES df;
         ZeroMemory(&df, sizeof(df));
         df.pFiles = sizeof(df);
         sf.Write(&df, sizeof(df));
      
         sf.Write(str, str.GetLength()); // You can write to the clipboard as you would to any CFile
      
         TCHAR sz[10]; ZeroMemory(sz, sizeof(sz));
         sf.Write(sz, sizeof(sz)); // You can write to the clipboard as you would to any CFile
      
         hMem = sf.Detach();
      }
   }
   
   return hMem;
}

HGLOBAL CProcessView::GetSelectedFileText()
{
   HGLOBAL hMem = NULL;
   
   HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();
   
   if (hItem != NULL) 
   {
      CSysMateDoc* pDoc = GetDocument();
      
      DWORD dwProcessId = GetTreeCtrl().GetItemData(hItem);
      CString strText;

      if (dwProcessId == 0)
         return NULL;

      CString str = pDoc->m_ProcessAPI.GetProcessExecutableName(dwProcessId);
      strText = GetModuleDesc(str);

      if (str.GetLength())
      {
      
         CSharedFile	sf(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT);
         sf.Write(strText, strText.GetLength()); // You can write to the clipboard as you would to any CFile
         hMem = sf.Detach();
      }
   }
   
   return hMem;
}

 
   
   
// FindItem		- Finds an item that contains the search string
// Returns		- Handle to the item or NULL
// str			- String to search for
// bCaseSensitive	- Should the search be case sensitive
// bDownDir		- Search direction - TRUE for down
// bWholeWord		- True if search should match whole words
// hItem		- Item to start searching from. NULL for
//			- currently selected item
HTREEITEM CProcessView::FindItem(CString &str, 
				BOOL bCaseSensitive /*= FALSE*/, 
				BOOL bDownDir /*= TRUE*/, 
				BOOL bWholeWord /*= FALSE*/, 
				HTREEITEM hItem /*= NULL*/)
{
   int lenSearchStr = str.GetLength();

   if( lenSearchStr == 0 ) 
      return NULL;
   
   HTREEITEM htiSel = hItem ? hItem : GetTreeCtrl().GetSelectedItem() ? GetTreeCtrl().GetSelectedItem() : GetTreeCtrl().GetRootItem();
   HTREEITEM htiCur = bDownDir ? GetNextItem( htiSel ) : GetPrevItem( htiSel );
   
   CString sSearch = str;
   
   if (htiCur == NULL) 
   {
      if( bDownDir )
         htiCur = GetTreeCtrl().GetRootItem();
      else
         htiCur = GetLastItem( NULL );
   }
   
   if( !bCaseSensitive )
      sSearch.MakeLower();
   
   while(htiCur && (htiCur != htiSel))
   {
      CString sItemText = GetTreeCtrl().GetItemText( htiCur );

      if( !bCaseSensitive )
         sItemText.MakeLower();
      
      int n;
      while( (n = sItemText.Find( sSearch )) != -1 )
      {
         // Search string found
         if( bWholeWord )
         {
            // Check preceding char
            if( n != 0 )
            {
               if( isalpha(sItemText[n-1]) || 
                  sItemText[n-1] == '_' ){
                  // Not whole word
                  sItemText = sItemText.Right(
                     sItemText.GetLength() - n - 
                     lenSearchStr );
                  continue;
               }
            }
            
            // Check succeeding char
            if( sItemText.GetLength() > n + lenSearchStr
               && ( isalpha(sItemText[n+lenSearchStr]) ||
               sItemText[n+lenSearchStr] == '_' ) )
            {
               // Not whole word
               sItemText = sItemText.Right( sItemText.GetLength() 
                  - n - sSearch.GetLength() );
               continue;
            }
         }
         
         if( IsFindValid( htiCur ) )
            return htiCur;
         else 
            break;
      }
      
      
      htiCur = bDownDir ? GetNextItem( htiCur ) : GetPrevItem( htiCur );

      if( htiCur == NULL )
      {
         if( bDownDir )  
            htiCur = GetTreeCtrl().GetRootItem();
         else 
            htiCur = GetLastItem( NULL );
      }
   }

   return NULL;
}


// IsFindValid	- Virtual function used by FindItem to allow this
//		  function to filter the result of FindItem
// Returns	- True if item matches the criteria
// Arg		- Handle of the item
BOOL CProcessView::IsFindValid( HTREEITEM )
{
	return TRUE;
}

// GetNextItem  - Get previous item as if outline was completely expanded
// Returns              - The item immediately above the reference item
// hItem                - The reference item
HTREEITEM CProcessView::GetPrevItem( HTREEITEM hItem )
{
   HTREEITEM       hti;
   
   hti = GetTreeCtrl().GetPrevSiblingItem(hItem);

   if( hti == NULL )
      hti = GetTreeCtrl().GetParentItem(hItem);
   else
      hti = GetLastItem(hti);

   return hti;
}

// GetNextItem  - Get next item as if outline was completely expanded
// Returns      - The item immediately below the reference item
// hItem        - The reference item
HTREEITEM CProcessView::GetNextItem( HTREEITEM hItem )
{
   HTREEITEM       hti = NULL;
   
   if( GetTreeCtrl().ItemHasChildren( hItem ) )
   {
      LPARAM lParam = GetTreeCtrl().GetItemData(hItem);
      PopulateChild(hItem, lParam);
      
      return GetTreeCtrl().GetChildItem( hItem );           // return first child
   }
   else
   {
      // return next sibling item
      // Go up the tree to find a parent's sibling if needed.
      while( (hti = GetTreeCtrl().GetNextSiblingItem( hItem )) == NULL )
      {
         if( (hItem = GetTreeCtrl().GetParentItem( hItem ) ) == NULL )
            return NULL;
      }
   }

   return hti;
}

// GetLastItem  - Gets last item in the branch
// Returns      - Last item
// hItem        - Node identifying the branch. NULL will 
//                return the last item in outine
HTREEITEM CProcessView::GetLastItem( HTREEITEM hItem )
{
   // Last child of the last child of the last child ...
   HTREEITEM htiNext;
   
   if( hItem == NULL ){
      // Get the last item at the top level
      htiNext = GetTreeCtrl().GetRootItem();
      while( htiNext )
      {
         hItem = htiNext;
         htiNext = GetTreeCtrl().GetNextSiblingItem( htiNext );
      }
   }
   
   while( GetTreeCtrl().ItemHasChildren( hItem ) )
   {
      LPARAM lParam = GetTreeCtrl().GetItemData(hItem);
      PopulateChild(hItem, lParam);

      htiNext = GetTreeCtrl().GetChildItem( hItem );

      while( htiNext )
      {
         hItem = htiNext;
         htiNext = GetTreeCtrl().GetNextSiblingItem( htiNext );
      }
   }
   
   return hItem;
}





void CProcessView::OnEditFind() 
{
   if (m_pFindDialog == NULL)
   {
      m_pFindDialog = new CProcessFindReplaceDialog(this);
      m_pFindDialog->Create(TRUE, "", NULL, FR_DOWN, this);
   }
   else
   {
      m_pFindDialog->DestroyWindow();
   }
	
}

void CProcessView::OnUpdateEditFind(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_pFindDialog != NULL);
}


void CProcessView::CloseFindDialog()
{
   if (m_pFindDialog)
   {
      m_pFindDialog = NULL;
   }

}

LRESULT CProcessView::OnFindReplace(WPARAM wparam, LPARAM lparam)
{
   CFindReplaceDialog *pDlg = CFindReplaceDialog::GetNotifier(lparam);

   if( NULL != pDlg && !pDlg->IsTerminating())
   {
      CWaitCursor wait;
      // Use pDlg as a pointer to the existing FindReplace dlg to 
      // call CFindReplaceDialog member functions

      HTREEITEM h = FindItem(pDlg->GetFindString(), 
				pDlg->MatchCase(), 
				pDlg->SearchDown(), 
				pDlg->MatchWholeWord());

      if (h)
      {
         GetTreeCtrl().EnsureVisible(h);
         GetTreeCtrl().Select(h, TVGN_CARET);
      }
      else
      {
         AfxMessageBox(IDS_ITEM_NOT_FOUND);
      }

   }

   return 0;
}


