// Performance.h: interface for the CPerformance class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PERFORMANCE_H__939A5855_EA39_4FCB_A483_69899992DD5A__INCLUDED_)
#define AFX_PERFORMANCE_H__939A5855_EA39_4FCB_A483_69899992DD5A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CPerformance : public CObject  
{
public:
	static CPerformance * Create();
	static BOOL IsRunningWindowsNT();
	CPerformance();
	virtual ~CPerformance();

   virtual void Close() = 0;
   virtual BOOL Open(LPCSTR szGroup, LPCSTR szItem) = 0;

   virtual int GetData() const = 0;


protected:
	BOOL m_bOpen;
};

#endif // !defined(AFX_PERFORMANCE_H__939A5855_EA39_4FCB_A483_69899992DD5A__INCLUDED_)
