// NetworkView.cpp : implementation file
//

#include "stdafx.h"
#include "SysMate.h"
#include "NetworkView.h"

#include <vector>

#define HOSTNAMELEN 256
#define PORTNAMELEN 256
#define ADDRESSLEN HOSTNAMELEN+PORTNAMELEN

typedef struct _tcpinfo {
	struct _tcpinfo		*prev;
	struct _tcpinfo		*next;
	UINT				state;
	UINT				localip;
	UINT				localport;
	UINT				remoteip;
	UINT				remoteport;
} TCPINFO, *PTCPINFO;


SEINIT pSnmpExtensionInit;
SEQUERY pSnmpExtensionQuery;

//
// Possible TCP endpoint states
//
static char TcpState[][32] = {
	"???",
	"CLOSED",
	"LISTENING",
	"SYN_SENT",
	"SEN_RECEIVED",
	"ESTABLISHED",
	"FIN_WAIT",
	"FIN_WAIT2",
	"CLOSE_WAIT",
	"CLOSING",
	"LAST_ACK",
	"TIME_WAIT"
};
	



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetworkView

IMPLEMENT_DYNCREATE(CNetworkView, CListView)

CNetworkView::CNetworkView()
{
 	
}

CNetworkView::~CNetworkView()
{
 
}


BEGIN_MESSAGE_MAP(CNetworkView, CListView)
	//{{AFX_MSG_MAP(CNetworkView)
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetworkView drawing

void CNetworkView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CNetworkView diagnostics

#ifdef _DEBUG
void CNetworkView::AssertValid() const
{
	CListView::AssertValid();
}

void CNetworkView::Dump(CDumpContext& dc) const
{
	CListView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNetworkView message handlers

void CNetworkView::OnInitialUpdate() 
{
	CListView::OnInitialUpdate();


   GetListCtrl().InsertColumn(0, "Proto", LVCFMT_LEFT, 50);
   GetListCtrl().InsertColumn(1, "Local Address", LVCFMT_LEFT, 150);
   GetListCtrl().InsertColumn(2, "Foreign Address", LVCFMT_LEFT, 150);
   GetListCtrl().InsertColumn(3, "State", LVCFMT_LEFT, 100);

	
   Populate();	
}




BOOL CNetworkView::PreCreateWindow(CREATESTRUCT& cs) 
{
   m_dwDefaultStyle |= LVS_REPORT | LVS_SORTASCENDING;	

	return CListView::PreCreateWindow(cs);
}

void CNetworkView::OnViewRefresh() 
{
	GetListCtrl().DeleteAllItems();
   Populate();	
}

int CNetworkView::Populate()
{
   CWaitCursor wait;
    
   HANDLE					hTrapEvent;
   AsnObjectIdentifier		hIdentifier;
   RFC1157VarBindList		bindList;
   RFC1157VarBind			bindEntry;
   UINT					tcpidentifiers[] = { 1,3,6,1,2,1,6,13,1,1};
   UINT					udpidentifiers[] = { 1,3,6,1,2,1,7,5,1,1};
   AsnInteger				errorStatus, errorIndex;
   TCPINFO					*currentEntry, *newEntry;
   UINT					currentIndex;
   WORD					wVersionRequested;
	WSADATA					wsaData;
	char					localname[HOSTNAMELEN], remotename[HOSTNAMELEN];
	char					remoteport[PORTNAMELEN], localport[PORTNAMELEN];
	char					localaddr[ADDRESSLEN], remoteaddr[ADDRESSLEN];


   //
   // Lists of endpoints
   //
   TCPINFO		TcpInfoTable;
   TCPINFO		UdpInfoTable;
	//
	// Initialize winsock
	//
	wVersionRequested = MAKEWORD( 1, 1 );
	if( WSAStartup(  wVersionRequested, &wsaData ) ) {

		TRACE("Could not initialize Winsock.\n");
		return 1;
	}

	//
	// Locate and initialize INETMIB1
	//
	if( !LoadInetMibEntryPoints()) {

		TRACE("Could not load extension DLL.\n");
		return 1;
	}

	if( !pSnmpExtensionInit( GetCurrentTime(), &hTrapEvent, &hIdentifier )) {

		TRACE("Could not initialize extension DLL.\n");
		return 1;
	}

	//
	// Initialize the query structure once
	//
	bindEntry.name.idLength = 0xA;
	bindEntry.name.ids = tcpidentifiers;
	bindList.list = &bindEntry;
	bindList.len  = 1;

	TcpInfoTable.prev = &TcpInfoTable;
	TcpInfoTable.next = &TcpInfoTable;

	//
	// Roll through TCP connections
	//
	currentIndex = 1;
	currentEntry = &TcpInfoTable;
	while(1) {

		if( !pSnmpExtensionQuery( ASN_RFC1157_GETNEXTREQUEST,
			&bindList, &errorStatus, &errorIndex )) {

			return 1;
		}

		//
		// Terminate when we're no longer seeing TCP information
		//
		if( bindEntry.name.idLength < 0xA ) break;

		//
		// Go back to start of table if we're reading info
		// about the next byte
		//
		if( currentIndex != bindEntry.name.ids[9] ) {

			currentEntry = TcpInfoTable.next;
			currentIndex = bindEntry.name.ids[9];
		}

		//
		// Build our TCP information table 
		//
		switch( bindEntry.name.ids[9] ) {

		case 1:
			
			//
			// Always allocate a new structure
			//
			newEntry = (TCPINFO *) malloc( sizeof(TCPINFO ));
			newEntry->prev = currentEntry;
			newEntry->next = &TcpInfoTable;
			currentEntry->next = newEntry;
			currentEntry = newEntry;

			currentEntry->state = bindEntry.value.asnValue.number;
			break;

		case 2:

			currentEntry->localip = 
				*(UINT *) bindEntry.value.asnValue.address.stream;
			currentEntry = currentEntry->next;
			break;

		case 3:
			
			currentEntry->localport = 
				bindEntry.value.asnValue.number;
			currentEntry = currentEntry->next;
			break;

		case 4:

			currentEntry->remoteip = 
				*(UINT *) bindEntry.value.asnValue.address.stream;
			currentEntry = currentEntry->next;
			break;

		case 5:

			currentEntry->remoteport = 
				bindEntry.value.asnValue.number;
			currentEntry = currentEntry->next;
			break;
		}

	}
	
	//
	// Now print the connection information
	//
	
	currentEntry = TcpInfoTable.next;
	while( currentEntry != &TcpInfoTable ) {

		sprintf( localaddr, "%s:%s", 
			GetIpHostName( TRUE, currentEntry->localip, localname, HOSTNAMELEN), 
			GetPortName( currentEntry->localport, "tcp", localport, PORTNAMELEN ));

		sprintf( remoteaddr, "%s:%s",
			GetIpHostName( FALSE, currentEntry->remoteip, remotename, HOSTNAMELEN), 
			currentEntry->remoteip ? 
				GetPortName( currentEntry->remoteport, "tcp", remoteport, PORTNAMELEN ):
				"0" );

      int nItem = GetListCtrl().InsertItem(0, "TCP");
      GetListCtrl().SetItem(nItem, 1, LVIF_TEXT, localaddr, 0, 0, 0, 0);
      GetListCtrl().SetItem(nItem, 2, LVIF_TEXT, remoteaddr, 0, 0, 0, 0);
      GetListCtrl().SetItem(nItem, 3, LVIF_TEXT, TcpState[currentEntry->state], 0, 0, 0, 0);

		
		currentEntry = currentEntry->next;
	}
	
	//
	// Initialize the query structure once
	//
	bindEntry.name.idLength = 0xA;
	bindEntry.name.ids = udpidentifiers;
	bindList.list = &bindEntry;
	bindList.len  = 1;

	UdpInfoTable.prev = &UdpInfoTable;
	UdpInfoTable.next = &UdpInfoTable;

	//
	// Roll through UDP endpoints
	//
	currentIndex = 1;
	currentEntry = &UdpInfoTable;
	while(1) {

		if( !pSnmpExtensionQuery( ASN_RFC1157_GETNEXTREQUEST,
			&bindList, &errorStatus, &errorIndex )) {

			return 1;
		}

		//
		// Terminate when we're no longer seeing TCP information
		//
		if( bindEntry.name.idLength < 0xA ) break;

		//
		// Go back to start of table if we're reading info
		// about the next byte
		//
		if( currentIndex != bindEntry.name.ids[9] ) {

			currentEntry = UdpInfoTable.next;
			currentIndex = bindEntry.name.ids[9];
		}

		//
		// Build our TCP information table 
		//
		switch( bindEntry.name.ids[9] ) {

		case 1:
			
			//
			// Always allocate a new structure
			//
			newEntry = (TCPINFO *) malloc( sizeof(TCPINFO ));
			newEntry->prev = currentEntry;
			newEntry->next = &UdpInfoTable;
			currentEntry->next = newEntry;
			currentEntry = newEntry;

			currentEntry->localip = 
				*(UINT *) bindEntry.value.asnValue.address.stream;
			break;

		case 2:
			
			currentEntry->localport = 
				bindEntry.value.asnValue.number;
			currentEntry = currentEntry->next;
			break;
		}
	}
	
	//
	// Now print the connection information
	//
	currentEntry = UdpInfoTable.next;
	while( currentEntry != &UdpInfoTable ) {

      int nItem = GetListCtrl().InsertItem(0, "UDP");

      CString str;
      str.Format("%s:%s",
         GetIpHostName( TRUE, currentEntry->localip, localname, HOSTNAMELEN),
         GetPortName( currentEntry->localport, "udp", localport, PORTNAMELEN ));

      GetListCtrl().SetItem(nItem, 1, LVIF_TEXT, str, 0, 0, 0, 0);
		
		currentEntry = currentEntry->next;
	}

	return 0;
}

BOOL CNetworkView::LoadInetMibEntryPoints()
{
	HINSTANCE	hInetLib;

	if( !(hInetLib = LoadLibrary( "inetmib1.dll" ))) {

		return FALSE;
	}

	if( !(pSnmpExtensionInit =  (SEINIT)GetProcAddress( hInetLib,
			"SnmpExtensionInit" )) ) {

		return FALSE;
	}

	if( !(pSnmpExtensionQuery =  (SEQUERY)GetProcAddress( hInetLib,
			"SnmpExtensionQuery" )) ) {

		return FALSE;
	}
	
	return TRUE;
}

LPCSTR CNetworkView::GetIpHostName(BOOL local, UINT ipaddr, char *name, int namelen)
{

   
   

   UINT nipaddr = htonl( ipaddr );
   
   
   //nipaddr = htonl( ipaddr );
   if( !ipaddr  ) 
   {
      
      if( !local ) 
      {
         
         sprintf( name, "%d.%d.%d.%d", 
            (nipaddr >> 24) & 0xFF,
            (nipaddr >> 16) & 0xFF,
            (nipaddr >> 8) & 0xFF,
            (nipaddr) & 0xFF);
         
      } 
      else 
      {
         
         gethostname(name, namelen);
      }
      
   } 
   else if( ipaddr == 0x0100007f ) 
   {
      
      if( local ) 
      {
         
         gethostname(name, namelen);
      } 
      else 
      {
         
         
         strcpy( name, "localhost" );
      }
      
   } 
   else 
   {
      // Its so slow to resolve th network addresses
      // that I have taken it out. May be it could be put into
      // a worker tread or something (ZAC)

      //struct hostent			*phostent;

      //if( phostent = gethostbyaddr( (char *) &ipaddr,
      //   sizeof( nipaddr ), PF_INET )) 
      //{
      //   strcpy( name, phostent->h_name );
      //} 
      //else 
      {
         sprintf( name, "%d.%d.%d.%d", 
            (nipaddr >> 24) & 0xFF,
            (nipaddr >> 16) & 0xFF,
            (nipaddr >> 8) & 0xFF,
            (nipaddr) & 0xFF);
      }
   }

	return name;
}

LPCSTR CNetworkView::GetPortName(UINT port, char *proto, char *name, int namelen)
{
	struct servent *psrvent;

	if( psrvent = getservbyport( htons( (USHORT) port ), proto )) {

		strcpy( name, psrvent->s_name );

	} else {

		sprintf(name, "%d", port);

	}		
	return name;
}
