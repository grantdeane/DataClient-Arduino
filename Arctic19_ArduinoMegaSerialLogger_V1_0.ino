/*
Arctic 2019 Metling Frame Sensor Serial Logger
GBD 1/2/19

Written for the Arduino Mega2560

UART0 : PC USB
UART1 : esp8622
UART2 : GPS
UART3 : N/C
SPI : SD module, chip select 53

 */
#include <SPI.h>
#include <SD.h>

#define SDMODULE_CS_PIN 53                  // pin define for the SD card module
#define UART_BUFFER_LEN 256                 // length of UART character buffer
#define UART_COUNT 4                        // number of UARTS on the Mega2650 to monitor
#define MS_HEADER_LEN 32                    // message string header length
#define LOGGER_1CHARID 'A'                  // serial number for this logger
#define DATAFILENAME_UART0 "AUART0.txt"     // SD datafile name
#define DATAFILENAME_UART1 "AUART1.txt"     // SD datafile name
#define DATAFILENAME_UART2 "AUART2.txt"     // SD datafile name
#define DATAFILENAME_UART3 "AUART3.txt"     // SD datafile name

boolean SDLoggingEnabled = false;           // logging function state
char buf[UART_COUNT][UART_BUFFER_LEN];      // buffers for UART characters

unsigned long tick0;                        // ms count of first character in serial buffer
unsigned long tick1;                        // max value: 4294967295
unsigned long tick2;
unsigned long tick3;
unsigned long cmdtick;                      // system runtime in ms; for command kernal timing

typedef unsigned int BufferIndex;           // use this type for buffer index variables
BufferIndex bufix0 = 0;                     // index variables for the UART character buffers
BufferIndex bufix1 = 0;
BufferIndex bufix2 = 0;
BufferIndex bufix3 = 0;
                                            // buffer for logged/broadcast message of UART chars
char messageString[UART_BUFFER_LEN + MS_HEADER_LEN];

boolean bufeol0 = false;                    // true if eol, i.e. line feed 0x0A detected
boolean bufeol1 = false;
boolean bufeol2 = false;
boolean bufeol3 = false;

boolean bufoverrun0 = false;                // true if a UART buffer overrun occurs 
boolean bufoverrun1 = false;
boolean bufoverrun2 = false;
boolean bufoverrun3 = false;

File root;

void setup() {
  
  // Open primary serial communications and wait for port to open
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Configure esp8266
  Serial.println("esp8622 setup");
  Serial1.begin(115200);
   // Set the WiFi module to soft access port and station mode
  do {
    Serial1.println("AT+CWMODE_CUR=3");
  } while (!esp8266WaitOK());
  Serial.println("\nesp8622 WiFi mode set to 3");  
  // Connect to the wireless router
  do {
    Serial1.println("AT+CWJAP=\"NETGEAR94\",\"melodickayak627\"");
    //delay(5000);
  } while (!esp8266WaitOK());  
  Serial.println("\nesp8266 connected to network");
  // Close any UDP connections open on the esp8266 from a prior boot
  Serial1.println("AT+CIPCLOSE=4");
  esp8266WaitOK();
  Serial.println("\nAny existing esp8622 connections closed");
  // Configure for multiple IP connections
  do {
    Serial1.println("AT+CIPMUX=1");
  } while (!esp8266WaitOK());
  Serial.println("\nesp8622 configured for multiple connections");
  // Start UDP client for all subnet listeners on port 8080
  do {
    Serial1.println("AT+CIPSTART=4,\"UDP\",\"192.168.1.255\",8080,8080,0");
  } while (!esp8266WaitOK());
  Serial.println("\nesp8266 configured as UDP client"); 
 
  // Initialize UART2 and GPS module connected to it
  Serial2.begin(9600);
  Serial2.println("$PMTK220,10000*2F"); // a fix every 10,000 ms
  Serial2.println("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29"); //GPRMC message only

  // initialize Secure Digital storage card
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SDMODULE_CS_PIN))
    Serial.print("SD card initialization failed.\nLogging functions disabled\n");
  else {
    Serial.println(F("SD card initilized."));
    SDLoggingEnabled = true;  // Enable logging if the serial card initializes
  }

  // Initialize the serial character buffers
  buf[0][0] = '\0'; 
  buf[1][0] = '\0'; 
  buf[2][0] = '\0';
  buf[3][0] = '\0';
}

void loop() {
  // Create time-stamped message strings from serial port characters
  if (Serial.available()) {
    if (bufix0 == 0)                // timestamp the first character in the buffer
      tick0 = millis();
    byte inByte = Serial.read();    // read the character from the UART
    bufeol0 = (inByte == 0x0A);     // set the end of line flag
    buf[0][bufix0++] = inByte;      // store the character and increment the buffer index
    if (bufix0 == UART_BUFFER_LEN) {
      bufoverrun0 = true;           // a buffer overrun occurred
      bufix0 = 0;
    }
    buf[0][bufix0] = '\0';          // add a string terminator
  }
   
  if (Serial1.available()) {
    if (bufix1 == 0)
      tick1 = millis();
    byte inByte = Serial1.read();
    bufeol1 = (inByte == 0x0A);
    buf[1][bufix1++] = inByte;
    if (bufix1 == UART_BUFFER_LEN) {
      bufoverrun1 = true;
      bufix1 = 0;
    }
    buf[1][bufix1] = '\0';
  }   

  if (Serial2.available()) {
    if (bufix2 == 0)
      tick2 = millis();
    byte inByte = Serial2.read();
    bufeol2 = (inByte == 0x0A);
    buf[2][bufix2++] = inByte;
    if (bufix2 == UART_BUFFER_LEN) {
      bufoverrun2 = true;
      bufix2 = 0;
    }
    buf[2][bufix2] = '\0';
  }
     
  if (Serial3.available()) {
    if (bufix3 == 0)
      tick3 = millis();
    byte inByte = Serial3.read();
    bufeol3 = (inByte == 0x0A);
    buf[3][bufix3++] = inByte;
    if (bufix3 == UART_BUFFER_LEN) {
      bufoverrun3 = true;
      bufix3 = 0;
    }
    buf[3][bufix3] = '\0';
  }   

  // end of line occurred: process the buffer
  if (bufeol0) {
    sprintf(messageString, "%c:UART0:%lu:%d:%s", LOGGER_1CHARID,tick0,strlen(buf[0]),buf[0]);
    Serial.print(messageString);
    bufeol0 = false;
    bufix0 = 0;
    if (SDLoggingEnabled)
      logMsgStr(DATAFILENAME_UART0,messageString);
  }
  
  if (bufeol1) {
    sprintf(messageString, "%c:UART1:%lu:%d:%s", LOGGER_1CHARID,tick1,strlen(buf[1]),buf[1]);
    Serial.print(messageString);
    bufeol1 = false;
    bufix1 = 0;
    //if (SDLoggingEnabled)
      //logMsgStr(DATAFILENAME_UART1,messageString);
    //esp8266SendUDPMessage(messageString);
  }
  
  if (bufeol2) {
    sprintf(messageString, "%c:UART2:%lu:%d:%s", LOGGER_1CHARID,tick2,strlen(buf[2]),buf[2]);
    Serial.print(messageString);
    bufeol2 = false;
    bufix2 = 0;
    if (SDLoggingEnabled) {
      logMsgStr(DATAFILENAME_UART2,messageString);
    }
    SendUDPMessage(messageString);
  }

  if (bufeol3) {
    sprintf(messageString, "%c:UART3:%lu:%d:%s", LOGGER_1CHARID,tick3,strlen(buf[3]),buf[3]);
    Serial.print(messageString);
    bufeol3 = false;
    bufix3 = 0;
    if (SDLoggingEnabled) {
      logMsgStr(DATAFILENAME_UART3,messageString);
    }
    SendUDPMessage(messageString);
  }

  
  // buffer overrun occurred
  if (bufoverrun0) {
    Serial.println("<Warning> Serial buffer over-run on primary UART0");
    bufoverrun0 = false;
  }
  if (bufoverrun1) {
    Serial.println("<Warning> Serial buffer over-run on UART1");
    bufoverrun1 = false;
  }
  if (bufoverrun2) {
    Serial.println("<Warning> Serial buffer over-run on UART2");
    bufoverrun2 = false;
  }
  if (bufoverrun3) {
    Serial.println("<Warning> Serial buffer over-run on UART3");
    bufoverrun3 = false;
  }

}

// Log a message string to the SD card log file.
boolean logMsgStr(String filename, String message) {
  boolean logError = false;
  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {
    dataFile.print(message);
    dataFile.close();
  }
  else {
    Serial.println("Error opening data log file");
    logError = true;
  }
  return logError;
}

// Function transmits a UDP message to WiFi using the esp8266
void SendUDPMessage(String message) {
  Serial1.println("AT+CIPSEND=4,"+String(message.length()+2));
  delayMicroseconds(4000);
  Serial1.println(message);
}

// Function returns true if the esp8266 transmits an 'OK' or false if it times out.
// A state machine is used, which allows for an unlimited esp8266 response length within
// the timeout interval, currently set to 6000 ms.
boolean esp8266WaitOK(){
  boolean esp8266Connected = false;
  byte state = 0;
  unsigned long msSinceStartup;
  unsigned long msPolling;

  msSinceStartup = millis();  // note the system tick in ms.  Rolls over in 50 days!
  do {
    if (Serial1.available()) {
      int inByte = Serial1.read();
      Serial.write(inByte); // echo
      switch (state) {
        case 0:
          if (char(inByte) == 'O')
            state = 1;
          else
            state = 0;
          break;
        case 1:
          if (char(inByte) == 'K')
            state = 2;
          else
            state = 0;
          break;
        case 2:
          state = 0;
          esp8266Connected = true;
          break;
      }
    }
    msPolling = millis() - msSinceStartup;
  } while(!esp8266Connected && msPolling < 6000);
  return esp8266Connected;
}


