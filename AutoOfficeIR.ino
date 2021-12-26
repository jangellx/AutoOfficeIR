// AutoOfficeIR
//
// Web server for AutoOffice that uses IR to toggle power to Samsung TVs.  Also
//  features a rotary encoder for adjusting the volume on a Sony A/V reciever adjusting
//  toggling between two inputs.
//
// Rotary encoder tutorial: https://bildr.org/2012/08/rotary-encoder-arduino/
//
// Using ESP Rotary for laziness.  We're not high performance, so testing in loop()
//  instead of using interupts is fine: https://github.com/LennartHennigs/ESPRotary
//
// Fork of IRremote that works with ESP32.  Note that RobotIRRemote needs to be deleted
//  from the built-in libraries to avoid conflicts.  https://github.com/ExploreEmbedded/Arduino-IRremote
//
// Wifi code is a mix of the AutoOffice-ESP8266 code and some simplfiied code from other
//  sources.  Note that on the ESP32, the web server can't be launched until after the
//  wifi is connected or the device will crash on startup.
//
// Build Settings
//  Board:  ESP32 Dev Module (probably any module will work)
//  All other settings at default
// May need to hold down the 'boot" button on the board to get it to conenct for programming.

#include "ESPRotary.h"
#include "IRremote.h"
#include "InputDebounce.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "AutoOfficeIR_Config.h"

#define PIN_ENCODER_A       25		// Encoder A
#define PIN_ENCODER_B       26		// Encoder B
#define PIN_ENCODER_BUTTON  33		// Encoder Button
#define PIN_MAIN_IR_LED      5		// Main transistor for the IR LEDs.  Actually pulses the LEDs.  Cannot be changed.
#define PIN_MONITOR_IR_LED	14		// Transistor that controls if the Samsung TV used as the Windows monitor is enabled.  Disabled when changing inputs on the A/V receiver monitor
#define PIN_INPUT_HDMI4		18		// HDMI Input 4 Button
#define PIN_INPUT_HDMI3		13		// HDMI Input 3 Button
#define PIN_INPUT_HDMI2		32		// HDMI Input 2 Button
#define PIN_INPUT_HDMI1		19		// HDMI Input 1 Button

#define PIN_INPUT_TEST		27		// Test pin

// HDMI pin lookups.  Indices match the input, so 0 is invalid, 1 is HDMI input 1, etc
int hdmiButtonsPins[] = { -1, PIN_INPUT_HDMI1, PIN_INPUT_HDMI2, PIN_INPUT_HDMI3, PIN_INPUT_HDMI4 };
int HDMIButtonFromPin( int pin ) {
	switch( pin ) {
		case PIN_INPUT_HDMI1:	return 1;
		case PIN_INPUT_HDMI2:	return 2;
		case PIN_INPUT_HDMI3:	return 3;
		case PIN_INPUT_HDMI4:	return 4;
	}

    Serial.printf( "ERROR:  invalid pin %d for HDMIButtonFromPin()\n", pin );
	return -1;
}

// Rotary encoder manager
ESPRotary encoder = ESPRotary( PIN_ENCODER_A, PIN_ENCODER_B, 4 );   // 4 pulses per tick is the standard for mechanical encoders

// IR sender instance
IRsend irsend;

// Raw Samsung power codes
unsigned int samsungOnCode[] = {
    4523,4523,579,1683,579,1683,579,1683,579,552,579,552,579,552,579,552,579,552,579,
    1683,579,1683,579,1683,579,552,579,552,579,552,579,552,579,552,579,1683,579,552,
    579,552,579,1683,579,1683,579,552,579,552,579,1683,579,552,579,1683,579,1683,579,
    552,579,552,579,1683,579,1683,579,552,579,47779
};

unsigned int samsungOffCode[] = {
	4523,4523,552,1683,552,1683,552,1683,552,552,552,552,552,552,552,552,552,552,552,1683,552,1683,552,1683,552,552,
	552,552,552,552,552,552,552,552,552,552,552,552,552,552,552,1683,552,1683,552,552,552,552,552,1683,552,1683,552,
	1683,552,1683,552,552,552,552,552,1683,552,1683,552,552,552,43993
};

unsigned int samsungPowerCode[] = {
	4600,4350,700,1550,650,1550,650,1600,650,450,650,450,650,450,650,450,700,400,700,1550,650,1550,650,1600,650,450,650,
	450,650,450,700,450,650,450,650,450,650,1550,700,450,650,450,650,450,650,450,650,450,700,400,650,1600,650,450,650,
	1550,650,1600,650,1550,650,1550,700,1550,650,1550,650
};

// Rawm Samsung HDMI Codes
unsigned int samsungHDMI1Code[] = {
	4523,4523,552,1683,552,1683,552,1683,552,552,552,552,552,552,552,552,552,552,552,1683,552,1683,552,1683,552,552,552,
	552,552,552,552,552,552,552,552,1683,552,552,552,552,552,1683,552,552,552,1683,552,1683,552,1683,552,552,552,1683,
	552,1683,552,552,552,1683,552,552,552,552,552,552,552,4399
};

unsigned int samsungHDMI2Code[] = {
	4523,4523,552,1683,552,1683,552,1683,552,552,552,552,552,552,552,552,552,552,552,1683,552,1683,552,1683,552,552,552,
	552,552,552,552,552,552,552,552,552,552,1683,552,1683,552,1683,552,1683,552,1683,552,552,552,1683,552,1683,552,552,
	552,552,552,552,552,552,552,552,552,1683,552,552,552,43993
};

unsigned int samsungHDMI3Code[] = {
	4523,4523,552,1683,552,1683,552,1683,552,552,552,552,552,552,552,552,552,552,552,1683,552,1683,552,1683,552,552,552,
	552,552,552,552,552,552,552,552,552,552,1683,552,552,552,552,552,552,552,552,552,1683,552,1683,552,1683,552,552,552,
	1683,552,1683,552,1683,552,1683,552,552,552,552,552,43993
};

unsigned int samsungHDMI4Code[] = {
	4523,4497,552,1709,552,1709,552,1709,552,579,552,579,552,579,552,579,552,579,552,1709,552,1709,552,1709,552,579,552,
	579,552,579,552,579,552,579,552,1709,552,579,552,1709,552,579,552,579,552,579,552,1709,552,1709,552,579,552,1709,552,
	579,552,1709,552,1709,552,1709,552,579,552,579,552,4399
};

// Raw Yamaha volume codes
unsigned int yamahaVolUpCode[] = {
	8777,4646,568,568,568,1704,568,568,568,1704,568,1704,568,1704,568,1704,568,568,568,1704,568,568,568,1704,568,568,568,568,
	568,568,568,568,568,1704,568,568,568,1704,568,568,568,1704,568,1704,568,568,568,568,568,568,568,1704,568,568,568,1704,568,
	568,568,568,568,1704,568,1704,568,1704,568,3970
};

unsigned int yamahaVolDownCode[] = {
	8777,4646,568,568,568,1704,568,568,568,1704,568,1704,568,1704,568,1704,568,568,568,1704,568,568,568,1704,568,568,568,568,
	568,568,568,568,568,1704,568,1704,568,1704,568,568,568,1704,568,1704,568,568,568,568,568,568,568,568,568,568,568,1704,568,
	568,568,568,568,1704,568,1704,568,1704,568,3970
};

// NEC Yamaha scene 1-4 codes
unsigned int yamahaHDMICodes[4] = { 
	0x5EA100FE, 0x5EA1C03E, 0x5EA1609E, 0x5EA1906E
};

#define RECIEVER_SONY     0
#define RECIEVER_YAHAMA   1
int recieverTypeForVolume = RECIEVER_YAHAMA;

static unsigned int *samsungHDMICodes[4]   = { samsungHDMI1Code, samsungHDMI2Code, samsungHDMI3Code, samsungHDMI4Code };
static int  samsungHDMILengths[4] = { 0, 0, 0, 0 };

// Get the Samsung HDMI code.  Code length is indirectly returned.
unsigned int * SamsungHDMICodeFromPin( int pin, int *len ) {
	// Initialize lengths, if needed
	if( samsungHDMILengths == 0 ) {
		samsungHDMILengths[0] = sizeof( samsungHDMI1Code ) / sizeof( samsungHDMI1Code[0] );
		samsungHDMILengths[1] = sizeof( samsungHDMI2Code ) / sizeof( samsungHDMI2Code[0] );
		samsungHDMILengths[2] = sizeof( samsungHDMI3Code ) / sizeof( samsungHDMI3Code[0] );
		samsungHDMILengths[3] = sizeof( samsungHDMI4Code ) / sizeof( samsungHDMI4Code[0] );
	}

	auto index = HDMIButtonFromPin( pin ) - 1;
	if( index == -1 ) {
		Serial.printf( "ERROR:  invalid pin %d for SamsungHDMICodeFromPin()\n", pin );
		return NULL;
	}

	// Samsung code
	*len = samsungHDMILengths[ index ];
	return samsungHDMICodes[  index ]; 
}

// Input debouncing
InputDebounce  debounceButton, debounceHDMI1, debounceHDMI2, debounceHDMI3, debounceHDMI4, debounceTest;
#define BUTTON_DEBOUNCE_DELAY   20   // [ms]

void setup() {
    // Open the serial port
    Serial.begin(115200);
    delay(1000);
    Serial.print( "\nAutoOfficeIR\n" );

	// Button Pin Setup
    pinMode( PIN_ENCODER_BUTTON, INPUT_PULLUP );
    pinMode( PIN_INPUT_HDMI1,    INPUT_PULLUP );
    pinMode( PIN_INPUT_HDMI2,    INPUT_PULLUP );
    pinMode( PIN_INPUT_HDMI3,    INPUT_PULLUP );
    pinMode( PIN_INPUT_HDMI4,    INPUT_PULLUP );
 
    pinMode( PIN_INPUT_TEST,     INPUT_PULLUP );

	// Encoder Pin Setup
    encoder.setLeftRotationHandler(  encoderDir );
    encoder.setRightRotationHandler( encoderDir );

	// LED Pin Setup
    pinMode( PIN_MONITOR_IR_LED, OUTPUT );

    digitalWrite( PIN_MAIN_IR_LED,    HIGH );
    digitalWrite( PIN_MONITOR_IR_LED, LOW  );		// LOW enables the transistor for the monitor

	// Encoder Debouncing
    debounceButton.setup( PIN_ENCODER_BUTTON, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES );
    debounceButton.registerCallbacks( buttonPressed, buttonReleased, NULL, NULL );

	// HDMI Button Debouncing.  All use the same callbacks, and we just check the pin number
    debounceHDMI1.setup( PIN_INPUT_HDMI1, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES );
    debounceHDMI1.registerCallbacks( hdmiButtonPressed, hdmiButtonReleased, NULL, NULL );

    debounceHDMI2.setup( PIN_INPUT_HDMI2, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES );
    debounceHDMI2.registerCallbacks( hdmiButtonPressed, hdmiButtonReleased, NULL, NULL );

    debounceHDMI3.setup( PIN_INPUT_HDMI3, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES );
    debounceHDMI3.registerCallbacks( hdmiButtonPressed, hdmiButtonReleased, NULL, NULL );

    debounceHDMI4.setup( PIN_INPUT_HDMI4, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES );
    debounceHDMI4.registerCallbacks( hdmiButtonPressed, hdmiButtonReleased, NULL, NULL );

    debounceTest.setup( PIN_INPUT_TEST, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES );
    debounceTest.registerCallbacks( testButtonPressed, testButtonReleased, NULL, NULL );

    // Set up the web server
    SetupWebServer();
}

// Rotary Encoder
//  On left or right rotation, change the volume on the Sony receiver.
//  Each code has to be sent 3 times with a small delay between them.
//  If we sent the last code very recently in the same direction, we
//  send the repeat code instead.

// For the Yamaha receiver, we only send each code once.

// Repeat codes aren't working, though, so we've disabled them.
#define SONY_REPEAT
void encoderDir( ESPRotary &r ) {
	static int lastDirWasDown      = false;
	static int timeSinceLastIRSend = 0;

	bool isDirDown = (encoder.getDirection() == 1);

   #ifdef SONY_REPEAT )
	if( (lastDirWasDown == isDirDown) && (millis() - timeSinceLastIRSend < 100) ) {
		// Same direction as last time, and it has been a short amonut of time; send the repeat code
		irsend.sendNEC( REPEAT, 12 );
	    Serial.println( "Dir (Repeat)" );

	} else {
   #endif

	// New direction or more time has passed; send new codes
	Serial.printf( "Dir:  %d, %s", isDirDown? 1 : -1,  isDirDown ? "CCW (volume down)" : "CW (volume up)" );
	if( recieverTypeForVolume == RECIEVER_YAHAMA ) {
		if( isDirDown ) {
			// Volume down
			irsend.sendRaw( yamahaVolDownCode, sizeof( yamahaVolDownCode ) / sizeof( yamahaVolDownCode[0] ), 38 );

		} else {
			// Volume up
			irsend.sendRaw( yamahaVolUpCode, sizeof( yamahaVolUpCode ) / sizeof( yamahaVolUpCode[0] ), 38 );
		}

	    Serial.print( " sent\n" );

	} else if( recieverTypeForVolume == RECIEVER_SONY ) {
		// Sony
		for( int i=0; i < 3; i ++ ) {
			if( isDirDown ) {
				// Volume down
				irsend.sendSony( 0x640C, 15 );

			} else {
				// Volume up
				irsend.sendSony( 0x240C, 15 );
			}

			if( i != 2 )
				delay( 5 );

			Serial.print( "." );
		}

	    Serial.print( " sent\n" );
	}

   #ifdef SONY_REPEAT )
		lastDirWasDown = isDirDown;
	}
   #endif

	timeSinceLastIRSend = millis();
}

bool tvsAreOn = false;      // Last state we set on the TVs

// Encoder Button
//  On button press after debouncing
void buttonPressed( uint8_t pinIn ) {
//     digitalWrite( PIN_MAIN_IR_LED, LOW );
 
	TurnOnTVs( !tvsAreOn );
}

//  On release after debouncing
void buttonReleased( uint8_t pinIn ) {
//    digitalWrite( PIN_MAIN_IR_LED, HIGH );
    Serial.printf( "Button:  Up\n" );
}

// HDMI Button
//  On button press after debouncing.  Store the time the button went down.
static int hdmiButtonDownAt = 0;													// When the button went down.  We could use separate tests for each button, but you shouldn't be holding two buttons at once.
#define    HDMI_BUTTON_HOLD_PERIOD   1000											// How long to wait before doing the alternate (Samsung) input codes instead of the Yamaha codes
void hdmiButtonPressed( uint8_t pinIn ) {
	hdmiButtonDownAt = millis();
}

//  On release after debouncing
void hdmiButtonReleased( uint8_t pinIn ) {
	bool wasHeld = (millis() - hdmiButtonDownAt) > HDMI_BUTTON_HOLD_PERIOD;
	if( !wasHeld ) {
	}

	if( wasHeld ) {
		// Held; get the Samsung IR code for this input
		int  hdmiCodeLen;
		auto hdmiCode = SamsungHDMICodeFromPin( pinIn, &hdmiCodeLen );
		if( hdmiCode == NULL )
			return;

		// Send this input's code
		digitalWrite( PIN_MONITOR_IR_LED, HIGH );									// Disable the Windows monitor so we only change intputs on the Mac mini one
		irsend.sendRaw( hdmiCode, hdmiCodeLen, 38 );
		digitalWrite( PIN_MONITOR_IR_LED, LOW  );									// Turn back on the Windows monitor

	} else {
		// No theld; first switch to Samsung HDMI 4, since that's where the Yamaha reciever is
		digitalWrite( PIN_MONITOR_IR_LED, HIGH );									// Disable the Windows monitor so we only change intputs on the Mac mini one
		irsend.sendRaw( samsungHDMI4Code, sizeof( samsungHDMI4Code ) / sizeof( samsungHDMI4Code[0]  ), 38 );
		digitalWrite( PIN_MONITOR_IR_LED, LOW  );									// Turn back on the Windows monitor

		// Send the input's code to the reciever
		irsend.sendNEC( yamahaHDMICodes[ HDMIButtonFromPin( pinIn ) - 1 ], 32 );	// -1 for index
	}

	Serial.printf( "HDMI %d %s; switching to input on %s\n", HDMIButtonFromPin( pinIn ), wasHeld ? "held" : "pressed", wasHeld ? "Samsung TV" : "Yamaha reciever" );
}

// Test Button
//  On button press after debouncing
void testButtonPressed( uint8_t pinIn ) {
	Serial.printf( "Test button pressed\n" );
}

//  On release after debouncing
void testButtonReleased( uint8_t pinIn ) {
	Serial.printf( "Test button released\n" );
}

// Called to let the WebHooks client know that our state (specifically, the "TVs on" state) has updated.
void UpdateWebhooks(void) {
    // Report to WebHooks
    String url = String( webhooksURL) + "/?accessoryId=" + String( accessoryId ) + "&state=" + String( tvsAreOn ? "true" : "false" );
    Serial.print( "Updating HTTPWebhooks with new state of " );
    Serial.print( tvsAreOn ? "true" : "false" );
    Serial.print( " at " );
    Serial.println( url );
    
    HTTPClient  http;
    http.begin( url );
    int error = http.sendRequest( "PUT" );
    
    if( error < 0 ) {
        Serial.print(   " - HTTP PUT request failed:  " );
        Serial.println( http.errorToString( error ) );
    } else {
        Serial.print(   " - HTTP response:  " );
        Serial.println( error );
        Serial.println( " ------------------------------ " );
        Serial.println( http.getString() );
        Serial.println( " ------------------------------ " );
    }
}


// Toggle the state of the TVs.
void TurnOnTVs( bool state ) {
    // Discrete
	digitalWrite( PIN_MONITOR_IR_LED, LOW );	// Set to low to make sure the Windows monitor pin is on so the power signal goes to it too

    for( int i=0; i < 3; i++ ) {
        if( state ) {
            irsend.sendRaw( samsungOnCode,  sizeof( samsungOnCode )  / sizeof( samsungOnCode[0]  ), 38 );
        } else {
            irsend.sendRaw( samsungOffCode, sizeof( samsungOffCode ) / sizeof( samsungOffCode[0] ), 38 );
        }
    }

    if( state )
		Serial.printf( "Sent Samsung code for on (%d parts)\n", sizeof( samsungOnCode ) / sizeof( samsungOnCode[0] ) );
	else
		Serial.printf( "Sent Samsung code for off (%d parts)\n", sizeof( samsungOffCode ) / sizeof( samsungOffCode[0] ) );

	tvsAreOn = state;

    // Toggle
//  irsend.sendSAMSUNG(0xE0E040BF, 32);
//	irsend.sendRaw( samsungPowerCode,  sizeof( samsungPowerCode )  / sizeof( samsungPowerCode[0]  ), 38 );

    // Report to WebHooks
    UpdateWebhooks();
}

// Web Server
WebServer	 webServer( serverPort );
#define WIFI_CONNECT_RETRY_DELAY		10000	// How long to wait before trying to conenct to wifi again, if auto-recoonect doesn't do it for us

// Send a response to the server containing the "isAwake" state as JSON.
void RespondWithAwakeStateJSON( bool state ) {
	DynamicJsonDocument	 jsonDoc(256);					// Must be declared here (as opposed to globally), or we run out of memory and stop parsing eventually ( https://bblanchon.github.io/ArduinoJson/faq/the-first-parsing-succeeds-why-do-the-next-ones-fail/ )

	jsonDoc["isAwake"] = state ? 1 : 0;

	char			 buf[256];
	serializeJson( jsonDoc, buf );

	webServer.send( 200, "applicationt/json", buf );
}

//  Set up the web server and the paths we handle.
void SetupWebServer() {
    Serial.println( "Setting up web server..." );

    Serial.println( "  /" );
	webServer.on( "/", [](){
		// Just display the status
		String	string = "AutoOfficeIR Server Running.\n\n";
        string += "TVs were last set to " + String( tvsAreOn ? "on" : "off" ) + ".";
		webServer.send( 200, "text/html", string );
	});

    Serial.println( "  /do" );
	webServer.on( "/do", HTTP_PUT, [](){
		// PUT request to toggle TV power
		DynamicJsonDocument	 jsonDoc(256);				// Must be declared here (as opposed to globally), or we run out of memory and stop parsing eventually ( https://bblanchon.github.io/ArduinoJson/faq/the-first-parsing-succeeds-why-do-the-next-ones-fail/ )
		DeserializationError result = deserializeJson( jsonDoc, webServer.arg(0) );

		if( result != DeserializationError::Ok ) {
			Serial.print( "/do request; error parsing JSON:  " );
			Serial.print( webServer.arg(0) );
			Serial.println();

		} else {
			const char	*command = jsonDoc["command"];
			if( command == NULL ) {
				Serial.println( "/do request; command not found." );

			} else if( strcmp( command, "wake" ) == 0 ) {
                TurnOnTVs( true );
                Serial.println( "/do request; wake - TVs on." );

			} else if( strcmp( command, "sleep" ) == 0 ) {
                TurnOnTVs( false );
                Serial.println( "/do request; sleep - TVs off." );

			} else {
				Serial.println( "/do request; unknown command." );
			}
		}

		RespondWithAwakeStateJSON( tvsAreOn  );
	});

    Serial.println( "  /sleep" );
	webServer.on( "/sleep", HTTP_GET, [](){
		// Sleep
		if( !tvsAreOn ) {
			Serial.println( "/sleep request; TVs already off, nothing to do." );
		} else {
			TurnOnTVs( false );
	
			Serial.println(   "/sleep request; turning TVs off." );
		}

		RespondWithAwakeStateJSON( false );
	});

    Serial.println( "  /wake" );
	webServer.on( "/wake", HTTP_GET, [](){
		// Wake
		if( tvsAreOn ) {
			Serial.println( "/wake request; TVs already on, nothing to do." );
		} else {
			TurnOnTVs( true );
	
			Serial.println(   "/wake request; turning TVs on." );
		}

		RespondWithAwakeStateJSON( true );
	});

    Serial.println( "  /status" );
	webServer.on( "/status", HTTP_GET, [](){
		// Status as JSON
		Serial.println( "/status request." );

		RespondWithAwakeStateJSON( tvsAreOn );
	});

//    Serial.println( "Starting server..." );
//	webServer.begin();

//	Serial.println( "Web server started." );
}

// Connect to wifi.  Two things we learned here:
//  - For some reason, WL_IDLE_STATUS is a lie, and we're getting it when we dont' expect it
//  - For some reason, while attempting to connected we're getting WL_DISCCONECTED instead of
//    WL_IDLE_STATUS, so we keep canceling and trying to connect again.
// For this reason, we ignore the IDLE_STATE and use a 10 second delay to try to reconnect.
//  This seems to do the trick... for now...
// Also, we enabled auto-reconnect.  That means we shouldn't need to try to reconnect ourselves,
//  but if we see that we're not connected we'll try to do that anyway.
void ConnectToWifi() {
	static int	 lastConnectAttempt = 0;
	static bool	 wasConnected       = false;
	int		     curStatus          = WiFi.status();

	// Check the status.  If we're connected, there's nothing to do.
	if( curStatus == WL_CONNECTED ) {
		if( !wasConnected ) {
			Serial.print(  "Wifi connected with ip " );
			Serial.println( WiFi.localIP() );

			// Print the MAC address of the ESP8266
			byte mac[6];
			WiFi.macAddress(mac);
			Serial.print("MAC Address: ");
			Serial.print(mac[5],HEX);
			Serial.print(":");
			Serial.print(mac[4],HEX);
			Serial.print(":");
			Serial.print(mac[3],HEX);
			Serial.print(":");
			Serial.print(mac[2],HEX);
			Serial.print(":");
			Serial.print(mac[1],HEX);
			Serial.print(":");
			Serial.println(mac[0],HEX);

			wasConnected = true;

			// The web server can't be started until after we have connected to wifi on the ESP32
			Serial.print( "Starting web server..." );
			webServer.begin();
			Serial.println( " done." );

		}

		return;
	}

	// If we're trying to conenct (IDLE_STATUS), we just return
//	if( curStatus == WL_IDLE_STATUS )
//		return;

	// Wait a bit before trying to connect, if necessary
	if( millis() < (lastConnectAttempt + WIFI_CONNECT_RETRY_DELAY) ) 
		return;

	lastConnectAttempt = millis();

	// Report status
	if( curStatus == WL_DISCONNECTED )  	Serial.println( "Wifi disconnected." );
	if( curStatus == WL_CONNECT_FAILED )	Serial.println( "Wifi connection failed." );
	if( curStatus == WL_CONNECTION_LOST )	Serial.println( "Wifi connection lost." );
	
	// If we got this far, we try to conenct to the access point access point
	Serial.print( "Connecting to wifi access point " );
	Serial.print( wifiSSID );
	Serial.println( "..." );

    WiFi.setAutoReconnect( true  );			// Auto-reconnect if we lose the connection
	WiFi.persistent(       false );			// Don't remember IP address on next connection

	WiFi.setHostname( "AutoOfficeIR" );
//	wifi_station_set_hostname( "AutoOfficeIR" );
//	WiFi.mode(   WIFI_STA );
	WiFi.begin(  wifiSSID, wifiPassword );

	if( wifiUseStaticIP )
		WiFi.config( wifiIP, wifiSubnet, wifiGateway, wifiDNS1, wifiDNS2 );

//	WiFi.printDiag( Serial );
}

// Main Loop
void loop() {
    // Connect or reconnect to wifi
    ConnectToWifi();

    // Handle HTTP requests
    webServer.handleClient();

    // Test the encoder
    encoder.loop();

    // Debounce the buttons
    debounceButton.process( millis() );
    debounceHDMI1.process(  millis() );
    debounceHDMI2.process(  millis() );
    debounceHDMI3.process(  millis() );
    debounceHDMI4.process(  millis() );
    debounceTest.process(  millis() );
}
