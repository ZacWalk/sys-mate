#if !defined(AFX_NETWORKVIEW_H__C88AC7F8_6CBB_48D6_9F26_57F508251630__INCLUDED_)
#define AFX_NETWORKVIEW_H__C88AC7F8_6CBB_48D6_9F26_57F508251630__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// NetworkView.h : header file
//

typedef BOOL (CALLBACK* SEQUERY)(BYTE,RFC1157VarBindList*,AsnInteger*,AsnInteger*);
typedef BOOL (CALLBACK* SEINIT)(DWORD,HANDLE*,AsnObjectIdentifier*);


/////////////////////////////////////////////////////////////////////////////
// CNetworkView view

class CNetworkView : public CListView
{
protected:
	CNetworkView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CNetworkView)

// Attributes
public:




// Operations
public:
	LPCSTR GetPortName( UINT port, char *proto, char *name, int namelen );
	LPCSTR GetIpHostName( BOOL local, UINT ipaddr, char *name, int namelen );
	BOOL LoadInetMibEntryPoints();
	int Populate();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetworkView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CNetworkView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CNetworkView)
	afx_msg void OnViewRefresh();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETWORKVIEW_H__C88AC7F8_6CBB_48D6_9F26_57F508251630__INCLUDED_)
