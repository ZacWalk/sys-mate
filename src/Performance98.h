// Performance98.h: interface for the CPerformance98 class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PERFORMANCE98_H__AF72726B_EB17_452E_B882_D5DAC9F42CFE__INCLUDED_)
#define AFX_PERFORMANCE98_H__AF72726B_EB17_452E_B882_D5DAC9F42CFE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Performance.h"

class CPerformance98 : public CPerformance  
{
public:
	virtual int GetData() const;
	virtual void Close();
	virtual BOOL Open(LPCSTR szGroup, LPCSTR szItem);
	CPerformance98();
	virtual ~CPerformance98();

   BOOL DisableDataCollection();
   BOOL EnableDataCollection();

private:
   CString m_strKey;
   HKEY m_hOpen;
};

#endif // !defined(AFX_PERFORMANCE98_H__AF72726B_EB17_452E_B882_D5DAC9F42CFE__INCLUDED_)
