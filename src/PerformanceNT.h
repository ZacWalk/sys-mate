// PerformanceNT.h: interface for the CPerformanceNT class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PERFORMANCENT_H__32A9AB5E_289C_4798_A3DB_20A51971EEDD__INCLUDED_)
#define AFX_PERFORMANCENT_H__32A9AB5E_289C_4798_A3DB_20A51971EEDD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Performance.h"

class CPerformanceNT : public CPerformance  
{
public:
	CPerformanceNT();
	virtual ~CPerformanceNT();

  	virtual int GetData() const { return 0; };
   virtual void Close() {};
   virtual BOOL Open(LPCSTR szGroup, LPCSTR szItem) { return TRUE; };


};

#endif // !defined(AFX_PERFORMANCENT_H__32A9AB5E_289C_4798_A3DB_20A51971EEDD__INCLUDED_)
