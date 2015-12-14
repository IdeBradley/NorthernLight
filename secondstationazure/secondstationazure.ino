// Code Update Notes
// used original code for BMP180, modified sept1 to accommodate the Arduino Mega and connections....
// contains code for BMP180 - pressure/temp.  magnetometer and wireless chip 
// loads to thingspeak and creates a JSON file for SQL Azure
// second station upload
//  to do ( check error correction for Gauss, add temp coefficient ) 



// references to required libraries - reference program files/Arduino/libraries
#include <TimerOne.h> // sets internalclock timer
#include <stdio.h> // standard input and output
#include <Wire.h> //I2C Arduino Library
#include <SFE_BMP180.h> //  BMP180 library
#include <Time.h>   // needed for timestamp for JSON string
#include <Adafruit_Sensor.h>   // magnetometer library
#include <Adafruit_HMC5883_U.h> // magnetometer library


//  set up definitions
#define DEBUG true
#define INTERVAL 30 // time between readings in seconds
#define LEDPIN 13
#define WIFISSID "#############"
#define WIFIPASS "#############"
#define MONITORBAUD 9600
#define ESPBAUD 115200
//#define THINGSPEAKIP "184.106.153.149"
#define THINGSPEAKIP "api.thingspeak.com"
#define THINGSPEAKPORT "80"
#define THINGSPEAKWRITEKEY "1XJN1ZWJLF1Y4MA0"
#define AZUREIP " "
#define AZUREPORT "80"
#define AZUREWRITEKEY " "
#define TIMESERVERIP "159.134.168.46"
#define address 0x1E //0011110b, I2C 7bit address of HMC5883


Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(00002);

SFE_BMP180 pressure;
int timerCnt = 0;
int sensorPin = A0;   

 void setup() {
    if(!mag.begin()) {
        if( DEBUG ) {
              Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
        }
        while(1);
    }
    pinMode(LEDPIN, OUTPUT);
    digitalWrite(LEDPIN, 0);
    setupBMP180();
    if( DEBUG ) {
        Serial.begin(MONITORBAUD);
    }
    Serial1.begin(ESPBAUD);
    resetESP(); 
    Timer1.initialize(1000000); 
    Timer1.attachInterrupt( timerIsr ); 
}
 
 
 
void loop() {
    if( analogRead(sensorPin) > 500 ) {
        digitalWrite(13, 1);
    } else {
        digitalWrite(13, 0);  
    }
    delay(150);
}
 
void timerIsr() {
    interrupts();    
    timerCnt++;
    if( timerCnt > INTERVAL ){
        timerCnt = 0;
        makeThingspeakReading();
    //makeAzureReading();
    }
}

void makeThingspeakReading() {
    // call subroutines to get the data
    double T= 0,P = 0;
    int x,y,z;  // change for Gauss
    String stationID = "court1";
    
    T = getTemp();
    P = getPx( T );
    
    sensors_event_t event; 
    mag.getEvent(&event);
    double xmag = event.magnetic.x; 
    double ymag = event.magnetic.y;
    double zmag = event.magnetic.z;
     
    
    String cipstart = "AT+CIPSTART=\"TCP\",\"";
    cipstart += THINGSPEAKIP;
    cipstart += "\",";
    cipstart += THINGSPEAKPORT;
    String reply = sendData(cipstart, "OK", 1000 ); 
    if( reply.indexOf( "CONNECT" ) <= -1 ) {
        resetESP();
        if( DEBUG ) {
            Serial.println( "Reset!" );
        }
        cipstart = "AT+CIPSTART=\"TCP\",\"";
        cipstart += THINGSPEAKIP;
        cipstart += "\",";
        cipstart += THINGSPEAKPORT;
        String reply = sendData(cipstart, "OK", 1000 );
    }
//    String cmd = "GET /update?key=";
//    cmd += THINGSPEAKWRITEKEY;
//    cmd += "&field1=";
//    cmd += T;
//    cmd += "&field2=";
//    cmd += P;
//    cmd += "\r\n";

// ** add magnetometer fields
    //build data string for Thingspeak out of temperature, pressure, x,y,z
    String data = "field1=";
    data += T;
    data += "&field2=";
    data += P;
    data += "&field3=";
    data += xmag;
    data += "&field4=";
    data += ymag;
    data += "&field5=";
    data += zmag;
    data += "&field6=";
    data += 02;
    data += "\r\n";
    Serial.print("Debug1:  ");
    Serial.println(data );
     
    String key = "X-THINGSPEAKAPIKEY: ";
    key += THINGSPEAKWRITEKEY;
    key += "\r\n";
    
    String length = "Content-Length: ";
    length += data.length();
    length += "\r\n";
    String cmd = "POST /update HTTP/1.1\r\n";
    cmd += "Host: api.thingspeak.com\r\n";
    cmd += "Connection: close\r\n";
    cmd += key;
    cmd += "Content-Type: application/x-www-form-urlencoded\r\n";
    cmd += length;
    cmd += "\r\n";
    cmd += data;
    httpWrite( cmd, "CLOSED" );
    
     //  Get initial webTimeStamp. The Arduino has no time keeping facility so needs to get data from somewhere
    cipstart = "AT+CIPSTART=\"TCP\",\"";
    cipstart += TIMESERVERIP;
    cipstart += "\",";
    cipstart += THINGSPEAKPORT;
    reply = sendData(cipstart, "OK", 1000 ); 
    cmd = "GET /index.html HTTP/1.1\r\n Host: www.google.com\r\n\r\n\r\n";  //construct http GET request
    String cmd1 = "AT+CIPSEND=";
    cmd1 += cmd.length();
    sendData( cmd1, ">", 2000 ); 
    //sendData( cmd, "OK", 2000 ); 
    String webTimeStamp = sendTimeData( cmd, 10000 ); 
    formatTime(webTimeStamp);
    
    String test1 = writeJsonString(stationID,P, T,x,y,z);
    Serial.print("Debug2:  " );
    Serial.println(test1);
}
void makeAzureReading() {
    // call subroutines to get the data
    double T= 0,P = 0;
    int x,y,z;  // change for Gauss
    String stationID = "court1";
    
    T = getTemp();
    P = getPx( T );
    
    sensors_event_t event; 
    mag.getEvent(&event);
    double xmag = event.magnetic.x; 
    double ymag = event.magnetic.y;
    double zmag = event.magnetic.z;
     
    
    String cipstart = "AT+CIPSTART=\"TCP\",\"";
    cipstart += AZUREIP;
    cipstart += "\",";
    cipstart += AZUREPORT;
    String reply = sendData(cipstart, "OK", 1000 ); 
    if( reply.indexOf( "CONNECT" ) <= -1 ) {
        resetESP();
        if( DEBUG ) {
            Serial.println( "Reset!" );
        }
        cipstart = "AT+CIPSTART=\"TCP\",\"";
        cipstart += AZUREIP;
        cipstart += "\",";
        cipstart += AZUREPORT;
        String reply = sendData(cipstart, "OK", 1000 );
    }


// ** add magnetometer fields
    //build data string for Thingspeak out of temperature, pressure, x,y,z
    String data = "field1=";
    data += T;
    data += "&field2=";
    data += P;
    data += "&field3=";
    data += xmag;
    data += "&field4=";
    data += ymag;
    data += "&field5=";
    data += zmag;
    data += "&field6=";
    data += 02;
    data += "\r\n";
    Serial.print("Debug1:  ");
    Serial.println(data );
     
    String key = "X-THINGSPEAKAPIKEY: ";
    key += THINGSPEAKWRITEKEY;
    key += "\r\n";
    
    String length = "Content-Length: ";
    length += data.length();
    length += "\r\n";
    String cmd = "POST /tables/weather/ HTTP/1.1\r\n";
    cmd += "Host: api.thingspeak.com\r\n";
    cmd += "Connection: close\r\n";
    cmd += key;
    cmd += "Content-Type: application/x-www-form-urlencoded\r\n";
    cmd += length;
    cmd += "\r\n";
    cmd += data;
    httpWrite( cmd, "CLOSED" );
    
     //  Get initial webTimeStamp. The Arduino has no time keeping facility so needs to get data from somewhere
    cipstart = "AT+CIPSTART=\"TCP\",\"";
    cipstart += TIMESERVERIP;
    cipstart += "\",";
    cipstart += THINGSPEAKPORT;
    reply = sendData(cipstart, "OK", 1000 ); 
    cmd = "GET /index.html HTTP/1.1\r\n Host: www.google.com\r\n\r\n\r\n";  //construct http GET request
    String cmd1 = "AT+CIPSEND=";
    cmd1 += cmd.length();
    sendData( cmd1, ">", 2000 ); 
    //sendData( cmd, "OK", 2000 ); 
    String webTimeStamp = sendTimeData( cmd, 10000 ); 
    formatTime(webTimeStamp);
    
    String test1 = writeJsonString(stationID,P, T,x,y,z);
    Serial.print("Debug2:  " );
    Serial.println(test1);
}

void resetESP() {
    sendData( "AT+RST","Ai-Thinker Technology Co. Ltd", 10000 );
    sendData( "AT+CWMODE=3", "OK", 10000 );
    String cwjap = "AT+CWJAP=\"";
    cwjap += WIFISSID;
    cwjap += "\",\"";
    cwjap += WIFIPASS;
    cwjap += "\"";
    sendData( cwjap, "OK", 10000 ); 
}

void httpWrite( String cmd, String res ){
    String cmd1 = "AT+CIPSEND=";
    cmd1 += cmd.length();
    sendData( cmd1, ">", 10000 ); 
    sendData( cmd, res, 10000 ); 
}
    
String sendData( String command, String res, const int timeout ) {
    String response = "";
    Serial1.print( command ); 
    Serial1.print( "\r\n" ); 
    long int time = millis();
    while( (time + timeout) > millis() && response.indexOf( res ) < 0 ) {
        while(Serial1.available()) {   
            char c = Serial1.read(); 
            response+=c;
        }  
    }
    if( DEBUG ) {
        Serial.print( response );
    }
    return response;
}

String sendTimeData(String command, const int timeout) {
    // get time from a current server to use in setting up Arduino clock
    String response = "";
    Serial1.print(command); 
    Serial1.print("\r\n"); 
    long int time = millis();
    while( (time+timeout) > millis()&& response.indexOf("Server") < 0 ) {
        while(Serial1.available()) {   
            char c = Serial1.read(); 
            response+=c;
        }  
    }
    response =response.substring(response.indexOf("Date")+11,response.indexOf("Server") );
    
    if(DEBUG) {
        Serial.print(response);
    }
    return response;
}

//
void formatTime(String webTimeStamp)
{
  
    int curDay = (webTimeStamp.substring(0,2)).toInt();
    int curMon =10;
    int curYear = (webTimeStamp.substring(7,11)).toInt();
    int curHour = (webTimeStamp.substring(12,14)).toInt();
    int curMin= (webTimeStamp.substring(15,17)).toInt();
    int curSec = (webTimeStamp.substring(18,20)).toInt();
   setTime(curHour,curMin,curSec,curDay,curMon,curYear); 
}

//String sendData(String command, String term, const int timeout) {
//    String response = "";
//    Serial1.print(command); 
//    Serial1.print("\r\n"); 
//    long int time = millis();
//    while( (time+timeout) > millis() && response.indexOf( term ) < 0 ) {
//        while(Serial1.available()) {   
//            char c = Serial1.read(); 
//            response+=c;
//        }  
//    }
//    if(DEBUG) {
//        Serial.print(response);
//    }
//    return response;
//}

//  DataRow readout 
  String writeJsonString(String stationID, float p, float t, float xmag, float ymag, float zmag) 
  {
  // Create a simple JSON;
       String datarow = "{\"StationID\":";
       datarow += stationID ;
    // need a unique ID for each one ; Primary Key is stationID and Timestamp concatenated
       datarow += ",\"TimeStamp\":";
       datarow += now();
       datarow += ",\"Pressure\":";
       datarow += p;
       datarow += ",\"Temperature\":";
       datarow += t;
       datarow += ",\"X-Axis\":";
       datarow += xmag;
       datarow += ",\"Y-Axis\":";
       datarow += ymag;
       datarow += ",\"Z-Axis\":";
       datarow += zmag;
       datarow += "}";
       Serial1.println(datarow);
       return (datarow);
   }



//   Procedures for pressure, temperature
 
  

void setupHMC5883L() {
  Wire.begin();
  Wire.beginTransmission( address );
  Wire.write( 0x02 );
  Wire.write( 0x00 );
  Wire.endTransmission();
}

void setupBMP180() {
  if (!pressure.begin())  {
    Serial.println("BMP180 init fail\n\n");
    while (1); // Pause forever.
  }
  Serial.println( "Pressure on" );
}



double getPx( double T ) {
  char status;
  double P = 0;



  status = pressure.startPressure( 3 );
  if ( status != 0 ) {
    delay( status );
    status = pressure.getPressure( P, T );
  }
  return P;
}

double getTemp() {
  char status;
  double T = 0;
  status = pressure.startTemperature();
  if ( status != 0 ) {
    delay(status);
    pressure.getTemperature( T );
  }
  return T;
}

/// edit...
 
 







