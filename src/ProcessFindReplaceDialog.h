#if !defined(AFX_PROCESSFINDREPLACEDIALOG_H__3002FE37_CE7B_411C_A94C_297F318DC08F__INCLUDED_)
#define AFX_PROCESSFINDREPLACEDIALOG_H__3002FE37_CE7B_411C_A94C_297F318DC08F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProcessFindReplaceDialog.h : header file
//

class CProcessView;

/////////////////////////////////////////////////////////////////////////////
// CProcessFindReplaceDialog window

class CProcessFindReplaceDialog : public CFindReplaceDialog
{
// Construction
public:
	CProcessFindReplaceDialog(CProcessView *pView);

// Attributes
public:

// Operations
public:
   CProcessView *m_pView;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProcessFindReplaceDialog)
	protected:
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CProcessFindReplaceDialog();

	// Generated message map functions
protected:
	//{{AFX_MSG(CProcessFindReplaceDialog)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSFINDREPLACEDIALOG_H__3002FE37_CE7B_411C_A94C_297F318DC08F__INCLUDED_)
