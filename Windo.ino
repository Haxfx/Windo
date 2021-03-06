#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include <Servo.h> 

Servo myservo;  // create servo object to control a servo 
                // a maximum of eight servo objects can be created 
                
int pos = 0;    // variable to store the servo position 
int incomingByte = 0;  

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define WLAN_SSID       "Seb iFoon"           // cannot be longer than 32 characters!
#define WLAN_PASS       "123abcd-/"

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  30000     // Amount of time to wait (in milliseconds) with no data
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.

// What page to grab!
#define API_KEY      "af27abcce2eb087beb03b1b606dd5"      // Sign up at https://www.worldweatheronline.com/
#define CITY         "Rotterdam"  // Provice/city, or postal code

#define WEBSITE      "api.worldweatheronline.com"
#define WEBPAGE      "/free/v2/weather.ashx?format=csv&q=" CITY "&key=" API_KEY


/**************************************************************************/
/*!
    @brief  Sets up the HW and the CC3000 module (called automatically
            on startup)
*/
/**************************************************************************/
#define COLS 16
#define ROWS 2

uint32_t ip;

void setup(void)
{
  Serial.begin(115200);
  Serial.println(F("Welcome to Windo!\n"));
//
//  Serial.print(F("Free RAM: ")); Serial.println(getFreeRam(), DEC);

  /* Initialise the module */
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("ERR. INIT CC3000"));
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
}

bool connect(Adafruit_CC3000_Client& www) {
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    return false;
  }

  Serial.println(F("Connected!"));

  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }

  /* Display the IP address DNS, Gateway, etc. */
  while (! displayConnectionDetails()) {
    delay(1000);
  }

  ip = 0;
  // Try looking up the website's IP address
  Serial.print(WEBSITE); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }

  cc3000.printIPdotsRev(ip);

  www = cc3000.connectTCP(ip, 80);

  if (!www.connected()) {
    cc3000.disconnect();
    Serial.println(F("ERR. TCP CONNECT"));
    Serial.println("ERROR: connection failed");
    return false;
  }

  return true;
}

void disconnect(Adafruit_CC3000_Client& www) {
  Serial.println(F("\n\nClosing"));
  www.close();
  Serial.println(F("\n\nDisconnecting"));
  cc3000.disconnect();
}


void loop(void)
{
  Adafruit_CC3000_Client www;
  if (connect(www)) {
    pollWeather(www);
    disconnect(www);
  }
  delay(15 * 10000);
}

void pollWeather(Adafruit_CC3000_Client& www) {
  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  www.fastrprint(F("GET "));
  www.fastrprint(F(WEBPAGE));
  www.fastrprint(F(" HTTP/1.1\r\n"));
  www.fastrprint(F("Host: ")); www.fastrprint(F(WEBSITE)); www.fastrprint(F("\r\n"));
  // www.fastrprint(F("Connection: Close")); www.fastrprint(F("\r\n"));
  www.fastrprint(F("\r\n"));
  www.println();

  /* Read the HTTP status line */
  String status = readLine(www);
  if (status.compareTo("HTTP/1.1 200 OK") != 0) {
    Serial.print(F("ERR. HTTP STATUS"));
    Serial.print(F("ERROR: unexpected HTTP header - ")); Serial.println(status);
    return;
  }
  status = "";

  /* Read the HTTP headers */
  long contentLength;
  while (true) {
    String name, value;

    if (!readHeader(www, name, value)) {
      Serial.println(F("ERR. HTTP HEAD"));
      Serial.println(F("ERROR: problem with headers"));
      return;
    }

    if (name.length() == 0 && value.length() == 0) {
      break;
    } else if (name.compareTo("Content-Length") == 0) {
      contentLength = value.toInt();
    }
  }

  /* Read the body */
  long consumed = 0;

  bool comment = false;
  long row = 0;
  long col = 0;
  String cell;

  while (consumed < contentLength) {
    int16_t c = readChar(www);
    if (c < 0) {
      Serial.println(F("ERR. HTTP BODY"));
      Serial.println(F("ERROR: content truncated"));
      return;
    }

    consumed++;

    switch (c) {
    case '#':
      comment = true;
      break;
    case '\n':
      comment = false;
      if (col > 0 || cell.length() > 0) {
        handleCell(row, col, cell);
        cell = "";
        col = 0;
        row++;
      }
      break;
    case ',':
      if (!comment) {
        handleCell(row, col, cell);
        cell = "";
        col++;
      }
      break;
    default:
      if (!comment) {
        cell += (char) c;
      }
      break;
    }
  }
}

void handleCell(long row, long col, String& cell) {
//  Serial.print(F("row = ")); Serial.print(row); Serial.print(F(", col = "));
//  Serial.print(col); Serial.print(F(": ")); Serial.println(cell);

  if (row == 0 && col == 5) {
    Serial.println(cell);
    
    if((cell == "Sunny") || (cell == "Cloudy") || (cell == "Partly Cloudy")){
        incomingByte = 85;
    }  
    if(cell == "Patchy light"){
        incomingByte = 90;
    }  
    if(cell == "Patchy rain"){
        incomingByte = 80;
    }  
  }
  
  Serial.println(incomingByte);
        
    myservo.attach(9);
    
    if(pos < incomingByte){
      for(pos; pos < incomingByte; pos += 1)  
      {                                  // in steps of 1 degree 
        myservo.write(pos);              // tell servo to go to position in variable 'pos' 
        Serial.println(pos);
        delay(15);                       // waits 15ms for the servo to reach the position 
        
      }
    } 
    if(pos > incomingByte){
      for(pos; pos>incomingByte; pos-=1)     // goes from 180 degrees to 0 degrees 
      {                                
        myservo.write(pos);              // tell servo to go to position in variable 'pos' 
        delay(15);                       // waits 15ms for the servo to reach the position 
      }
    } else {
      if(pos == incomingByte)
      myservo.detach();
    }
}

bool readHeader(Adafruit_CC3000_Client& www, String& name, String& value) {
  int16_t c;

  /* Read header name */
  while (true) {
    c = readChar(www);
    if (c < 0) {
      return false; /* disconnect while reading header name */
    } else if (c == ':') {
      break; /* end of header name */
    } else {
      name += (char) c;

      if (name.endsWith("\r\n")) {
        /* Should signify the end of headers */
        name = name.substring(0, name.length()-2);
        return true;
      }
    }
  }

  /* Read header value */
  while (true) {
    /* Discard whitespace */
    while (true) {
      c = readChar(www);
      if (c < 0) {
        return false; /* disconnect */
      } else if (c == ' ' || c == '\t') {
        continue;
      } else {
        value += (char) c;
        break;
      }
    }

    /* Read until CRLF */
    while (true) {
      c = readChar(www);

      if (c < 0) {
        return false;
      }

      value += (char) c;

      if (value.endsWith("\r\n")) {
        value = value.substring(0, value.length()-2);
        break;
      }
    }

    /* Check for header continuation */
    c = readChar(www);

    if (c != ' ' && c != '\t') {
      putBack(c);
      break;
    } else {
      value += ' ';
    }
  }
}

int16_t buf = -1;

void putBack(char c) {
  if (buf >= 0) {
    Serial.println(F("ERROR: too many characters put back!"));
    return;
  }

  buf = c;
}

int16_t readChar(Adafruit_CC3000_Client& www) {
  if (buf >= 0) {
    char c = (char) buf;
    buf = -1;
    return c;
  }

  while (www.connected()) {
    if (www.available()) {
      return www.read();
    }
  }
  return -1;
}

String readLine(Adafruit_CC3000_Client& www) {
  /* Read data until either the connection is closed, or the idle timeout is reached. */
  String line = "";
  int16_t c;
  while (0 <= (c = readChar(www))) {
    line += (char) c;
    if (line.endsWith("\r\n")) {
      return line.substring(0, line.length()-2);
    }
  }

  return line;
}

/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}
