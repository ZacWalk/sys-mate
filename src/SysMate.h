// SysMate.h : main header file for the SYSMATE application
//

#if !defined(AFX_SYSMATE_H__31D5CD0B_CE41_4035_B81F_269816E6D46C__INCLUDED_)
#define AFX_SYSMATE_H__31D5CD0B_CE41_4035_B81F_269816E6D46C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSysMateApp:
// See SysMate.cpp for the implementation of this class
//

class CSysMateApp : public CWinApp
{
public:
	HIMAGELIST GetSystemImageList(BOOL fSmall);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	CSysMateApp();

   CImageList m_SystemImageList;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSysMateApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
	COleTemplateServer m_server;
		// Server object for document creation
	//{{AFX_MSG(CSysMateApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

extern CSysMateApp theApp;


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SYSMATE_H__31D5CD0B_CE41_4035_B81F_269816E6D46C__INCLUDED_)
