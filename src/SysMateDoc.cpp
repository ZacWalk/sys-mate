// SysMateDoc.cpp : implementation of the CSysMateDoc class
//

#include "stdafx.h"
#include "SysMate.h"

#include "SysMateDoc.h"
#include "FileVersion.h"
#include "ProgDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSysMateDoc

IMPLEMENT_DYNCREATE(CSysMateDoc, CDocument)

BEGIN_MESSAGE_MAP(CSysMateDoc, CDocument)
	//{{AFX_MSG_MAP(CSysMateDoc)
	ON_COMMAND(ID_FILE_DUMPTOTEXTFILE, OnFileDumptotextfile)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_FILE_SEND_MAIL, OnFileSendMail)
	ON_UPDATE_COMMAND_UI(ID_FILE_SEND_MAIL, OnUpdateFileSendMail)
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CSysMateDoc, CDocument)
	//{{AFX_DISPATCH_MAP(CSysMateDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//      DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_ISysMate to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {CC9D2ADC-3728-4CDA-9369-4F4581CF751C}
static const IID IID_ISysMate =
{ 0xcc9d2adc, 0x3728, 0x4cda, { 0x93, 0x69, 0x4f, 0x45, 0x81, 0xcf, 0x75, 0x1c } };

BEGIN_INTERFACE_MAP(CSysMateDoc, CDocument)
	INTERFACE_PART(CSysMateDoc, IID_ISysMate, Dispatch)
END_INTERFACE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSysMateDoc construction/destruction

CSysMateDoc::CSysMateDoc()
{
	// TODO: add one-time construction code here

	EnableAutomation();

	AfxOleLockApp();
}

CSysMateDoc::~CSysMateDoc()
{
	AfxOleUnlockApp();
}

BOOL CSysMateDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

   TCHAR sz[255];
   ULONG n = 255;
   GetComputerName(sz, &n);

   SetTitle(sz);

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CSysMateDoc serialization

void CSysMateDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
      CProgressDlg dlg;

      

		// Dump the process information
  	   CMapStringToString PIDNameMap;
	   CString PIDString;
	   DWORD nPID=0;
	   DWORD nCurrentPID=::GetCurrentProcessId();
	   POSITION pos = NULL;

	   if (m_ProcessAPI.BuildProcessList(PIDNameMap)) 
      {
         dlg.SetRange(0, PIDNameMap.GetCount());
         dlg.SetStep(1);
         dlg.Create();
         dlg.SetWindowText("Working...");

		   pos = PIDNameMap.GetStartPosition();

		   while( pos != NULL && !dlg.CheckCancelButton())
         {
			   CString strProcessName;
			   CString strPIDString;
            CString strProcessFullName;

			   // Get key ( PIDString ) and value ( ProcessName )
			   PIDNameMap.GetNextAssoc( pos, strPIDString, strProcessName );
            nPID = atol(strPIDString);
            strProcessFullName = m_ProcessAPI.GetProcessExecutableName(nPID);

            ar.WriteString("\r\n");
            ar.WriteString(strProcessFullName);
            ar.WriteString(GetModuleDesc(strProcessFullName));
            ar.WriteString("\r\n");

            CStringList ModuleList;

	         if (m_ProcessAPI.BuildModuleList(nPID, ModuleList)) 
            {

		         POSITION pos = ModuleList.GetHeadPosition();

		         while( pos != NULL ){

			         // Get file name
			         CString str = ModuleList.GetNext(pos);

                  ar.WriteString("\t\t");
                  ar.WriteString(str);
                  ar.WriteString(GetModuleDesc(str));
                  ar.WriteString("\r\n");

		         }

	         }

            ar.WriteString("\r\n");

            dlg.StepIt();
		   }
		   
	   }
	}
	else
	{
		// TODO: add loading code here
      ASSERT(FALSE);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSysMateDoc diagnostics

#ifdef _DEBUG
void CSysMateDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CSysMateDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSysMateDoc commands

void CSysMateDoc::OnFileDumptotextfile() 
{
   CWaitCursor wait;

	CString strFileName;
   strFileName.Format("%s\\output.txt", getenv("TEMP"));

   CStdioFile f;

   if (f.Open(strFileName, CFile::modeCreate | CFile::modeWrite))
   {

      CArchive ar(&f, CArchive::store);
      Serialize(ar);
      ar.Flush();

      f.Close();
   }


   // Open notepad to display the info
   WinExec(CString("notepad.exe ") + strFileName, SW_SHOW);
	
}

CString CSysMateDoc::GetModuleDesc(const CString &strFileName, BOOL bWantCRs)
{
   if (strFileName.IsEmpty())
      return "";

   CString str, strOut;
   CFileVersion fileVersion;
   CString FileDescription;
   CString FileVersion;
   CString ProductName;
   CString ProductVersion;
   CString FileModificationDateString;
   CString FileSizeString;


	if (fileVersion.Open(strFileName)) 
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
		/*// Get file size and date
		CFileStatus FileStatus;
		CTime FileModificationDate;
		LONG FileSize=0;
		if (CFile::GetStatus(strFileName, FileStatus)) {
			FileModificationDate = FileStatus.m_mtime;
			FileModificationDateString = FileModificationDate.Format("%B %d, %Y %H:%M:%S" );
			FileSize = FileStatus.m_size;
			FileSizeString.Format("%d",FileSize);
		} else {
			// Error: can't get file status
			FileModificationDateString = "?";
			FileSizeString = "?";
		}*/

		// Log it in debug window
		//str.Format("Module File Name = \"%s\"\t", strFileName);
      //strOut = str;
		str.Format(" (%s)", FileVersion);
      strOut += str;
		/*str.Format(" File Size       = %s bytes\t", FileSizeString);
      strOut += str;
		str.Format(" File Date       = \"%s\"\t", FileModificationDateString);
      strOut += str;
		str.Format(" File Descrition = \"%s\"\t", FileDescription);
      strOut += str;*/
		//str.Format(" Product Name    = \"%s\"\t", ProductName);
      //strOut += str;
		//str.Format(" Product Version = \"%s\"", ProductVersion);
      //strOut += str;

   return strOut;
}
