// ProcessView.h : interface of the CProcessView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROCESSVIEW_H__F08E72AF_1CB4_48CD_A91B_85225694DA35__INCLUDED_)
#define AFX_PROCESSVIEW_H__F08E72AF_1CB4_48CD_A91B_85225694DA35__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PrintTreeView.h"
class CProcessFindReplaceDialog;


class CProcessView : public CPrintTreeView
{
protected: // create from serialization only
	CProcessView();
	DECLARE_DYNCREATE(CProcessView)

// Attributes
public:
	CSysMateDoc* GetDocument();

  	CToolTipCtrl m_ToolTip;
	HTREEITEM m_hToolTipItem;
   CString m_strToolTip;

   CProcessFindReplaceDialog* m_pFindDialog;

   

public:
	virtual HTREEITEM FindItem(CString &sSearch, 
				BOOL bCaseSensitive = FALSE, 
				BOOL bDownDir = TRUE, 
				BOOL bWholeWord = FALSE, 
				HTREEITEM hItem = NULL);
protected:
	virtual BOOL IsFindValid( HTREEITEM );
   HTREEITEM GetNextItem( HTREEITEM hItem);
   HTREEITEM GetPrevItem( HTREEITEM hItem);
   HTREEITEM GetLastItem( HTREEITEM hItem );



// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProcessView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void OnInitialUpdate(); // called first time after construct
	//}}AFX_VIRTUAL

// Implementation
public:
	void CloseFindDialog();
	HGLOBAL GetSelectedFileText();
	HGLOBAL GetSelectedFile();
	CString GetModuleDesc(const CString &FileName);

   virtual void PopulateChild(HTREEITEM hTreeItem, int nProcId);

	void PopulateProcessList();
   BOOL OnToolTipNotify(UINT id, NMHDR *pNMHDR, LRESULT *pResult);
	virtual ~CProcessView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CProcessView)
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnViewRefresh();
	afx_msg void OnEditKill();
	afx_msg void OnUpdateEditKill(CCmdUI* pCmdUI);
	afx_msg void OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEditCopy();
	afx_msg void OnEditFind();
	afx_msg void OnUpdateEditFind(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
   afx_msg LONG OnFindReplace(WPARAM wParam, LPARAM lParam);

};

#ifndef _DEBUG  // debug version in ProcessView.cpp
inline CSysMateDoc* CProcessView::GetDocument()
   { return (CSysMateDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSVIEW_H__F08E72AF_1CB4_48CD_A91B_85225694DA35__INCLUDED_)
