// ComView.cpp : implementation file
//

#include "stdafx.h"
#include "SysMate.h"
#include "ComView.h"

#include "ConvertString.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CComView

IMPLEMENT_DYNCREATE(CComView, CListView)

CComView::CComView()
{
}

CComView::~CComView()
{
}


BEGIN_MESSAGE_MAP(CComView, CListView)
	//{{AFX_MSG_MAP(CComView)
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CComView drawing

void CComView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CComView diagnostics

#ifdef _DEBUG
void CComView::AssertValid() const
{
	CListView::AssertValid();
}

void CComView::Dump(CDumpContext& dc) const
{
	CListView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CComView message handlers

BOOL CComView::PreCreateWindow(CREATESTRUCT& cs) 
{
   m_dwDefaultStyle |= LVS_REPORT | LVS_SORTASCENDING | LVS_SHAREIMAGELISTS;

	return CListView::PreCreateWindow(cs);
}

void CComView::OnInitialUpdate() 
{
	CListView::OnInitialUpdate();
	
   GetListCtrl().InsertColumn(0, "Object", LVCFMT_LEFT, 150);
   GetListCtrl().InsertColumn(1, "ProgID", LVCFMT_LEFT, 150);
   GetListCtrl().InsertColumn(2, "Server", LVCFMT_LEFT, 400);

	/// Attach image list to List
	GetListCtrl().SetImageList(&theApp.m_SystemImageList, LVSIL_SMALL);


   PopulateRunningObjects();
	
}

void CComView::Error(LPCSTR sz, HRESULT hr)
{
   AfxMessageBox(sz);
}

void CComView::PopulateRunningObjects()
{
   
   // Get a BindCtx.
   IBindCtx *pbc;
   HRESULT hr = CreateBindCtx(0, &pbc);
   if(FAILED(hr)) 
   {
      Error("CreateBindCtx()", hr);
      return;
   }
   
   // Get running-object table.
   IRunningObjectTable *prot;
   hr = pbc->GetRunningObjectTable(&prot);

   if(FAILED(hr)) 
   {
      Error("GetRunningObjectTable()", hr);
      pbc->Release();
      return;
   }
   
   // Get enumeration interface.
   IEnumMoniker *pem;
   hr = prot->EnumRunning(&pem);

   if(FAILED(hr)) 
   {
      Error("EnumRunning()", hr);
      prot->Release();
      pbc->Release();
      return;
   }
   
   // Start at the beginning.
   pem->Reset();
   
   // Churn through enumeration.
   ULONG fetched;
   IMoniker *pmon;
   int n = 0;
   while(pem->Next(1, &pmon, &fetched) == S_OK) 
   {
      
      // Get DisplayName.
      LPOLESTR pName;
      pmon->GetDisplayName(pbc, NULL, &pName);
      
      // Convert it to ASCII.
      char szName[512];
      WideCharToMultiByte(CP_ACP, 0, pName, -1, szName, 512, NULL,
         NULL);

      CString str, strServerName, strProgId;
      CString strGUID = szName + 1;
      CRegistry r(HKEY_CLASSES_ROOT);

      str.Format("CLSID\\%s\\ProgID", strGUID);
      r.LoadKey(str, "", strProgId);
      str.Format("CLSID\\%s\\LocalServer32", strGUID);
      
      if (!r.LoadKey(str, "", strServerName))
      {
         str.Format("CLSID\\%s\\InprocServer32", strGUID);
         r.LoadKey(str, "", strServerName);
      }
      
      
      HIMAGELIST  himl;
      SHFILEINFO  sfi;
      
      himl = (HIMAGELIST)SHGetFileInfo(strServerName, 0, &sfi,
         sizeof(SHFILEINFO), SHGFI_SYSICONINDEX);
      
      int iIcon = (himl) ? sfi.iIcon : 0;
      
      int nItem = GetListCtrl().InsertItem(0, szName, iIcon);
      
      
      
      
      /*str2.Format("CLSID\\%s", strGUID);
      
        if (!r.LoadKey(str2, "", str))
        {
        CLSID ClassID;
        pmon->GetClassID(&ClassID);
        ProgIDFromCLSID(ClassID, &pName);
        
          StringFromCLSID(ClassID, &pName);
          
            WideCharToMultiByte(CP_ACP, 0, pName, -1, szName, 512, NULL, NULL);
            
              
                strGUID = szName;
   }*/
      

      if (!strProgId.IsEmpty())
         GetListCtrl().SetItem(nItem, 1, LVIF_TEXT, strProgId, 0, 0, 0, 0);

      GetListCtrl().SetItem(nItem, 2, LVIF_TEXT, strServerName, 0, 0, 0, 0);
      
      
      
      // Compare it against the name we got in SetHostNames().
      /*if(!strcmp(szName, m_szDocName)) {
      
        Error("Found document in ROT!");
        
          // Bind to this ROT entry.
          IDispatch *pDisp;
          hr = pmon->BindToObject(pbc, NULL, IID_IDispatch, (void
          **)&pDisp);
          if(!FAILED(hr)) {
          // Remember IDispatch.
          m_pDocDisp = pDisp;
          
            // Notice…
            sprintf(buf, "Document IDispatch = %08lx",
            m_pDocDisp);
            Error(buf);
            }
            else {
            Error("BindToObject()", hr);
            }
   }*/
      
      // Release interfaces.
      pmon->Release();
      
   }
   
   // Release interfaces.
   pem->Release();
   prot->Release();
   pbc->Release();
}




void CComView::OnViewRefresh() 
{
	GetListCtrl().DeleteAllItems();
   PopulateRunningObjects();
}

