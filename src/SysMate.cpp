// SysMate.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "SysMate.h"

#include "MainFrm.h"
#include "SysMateDoc.h"
#include "ProcessView.h"
#include "Splash.h"
#include <dos.h>
#include <direct.h>

#include "ConvertString.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSysMateApp

BEGIN_MESSAGE_MAP(CSysMateApp, CWinApp)
	//{{AFX_MSG_MAP(CSysMateApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSysMateApp construction

CSysMateApp::CSysMateApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CSysMateApp object

CSysMateApp theApp;

// This identifier was generated to be statistically unique for your app.
// You may change it if you prefer to choose a specific identifier.

// {0B5E6A75-CA5C-47A8-9E35-323BD362F342}
static const CLSID clsid =
{ 0xb5e6a75, 0xca5c, 0x47a8, { 0x9e, 0x35, 0x32, 0x3b, 0xd3, 0x62, 0xf3, 0x42 } };

/////////////////////////////////////////////////////////////////////////////
// CSysMateApp initialization

BOOL CSysMateApp::InitInstance()
{
	// CG: The following block was added by the Splash Screen component.
\
	{
\
		CCommandLineInfo cmdInfo;
\
		ParseCommandLine(cmdInfo);
\

\
		CSplashWnd::EnableSplashScreen(cmdInfo.m_bShowSplash);
\
	}
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

   

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("Walker\\SysMate"));

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

   m_SystemImageList.Attach(GetSystemImageList(true));

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CSysMateDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CProcessView));
	AddDocTemplate(pDocTemplate);

	// Connect the COleTemplateServer to the document template.
	//  The COleTemplateServer creates new documents on behalf
	//  of requesting OLE containers by using information
	//  specified in the document template.
	m_server.ConnectTemplate(clsid, pDocTemplate, TRUE);
		// Note: SDI applications register server objects only if /Embedding
		//   or /Automation is present on the command line.

	// Enable DDE Execute open
	EnableShellOpen();
	RegisterShellFileTypes(TRUE);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Check to see if launched as OLE server
	if (cmdInfo.m_bRunEmbedded || cmdInfo.m_bRunAutomated)
	{
		// Register all OLE server (factories) as running.  This enables the
		//  OLE libraries to create objects from other applications.
		COleTemplateServer::RegisterAll();

		// Application was run with /Embedding or /Automation.  Don't show the
		//  main window in this case.
		return TRUE;
	}

	// When a server application is launched stand-alone, it is a good idea
	//  to update the system registry in case it has been damaged.
	m_server.UpdateRegistry(OAT_DISPATCH_OBJECT);
	COleObjectFactory::UpdateRegistryAll();

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;


	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();

	// Enable drag/drop open
	//m_pMainWnd->DragAcceptFiles();

	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual BOOL OnInitDialog();
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CSysMateApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CSysMateApp message handlers


BOOL CSysMateApp::PreTranslateMessage(MSG* pMsg)
{
	// CG: The following lines were added by the Splash Screen component.
	if (CSplashWnd::PreTranslateAppMessage(pMsg))
		return TRUE;

	return CWinApp::PreTranslateMessage(pMsg);
}

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();	// CG:  This was added by System Info Component.

	// CG: Following block was added by System Info Component.
	{
		CString strFreeDiskSpace;
		CString strFreeMemory;
		CString strFmt;

		// Fill available memory
		MEMORYSTATUS MemStat;
		MemStat.dwLength = sizeof(MEMORYSTATUS);
		GlobalMemoryStatus(&MemStat);
		strFmt.LoadString(CG_IDS_PHYSICAL_MEM);
		strFreeMemory.Format(strFmt, MemStat.dwTotalPhys / 1024L);

		//TODO: Add a static control to your About Box to receive the memory
		//      information.  Initialize the control with code like this:
		// SetDlgItemText(IDC_PHYSICAL_MEM, strFreeMemory);

		// Fill disk free information
		struct _diskfree_t diskfree;
		int nDrive = _getdrive(); // use current default drive
		if (_getdiskfree(nDrive, &diskfree) == 0)
		{
			strFmt.LoadString(CG_IDS_DISK_SPACE);
			strFreeDiskSpace.Format(strFmt,
				(DWORD)diskfree.avail_clusters *
				(DWORD)diskfree.sectors_per_cluster *
				(DWORD)diskfree.bytes_per_sector / (DWORD)1024L,
				nDrive-1 + _T('A'));
		}
		else
			strFreeDiskSpace.LoadString(CG_IDS_DISK_SPACE_UNAVAIL);

		//TODO: Add a static control to your About Box to receive the memory
		//      information.  Initialize the control with code like this:
		// SetDlgItemText(IDC_DISK_SPACE, strFreeDiskSpace);
	}

	return TRUE;	// CG:  This was added by System Info Component.

}

HIMAGELIST CSysMateApp::GetSystemImageList(BOOL fSmall)
{
   HIMAGELIST  himl;
   SHFILEINFO  sfi;
   
   himl = (HIMAGELIST)SHGetFileInfo(TEXT("C:\\"), 0, &sfi,
      sizeof(SHFILEINFO), SHGFI_SYSICONINDEX |
      (fSmall ? SHGFI_SMALLICON : SHGFI_LARGEICON));
   
      /*
      Do a version check first because you only need to use this code on
      Windows NT version 4.0.
   */ 
   OSVERSIONINFO vi;
   vi.dwOSVersionInfoSize = sizeof(vi);
   GetVersionEx(&vi);
   if(VER_PLATFORM_WIN32_WINDOWS == vi.dwPlatformId)
      return himl;
   
      /*
      You need to create a temporary, empty .lnk file that you can use to
      pass to IShellIconOverlay::GetOverlayIndex. You could just enumerate
      down from the Start Menu folder to find an existing .lnk file, but
      there is a very remote chance that you will not find one. By creating
      your own, you know this code will always work.
   */ 
   HRESULT           hr;
   IShellFolder      *psfDesktop = NULL;
   IShellFolder      *psfTempDir = NULL;
   IMalloc           *pMalloc = NULL;
   LPITEMIDLIST      pidlTempDir = NULL;
   LPITEMIDLIST      pidlTempFile = NULL;
   TCHAR             szTempDir[MAX_PATH];
   TCHAR             szTempFile[MAX_PATH] = TEXT("");
   TCHAR             szFile[MAX_PATH];
   HANDLE            hFile;
   int               i;
   DWORD             dwAttributes;
   DWORD             dwEaten;
   IShellIconOverlay *psio = NULL;
   int               nIndex;
   
   // Get the desktop folder.
   hr = SHGetDesktopFolder(&psfDesktop);
   if(FAILED(hr))
      goto exit;
   
   // Get the shell's allocator.
   hr = SHGetMalloc(&pMalloc);
   if(FAILED(hr))
      goto exit;
   
   // Get the TEMP directory.
   if(!GetTempPath(MAX_PATH, szTempDir))
   {
   /*
   There might not be a TEMP directory. If this is the case, use the
   Windows directory.
      */ 
      if(!GetWindowsDirectory(szTempDir, MAX_PATH))
      {
         hr = E_FAIL;
         goto exit;
      }
   }
   
   // Create a temporary .lnk file.
   if(szTempDir[lstrlen(szTempDir) - 1] != '\\')
      lstrcat(szTempDir, TEXT("\\"));
   for(i = 0, hFile = INVALID_HANDLE_VALUE;
   INVALID_HANDLE_VALUE == hFile;
   i++)
   {
      lstrcpy(szTempFile, szTempDir);
      wsprintf(szFile, TEXT("temp%d.lnk"), i);
      lstrcat(szTempFile, szFile);
      
      hFile = CreateFile(  szTempFile,
         GENERIC_WRITE,
         0,
         NULL,
         CREATE_NEW,
         FILE_ATTRIBUTE_NORMAL,
         NULL);
      
      // Do not try this more than 100 times.
      if(i > 100)
      {
         hr = E_FAIL;
         goto exit;
      }
   }
   
   // Close the file you just created.
   CloseHandle(hFile);
   hFile = INVALID_HANDLE_VALUE;
   
   // Get the PIDL for the directory.
   hr = psfDesktop->ParseDisplayName(  NULL,
      NULL,
      CStringConvert(szTempDir),
      &dwEaten,
      &pidlTempDir,
      &dwAttributes);
   if(FAILED(hr))
      goto exit;
   
   // Get the IShellFolder for the TEMP directory.
   hr = psfDesktop->BindToObject(   pidlTempDir,
      NULL,
      IID_IShellFolder,
      (LPVOID*)&psfTempDir);
   if(FAILED(hr))
      goto exit;
   
      /*
      Get the IShellIconOverlay interface for this folder. If this fails,
      it could indicate that you are running on a pre-Internet Explorer 4.0
      shell, which doesn't support this interface. If this is the case, the
      overlay icons are already in the system image list.
   */ 
   hr = psfTempDir->QueryInterface(IID_IShellIconOverlay, (LPVOID*)&psio);
   if(FAILED(hr))
      goto exit;
   
   // Get the PIDL for the temporary .lnk file.
   hr = psfTempDir->ParseDisplayName(  NULL,
      NULL,
      CStringConvert(szFile),
      &dwEaten,
      &pidlTempFile,
      &dwAttributes);
   if(FAILED(hr))
      goto exit;
   
      /*
      Get the overlay icon for the .lnk file. This causes the shell
      to put all of the standard overlay icons into your copy of the system
      image list.
   */ 
   hr = psio->GetOverlayIndex(pidlTempFile, &nIndex);
   
exit:
   // Delete the temporary file.
   DeleteFile(szTempFile);
   
   if(psio)
      psio->Release();
   
   if(INVALID_HANDLE_VALUE != hFile)
      CloseHandle(hFile);
   
   if(psfTempDir)
      psfTempDir->Release();
   
   if(pMalloc)
   {
      if(pidlTempFile)
         pMalloc->Free(pidlTempFile);
      
      if(pidlTempDir)
         pMalloc->Free(pidlTempDir);
      
      pMalloc->Release();
   }
   
   if(psfDesktop)
      psfDesktop->Release();
   
   return himl;
}

int CSysMateApp::ExitInstance() 
{
	m_SystemImageList.Detach();
	
	return CWinApp::ExitInstance();
}
