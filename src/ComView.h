#if !defined(AFX_COMVIEW_H__DE0AB9F0_9E79_4911_9596_C3E63D8C49FC__INCLUDED_)
#define AFX_COMVIEW_H__DE0AB9F0_9E79_4911_9596_C3E63D8C49FC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ComView.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CComView view

class CComView : public CListView
{
protected:
	CComView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CComView)

// Attributes
public:

// Operations
public:
	void PopulateRunningObjects();
	void Error(LPCSTR sz, HRESULT hr = S_OK);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CComView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CComView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CComView)
	afx_msg void OnViewRefresh();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COMVIEW_H__DE0AB9F0_9E79_4911_9596_C3E63D8C49FC__INCLUDED_)
