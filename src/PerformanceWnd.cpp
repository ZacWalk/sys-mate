// PerformanceWnd.cpp : implementation file
//

#include "stdafx.h"
#include "SysMate.h"
#include "PerformanceWnd.h"
#include "Performance.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPerformanceWnd dialog


CPerformanceWnd::CPerformanceWnd()
	: CDialogBar()
{
	//{{AFX_DATA_INIT(CPerformanceWnd)
	//}}AFX_DATA_INIT

   ZeroMemory(m_nUsages, sizeof(DWORD) * SAMPLE_COUNT);
   m_nUsageSum = 0;
   m_nCount = 0;

   m_pPerf = NULL;
}


void CPerformanceWnd::DoDataExchange(CDataExchange* pDX)
{
	CDialogBar::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPerformanceWnd)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPerformanceWnd, CDialogBar)
	//{{AFX_MSG_MAP(CPerformanceWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPerformanceWnd message handlers

int CPerformanceWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialogBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	


	SetTimer(55, 100, NULL);
   m_pPerf = CPerformance::Create();

   m_pPerf->Open("KERNEL", "CPUUsage");

 
    return 0;
}


void CPerformanceWnd::OnDestroy() 
{
	CDialogBar::OnDestroy();

   m_pPerf->Close();
   delete m_pPerf;
   m_pPerf = NULL;
	
}


void CPerformanceWnd::OnPaint() 
{
   CRect r;
   GetClientRect(r);

	CPaintDC dc(this); // device context for painting

   POINT pt[SAMPLE_COUNT + 2];

   ZeroMemory(pt, SAMPLE_COUNT + 2 * sizeof(POINT));

   pt[0].x = r.left;
   pt[0].y = r.bottom;

	pt[SAMPLE_COUNT + 1].x = r.right;
   pt[SAMPLE_COUNT + 1].y = r.bottom;

   for(int i = 0; i < SAMPLE_COUNT; i++)
   {
      pt[i+1].x = (r.Width() * i) / (SAMPLE_COUNT-1);
      pt[i+1].y = (r.Height() * (100 - m_nUsages[i])) / 100;
   }

   dc.Polygon(pt, SAMPLE_COUNT + 2);
}

void CPerformanceWnd::OnTimer(UINT nIDEvent) 
{

   m_nUsageSum += m_pPerf->GetData();

   if (m_nCount++ > AVERAGE_COUNT)
   {

      for(int i = 0; i < (SAMPLE_COUNT-1); i++)
      {
         m_nUsages[i] = m_nUsages[i + 1];
      }

      m_nUsages[SAMPLE_COUNT-1] = m_nUsageSum / m_nCount;

      m_nUsageSum = 0;
      m_nCount = 0;

      Invalidate();
   }

   CDialogBar::OnTimer(nIDEvent);
}


