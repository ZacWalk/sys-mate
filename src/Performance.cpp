// Performance.cpp: implementation of the CPerformance class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SysMate.h"
#include "Performance.h"
#include "Performance98.h"
#include "PerformanceNT.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPerformance::CPerformance()
{
   m_bOpen = FALSE;
}

CPerformance::~CPerformance()
{
   if (m_bOpen)
   {
      //Close();
   }
}

BOOL CPerformance::IsRunningWindowsNT()
{
	OSVERSIONINFO versionInfo;

	// set the size of OSVERSIONINFO, before calling the function

	versionInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

	// Get the version information

	if (::GetVersionEx (&versionInfo)) {

	    if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
            return(TRUE);
	    }

    }

    return(FALSE);
}

CPerformance * CPerformance::Create()
{
   if (IsRunningWindowsNT())
   {
      return new CPerformanceNT;
   }
   else
   {
      return new CPerformance98;
   }
}
