// ProcessFindReplaceDialog.cpp : implementation file
//

#include "stdafx.h"
#include "SysMate.h"
#include "ProcessFindReplaceDialog.h"
#include "SysMateDoc.h"
#include "ProcessView.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CProcessFindReplaceDialog

CProcessFindReplaceDialog::CProcessFindReplaceDialog(CProcessView *pView)
{
   m_pView = pView;
}

CProcessFindReplaceDialog::~CProcessFindReplaceDialog()
{
}


BEGIN_MESSAGE_MAP(CProcessFindReplaceDialog, CFindReplaceDialog)
	//{{AFX_MSG_MAP(CProcessFindReplaceDialog)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CProcessFindReplaceDialog message handlers

void CProcessFindReplaceDialog::PostNcDestroy() 
{
	m_pView->CloseFindDialog();
	
	CFindReplaceDialog::PostNcDestroy();
}
