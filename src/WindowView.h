#if !defined(AFX_WINDOWVIEW_H__569F36D9_9B62_423F_8D70_916FAD10F1DD__INCLUDED_)
#define AFX_WINDOWVIEW_H__569F36D9_9B62_423F_8D70_916FAD10F1DD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// WindowView.h : header file
//

class CSysMateDoc;

#include "ProcessView.h"

/////////////////////////////////////////////////////////////////////////////
// CWindowView view

class CWindowView : public CProcessView
{
protected:
	CWindowView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CWindowView)

// Attributes
public:

   inline CSysMateDoc* GetDocument()
   { return (CSysMateDoc*)m_pDocument; }


   virtual void PopulateChild(HTREEITEM hTreeItem, int nProcId);
   static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

// Operations
public:

   CMap<HWND, HWND, HTREEITEM, HTREEITEM> m_mapWindows;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWindowView)
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CWindowView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

   DWORD m_nEnumProcId;
   HTREEITEM m_hEnumTreeRoot;

	// Generated message map functions
protected:
	//{{AFX_MSG(CWindowView)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINDOWVIEW_H__569F36D9_9B62_423F_8D70_916FAD10F1DD__INCLUDED_)
