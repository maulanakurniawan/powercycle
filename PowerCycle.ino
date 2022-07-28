#include <SoftwareSerial.h>
SoftwareSerial gsmSerial(2, 3);

bool debug = true;
bool gsmRegistered = false;
int relayPin = 4;
int readyPin = 5;
int powerCycleDelay = 5000;
int countLoop = 0;

void setup() {

  if (debug == true) {
    // Begin the serial
    Serial.begin(57600);
  }
  
  // Just initial message
  printToSerialMonitor("[THE POWER CYCLE MODULE - V1.0 DEC 2021]");

  // Set the relay pin as output and set it as high
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  printToSerialMonitor("Relay set");

  // Set the ready pin as output and set it as low
  pinMode(readyPin, OUTPUT);
  digitalWrite(readyPin, LOW);
  printToSerialMonitor("Ready Light Set");

  // Begin serial communication with Arduino and SIM800L
  gsmSerial.begin(57600);
  
  // Listen the serial
  gsmSerial.listen();

  // Wait the serial to be ready
  while(!gsmSerial) {
    delay(1000); 
  }

  // Check for module to be ready
  gsmSerial.println("AT"); 
  printToSerialMonitor(readQuickReplyGsm());

  // Check if GSM registered to network
  checkGsmRegistered();

  // Configure GSM params
  configureGsm();

  // All Ready
  printToSerialMonitor("All Ready");  
}

void loop() {
  updateGsm();
  
  if (countLoop == 10) {
    checkGsmRegistered();
  }

  countLoop++;
  if (countLoop >= 10) {
    countLoop = 0;
  }
}

void updateGsm() {
   
  // Delay for a while
  delay(500);

  // Read serial
  if (debug == true) {
    while (Serial.available() > 0) 
    {
      //Forward what Serial received to Software Serial Port
      gsmSerial.write(Serial.read()); 
    }
  }

  // Read reply
  String serialContent = readQuickReplyGsm();

  // If we have content
  if (serialContent.length() > 0) {

    // Print out to serial
    printToSerialMonitor(serialContent);

    // If we have sms request
    if (serialContent.indexOf("+CMT") >= 0) {
      printToSerialMonitor("Incoming SMS");

      // Get number
      int firstQuote = serialContent.indexOf("\"");
      int secondQuote = serialContent.indexOf("\"", firstQuote + 1);
      String number = serialContent.substring(firstQuote + 1, secondQuote);
      
      // Get text
      int newLine = serialContent.indexOf("\n", 2) + 1;
      String text = serialContent.substring(newLine, serialContent.length());

      // Convert text to uppercase
      text.toUpperCase();
      
      // Response text
      String response;

      // Power cycle
      if (text.indexOf("POWER CYCLE") >= 0) {

        // Set relay to low
        digitalWrite(relayPin, LOW);
        
        // Delay
        delay(powerCycleDelay);
        
        // Set back to high
        digitalWrite(relayPin, HIGH);
        
        // Done
        response = response + "Power has been recycled";
        
        sendSms(number, response);
        printToSerialMonitor(response);

      // Sms list
      } else if (text.indexOf("SMS LIST") >= 0) {
        
        // Send sms delete all command
        gsmSerial.println("AT+CMGL=\"ALL\"");
        response = response + readQuickReplyGsm();

        sendSms(number, response);
        printToSerialMonitor(response);

      // Sms delete
      } else if (text.indexOf("SMS DELETE") >= 0) {
        
        // Send sms delete all command
        gsmSerial.println("AT+CMGD=1,4");
        response = response + readQuickReplyGsm();

        sendSms(number, response);
        printToSerialMonitor(response);

      // Sms memory
      } else if (text.indexOf("SMS MEMORY") >= 0) {

        // Send sms delete all command
        gsmSerial.println("AT+CPMS?");
        response = response + readQuickReplyGsm();
        
        sendSms(number, response);
        printToSerialMonitor(response);
      }
    }
  }
}

bool isGsmRegistered() {
  
  // Check network registration status
  gsmSerial.println("AT+CREG?"); 
  String reply = readQuickReplyGsm();
  printToSerialMonitor(reply);
  
  if (reply.indexOf("+CREG: 0,1") >= 0) {
    return true;
  }

  return false;
}

void checkGsmRegistered() {

  // Once the handshake test is successful, it will back to OK
  gsmRegistered = isGsmRegistered();

  if (gsmRegistered == true) {
    
    // Configure GSM params
    configureGsm();

    // Turn on ready led
    digitalWrite(readyPin, HIGH);

  } else {

    // If not registered, keep checking
    while (gsmRegistered == false) {
      
      // Turn ready light off
      digitalWrite(readyPin, LOW);
      
      printToSerialMonitor("Waiting for GSM to be ready and registered...");
  
      // Keep looking for network
      gsmRegistered = isGsmRegistered();
  
      // If GSM registered
      if (gsmRegistered == true) {
  
        // Configure GSM params
        configureGsm();
    
        // Turn on ready led
        digitalWrite(readyPin, HIGH);
      }
  
      // Delay a bit when searching network
      delay(2000);
    }
  }
}

void configureGsm() {
  
  // Configuring TEXT mode
  gsmSerial.println("AT+CMGF=1"); 
  printToSerialMonitor(readQuickReplyGsm());

  // Decides how newly arrived SMS messages should be handled
  gsmSerial.println("AT+CNMI=1,2,0,0,0"); 
  printToSerialMonitor(readQuickReplyGsm());

  // Set charset to GSM
  gsmSerial.println("AT+CSCS=\"GSM\""); 
  printToSerialMonitor(readQuickReplyGsm());
}

String readQuickReplyGsm() {
  
  // Content 
  String content;

  // Delay
  delay(500);

  // Read everything
  while(gsmSerial.available() > 0) {
    char incoming = gsmSerial.read();
    content += incoming;
  }

  // Replace \r\n with \n only since the m590e always use
  // \r\n as new line, sending sms with this new line
  // will cut off on new line
  content.replace("\r", "");

  return content;
}

void sendSms(String number, String text) {
  
  // Print some to serial
  printToSerialMonitor("Outgoing SMS");

  // Sms mode
  gsmSerial.println("AT+CMGF=1");
  printToSerialMonitor(readQuickReplyGsm());

  // Set the recipient
  gsmSerial.println("AT+CMGS=\"" + number + "\"");
  printToSerialMonitor(readQuickReplyGsm());

  // Set the text
  gsmSerial.println(text);
  printToSerialMonitor(readQuickReplyGsm());

  // Set the end of message flag and send
  gsmSerial.write(26);
  delay(2000);

  printToSerialMonitor(readQuickReplyGsm());
}

void printToSerialMonitor(String text) {
  
  if (Serial && debug == true) {
    Serial.println(text);
  }
}

