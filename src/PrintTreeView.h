#if !defined(AFX_PRINTTREEVIEW_H__39048960_715C_4418_BE49_D6FC134D6C6B__INCLUDED_)
#define AFX_PRINTTREEVIEW_H__39048960_715C_4418_BE49_D6FC134D6C6B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PrintTreeView.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPrintTreeView view

class CPrintTreeView : public CTreeView
{
protected:
	CPrintTreeView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CPrintTreeView)

// Attributes
public:

private:
	CRect rcBounds;
	int m_nCharWidth;
	int m_nRowHeight;
	int m_nRowsPerPage;
	HANDLE hDIB;
	WINDOWPLACEMENT WndPlace;

   void PrintHeadFoot(CDC *pDC, CPrintInfo *pInfo);
	HANDLE DDBToDIB( CBitmap& bitmap, DWORD dwCompression, CPalette* pPal );


// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrintTreeView)
	public:
	virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CPrintTreeView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CPrintTreeView)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PRINTTREEVIEW_H__39048960_715C_4418_BE49_D6FC134D6C6B__INCLUDED_)
