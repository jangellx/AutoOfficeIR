
#ifndef AUTOOFFICEIR_CONFIG
#define AUTOOFFICEIR_CONFIG

// Office Lights Configuration
//
// Here we store all of the things that make the OfficeLights app unique to a particular installation.
//  This ranges from the router we're connecting to to the SmartThings authentication strings.

// The wifi state is specific to your setup.  If wifiUseStaticIP is true, then the gateway,
//  subnet mask and static IP are also required
const char	*wifiSSID     = "MySSIDHere";
const char	*wifiPassword = "myPasswordHere";

bool		 wifiUseStaticIP = false;							    	// If true, the following five must also be defined
IPAddress	 wifiSubnet(  255, 255, 255,   0 );
IPAddress	 wifiIP(      192, 168,   1, 230 );
IPAddress	 wifiGateway( 192, 168,   1,   1 );
IPAddress	 wifiDNS1(    208,  67, 222, 222 );							// OpenDNS 1
IPAddress	 wifiDNS2(    208,  67, 220, 220 );							// OpenDNS 2

// Port to run the wifi server on.
int		 serverPort = 8182;

#endif
