// Performance98.cpp: implementation of the CPerformance98 class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SysMate.h"
#include "Performance98.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPerformance98::CPerformance98()
{

}

CPerformance98::~CPerformance98()
{

}

BOOL CPerformance98::DisableDataCollection()
   {
    HKEY hOpen;
    LPBYTE pByte;
    DWORD cbData;
    DWORD dwType;
    DWORD rc;
    BOOL bSuccess = TRUE;

    if ( (rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StopStat", 0,
                           KEY_READ, &hOpen)) == ERROR_SUCCESS)
    {

      // query to get data size
      if ( (rc = RegQueryValueEx(hOpen,m_strKey,NULL,&dwType,
            NULL, &cbData )) == ERROR_SUCCESS )
      {
         pByte = (LPBYTE)LocalAlloc(LPTR, cbData);

         // query the performance start key to initialize performance data
         // query the start key to initialize performance data
         rc = RegQueryValueEx(hOpen,m_strKey,NULL,&dwType, pByte,
                              &cbData );
         // at this point we don't do anything with the data
         // free up resources
         LocalFree(pByte);

      }
      else
         bSuccess = FALSE;

      RegCloseKey(hOpen);
    }
    else
      bSuccess = FALSE;

    return bSuccess;
   }



BOOL CPerformance98::EnableDataCollection()
   {
    HKEY hOpen;
    DWORD cbData;
    DWORD dwType;
    LPBYTE pByte;
    DWORD rc;
    BOOL bSuccess = TRUE;

    if ( (rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StartStat", 0,
                           KEY_READ, &hOpen)) == ERROR_SUCCESS)
    {
      // query to get data size
      if ( (rc = RegQueryValueEx(hOpen,m_strKey,NULL,&dwType,
             NULL, &cbData )) == ERROR_SUCCESS )
      {
         pByte = (LPBYTE)LocalAlloc(LPTR, cbData);

         // query the performance start key to initialize performance data
         // query the start key to initialize performance data
         rc = RegQueryValueEx(hOpen,m_strKey,NULL,&dwType, pByte,
                              &cbData );
         // at this point we don't do anything with the data
         //  free up resources
         LocalFree(pByte);
      }
      else
         bSuccess = FALSE;

      RegCloseKey(hOpen);
    }
    else
      bSuccess = FALSE;

    return bSuccess;
   }





BOOL CPerformance98::Open(LPCSTR szGroup, LPCSTR szItem)
{
   m_strKey.Format("%s\\%s", szGroup, szItem);


    if (!EnableDataCollection())
       return FALSE;

    DWORD rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StatData", 0,
                           KEY_READ, &m_hOpen);

    if (rc == ERROR_SUCCESS)
    {
       m_bOpen = TRUE;
    }
    else
    {
       DisableDataCollection();
    }

    return m_bOpen;
}

void CPerformance98::Close()
{
   if (m_bOpen)
   {
      RegCloseKey(m_hOpen);
      DisableDataCollection();
      m_bOpen = FALSE;
   }
}

int CPerformance98::GetData() const
{
   DWORD cbData = sizeof(DWORD);
   DWORD dwType;
   DWORD dwVal = 0;

   if (m_bOpen)
      // retrieve performance data which is of size of DWORD
      RegQueryValueEx(m_hOpen,"KERNEL\\CPUUsage",0, &dwType,
               (LPBYTE)&dwVal, &cbData );  

   return dwVal;
}
