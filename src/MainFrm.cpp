// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "SysMate.h"

#include "MainFrm.h"
#include "NetworkView.h"
#include "ComView.h"
#include "WindowView.h"
#include "Splash.h"

#include "Performance.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
	
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

   if (!CreateToolBars())
      return -1;
	


	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		AfxMessageBox("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// CG: The following line was added by the Splash Screen component.
	CSplashWnd::ShowSplashScreen(this);
	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	m_tabWnd.Create(WS_VISIBLE|WS_CHILD, this, 0xE900);


	m_tabWnd.CreateView("Modules", pContext->m_pNewViewClass, pContext);
	m_tabWnd.CreateView("Network", RUNTIME_CLASS(CNetworkView), pContext);
   m_tabWnd.CreateView("ROT", RUNTIME_CLASS(CComView), pContext);
   m_tabWnd.CreateView("Windows", RUNTIME_CLASS(CWindowView), pContext);
   
	
	//return CFrameWnd::OnCreateClient(lpcs, pContext);
   return TRUE;
}



BOOL CMainFrame::CreateToolBars()
{
   CImageList img;
	CString str;



	if (!m_wndToolBar.CreateEx(this))
	{
		AfxMessageBox("Failed to create toolbar\n");
		return FALSE;      // fail to create
	}
	// set up toolbar properties
	m_wndToolBar.GetToolBarCtrl().SetButtonWidth(50, 150);
	m_wndToolBar.GetToolBarCtrl().SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);

	img.Create(IDB_BAR_HOT, 22, 0, RGB(255, 0, 255));
	m_wndToolBar.GetToolBarCtrl().SetHotImageList(&img);
	img.Detach();
	img.Create(IDB_BAR_COLD, 22, 0, RGB(255, 0, 255));
	m_wndToolBar.GetToolBarCtrl().SetImageList(&img);
	img.Detach();
	
	m_wndToolBar.SetButtons(NULL, 4);

	// set up each toolbar button
/*	m_wndToolBar.SetButtonInfo(0, ID_GO_BACK, TBSTYLE_BUTTON, 0);
	str.LoadString(IDS_BACK);
	m_wndToolBar.SetButtonText(0, str);
	m_wndToolBar.SetButtonInfo(1, ID_GO_FORWARD, TBSTYLE_BUTTON, 1);
	str.LoadString(IDS_FORWARD);
	m_wndToolBar.SetButtonText(1, str);
	m_wndToolBar.SetButtonInfo(2, ID_VIEW_STOP, TBSTYLE_BUTTON, 2);
	str.LoadString(IDS_STOP);
	m_wndToolBar.SetButtonText(2, str);*/
	m_wndToolBar.SetButtonInfo(0, ID_VIEW_REFRESH, TBSTYLE_BUTTON, 3);
	str.LoadString(IDS_REFRESH);
	m_wndToolBar.SetButtonText(0, str);
	/*m_wndToolBar.SetButtonInfo(4, ID_GO_START_PAGE, TBSTYLE_BUTTON, 4);
	str.LoadString(IDS_HOME);
	m_wndToolBar.SetButtonText(4, str);
	m_wndToolBar.SetButtonInfo(5, ID_GO_SEARCH_THE_WEB, TBSTYLE_BUTTON, 5);
	str.LoadString(IDS_SEARCH);
	m_wndToolBar.SetButtonText(5, str);
	m_wndToolBar.SetButtonInfo(6, ID_FAVORITES_DROPDOWN, TBSTYLE_BUTTON | TBSTYLE_DROPDOWN, 6);
	str.LoadString(IDS_FAVORITES);
	m_wndToolBar.SetButtonText(6, str);*/
	m_wndToolBar.SetButtonInfo(2, ID_FILE_PRINT, TBSTYLE_BUTTON, 7);
	str.LoadString(IDS_PRINT);
	m_wndToolBar.SetButtonText(2, str);
	/*m_wndToolBar.SetButtonInfo(8, ID_FONT_DROPDOWN, TBSTYLE_BUTTON | TBSTYLE_DROPDOWN, 8);
	str.LoadString(IDS_FONT);
	m_wndToolBar.SetButtonText(8, str);*/

   

  	m_wndToolBar.SetButtonInfo(1, ID_EDIT_KILL, TBSTYLE_BUTTON, 17);
	str.LoadString(IDS_KILL);
	m_wndToolBar.SetButtonText(1, str);

 	m_wndToolBar.SetButtonInfo(3, ID_EDIT_FIND, TBSTYLE_BUTTON, 5);
	str.LoadString(IDS_FIND);
	m_wndToolBar.SetButtonText(3, str);



	CRect rectToolBar;

	// set up toolbar button sizes
	m_wndToolBar.GetItemRect(0, &rectToolBar);
	m_wndToolBar.SetSizes(rectToolBar.Size(), CSize(30,20));

   if (CPerformance::IsRunningWindowsNT())
   {
	   if (!m_wndReBar.Create(this) ||
		   !m_wndReBar.AddBar(&m_wndToolBar))
	   {
         // If no rebar just dock the controls
         // without it.
		   //AfxMessageBox("Failed to create rebar\n");

         DockControlBar(&m_wndToolBar);
         DockControlBar(&m_wndDlgBar);
      }
      else
      {
         m_wndToolBar.ModifyStyle(0, TBSTYLE_FLAT | TBSTYLE_TRANSPARENT);
      }
   }
   else
   {
      if (!m_wndDlgBar.Create(this, IDR_MAINFRAME, 
		   CBRS_ALIGN_TOP, AFX_IDW_DIALOGBAR))
	   {
		   AfxMessageBox("Failed to create dialogbar\n");
		   return FALSE;		// fail to create
	   }

	   if (!m_wndReBar.Create(this) ||
		   !m_wndReBar.AddBar(&m_wndToolBar) ||
		   !m_wndReBar.AddBar(&m_wndDlgBar, "System Load"))
	   {
         // If no rebar just dock the controls
         // without it.
		   //AfxMessageBox("Failed to create rebar\n");

         DockControlBar(&m_wndToolBar);
         DockControlBar(&m_wndDlgBar);
      }
      else
      {
         m_wndToolBar.ModifyStyle(0, TBSTYLE_FLAT | TBSTYLE_TRANSPARENT);
      }
   }

   // TODO: Remove this if you don't want tool tips
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY);

   return TRUE;
}
