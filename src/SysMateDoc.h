// SysMateDoc.h : interface of the CSysMateDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_SYSMATEDOC_H__8A9E9223_4601_4718_B4D2_A1E2C73A9E4E__INCLUDED_)
#define AFX_SYSMATEDOC_H__8A9E9223_4601_4718_B4D2_A1E2C73A9E4E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SimpleProcessAPI.h"


class CSysMateDoc : public CDocument
{
protected: // create from serialization only
	CSysMateDoc();
	DECLARE_DYNCREATE(CSysMateDoc)

// Attributes
public:

   CSimpleProcessAPI m_ProcessAPI;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSysMateDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	CString GetModuleDesc(const CString &strFileName, BOOL bWantCRs = TRUE);
	virtual ~CSysMateDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CSysMateDoc)
	afx_msg void OnFileDumptotextfile();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(CSysMateDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SYSMATEDOC_H__8A9E9223_4601_4718_B4D2_A1E2C73A9E4E__INCLUDED_)
