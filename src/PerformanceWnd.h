#if !defined(AFX_PERFORMANCEWND_H__8C25A883_9731_4132_AD06_E7DDDABC1975__INCLUDED_)
#define AFX_PERFORMANCEWND_H__8C25A883_9731_4132_AD06_E7DDDABC1975__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PerformanceWnd.h : header file
//

#define SAMPLE_COUNT 50
#define AVERAGE_COUNT 5

class CPerformance;

/////////////////////////////////////////////////////////////////////////////
// CPerformanceWnd dialog

class CPerformanceWnd : public CDialogBar
{
// Construction
public:
	CPerformanceWnd();   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPerformanceWnd)
	enum { IDD = IDR_MAINFRAME };
	//}}AFX_DATA

   int  m_nUsages[SAMPLE_COUNT];
   CPerformance *m_pPerf;

   int   m_nUsageSum;
   int   m_nCount;


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPerformanceWnd)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPerformanceWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PERFORMANCEWND_H__8C25A883_9731_4132_AD06_E7DDDABC1975__INCLUDED_)
