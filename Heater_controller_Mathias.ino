

#define WEBDUINO_AUTH_REALM "Weduino Authentication Example"
#include <Time.h>
#include <EEPROM.h>
#include <SPI.h> 
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "WebServer.h"
//#include <SoftwareSerial.h>




const int LCDdelay=2;  // conservative, 2 actually works


// no-cost stream operator as described at 
// http://sundial.org/arduino/?page_id=119
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ 
  obj.print(arg); 
  return obj; 
}

// dallas temp sensor

#include <OneWire.h>
#include <DallasTemperature.h>
// Data wire is plugged into port 53 on the Arduino
#define ONE_WIRE_BUS 27
#define TEMPERATURE_PRECISION 9
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// arrays to hold device addresses
DeviceAddress sens1 = { 
  0x28, 0x5E, 0xF7, 0xAA, 0x03, 0x00, 0x00, 0x82 };
DeviceAddress sens2 = { 
  0x28, 0xA9, 0xD4, 0xAA, 0x03, 0x00, 0x00, 0xC3 };
DeviceAddress sens3 = { 
  0x28, 0xE3, 0xE8, 0xAA, 0x03, 0x00, 0x00, 0xDC };





// variables for heating
boolean epromRefresh=false; // only refresh eeprom if something has changed
int Heizung = 0; // heating on or off
float tempIst = 0; // actual roomtemp
int tempSoll = 0; // required roomtemp
int tempTag = 0; 
int tempNacht = 0;
int tempFrost = 0;
int modus = 3; // modes 0=auto, 1=day, 2=night, 3=antifreeze
int tempMin = 3;
int tempMax = 30;
int hyst = 1;
unsigned long modusLastUpdate = 0;
unsigned int modusResetTime = 0; //after given hours heating control automatically returns to modus "auto"

unsigned long heatingLastControl = 0; // keeps track when heating control was last executed

int heatingControlInterval = 300;

int tempMeasureInterval = 30;


int currentHour;
int nextHour;
int nextHourState;
int prevHour;
int prevHourState;


float t1;
float t2;
float t3;


//variables for calendar
int timeTable[168];
int timeTableNew[168];



//other variables
int c;

boolean lastKeyStateTag  = false;
boolean lastKeyStateNacht  = false;

float tempIstNew = 0;
float tempIstOld = 0;
unsigned long tempLastUpdate = 0;



//output pins

int ofenEin = 30;
int ofenAbsenkung = 32;

int ledAuto = 3;
int ledTag = 5;
int ledNacht = 9;
int ledFrost = 11;

int btnAuto = 43;
int btnTag = 41;
int btnNacht = 39;
int btnFrost = 37;




// variables for NTP

int sommerzeit = 1;

IPAddress timeServer(192, 53, 103, 108);

/* Set this to the offset (in seconds) to your local time 
 */
long timeZoneOffset = 7200;  

/* Syncs to NTP server every 15 seconds for testing,
 set to 1 hour or more to be reasonable */
unsigned int ntpSyncTime = 7200;        


/* ALTER THESE VARIABLES AT YOUR OWN RISK */
// local port to listen for UDP packets
unsigned int localPort = 8888;
// NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE= 48;      
// Buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];  
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;                    
// Keeps track of how long ago we updated the NTP server
unsigned long ntpLastUpdate = 0;    
// Check last time clock displayed (Not in Production)
time_t prevDisplay = 0; 





//variables for network config 
static uint8_t mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x7E, 0x1B };
static uint8_t ip[] = { 
  10, 0, 0, 25 };
static uint8_t gateway[]  = { 
  10, 0, 0, 138 };   

#define PREFIX ""
WebServer webserver(PREFIX, 80);


void hzCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{

  if (server.checkCredentials("dXNlcjp1c2Vy")) //login: user, pass: user
  {   
    if (type == WebServer::POST)

    {
      bool repeat;
      char name[2], value[4];
      do
      {
        /* readPOSTparam returns false when there are no more parameters
         * to read from the input.  We pass in buffers for it to store
         * the name and value strings along with the length of those
         * buffers. */
        repeat = server.readPOSTparam(name, 2, value, 4);   
        /* this is a standard string comparison function.  It returns 0
         * when there's an exact match.*/
        if (strcmp(name, "x") == 0)
        {
                        /* use the STRing TO Unsigned Long function to turn the string
           	        * version of the number into our integer  */
          int i = strtoul(value, NULL, 10);
          timeTableNew[i] = 1;       
        }


        if (strcmp(name, "t") == 0) { 
          int i = strtoul(value, NULL, 10); // degrees                
          tempTag = (min(tempMax, (max (i, tempMin))));   
        }

        if (strcmp(name, "n") == 0) { 
          int i = strtoul(value, NULL, 10); // degrees                
          tempNacht = (min(tempMax, (max(i, tempMin)))); 
        }

        if (strcmp(name, "f") == 0) { 
          int i = strtoul(value, NULL, 10); // degrees                
          tempFrost = (min(tempMax, (max(i, tempMin))));
        }

        if (strcmp(name, "m") == 0) {  
          int i = strtoul(value, NULL, 10);    // modus number
          modus = i;
          modusLastUpdate = now();
          if (modus == 0) modusResetTime = 0;
          if (modus == 1) modusResetTime = 24;
          if (modus == 2) modusResetTime = 24;
          if (modus == 3) modusResetTime = 0;
        }


        if (strcmp(name, "s") == 0) {  // daylight saving off
          int i = strtoul(value, NULL, 10);
          if (sommerzeit != i) {
          sommerzeit = i;
           //Try to get the date and time

            if (sommerzeit == 1) {
              timeZoneOffset = 7200;
            }
            if (sommerzeit == 0) {
              timeZoneOffset = 3600;
            }


            int trys=0;
            while(!getTimeAndDate() && trys<10) {
              trys++;
            }
          
          
          }

        }



      } 
      while (repeat);


      for (int i=0; i < 169; i++) { // copy everything to timeTable
        timeTable[i] = timeTableNew[i];
      }
      for (int i=0; i < 169; i++) { // delete everything in timeTableNew 
        timeTableNew[i] = 0;
      }

      epromRefresh = true;
      // after procesing the POST data, tell the web browser to reload
      // the page using a GET method
      server.httpSeeOther(PREFIX);
      return;

    }

  }


  else
  {
    /* send a 401 error back causing the web browser to prompt the user for credentials */
    server.httpUnauthorized();
    return;
  }
 

  /* for a GET or HEAD, send the standard "it's all OK headers" */
  server.httpSuccess();

  /* we don't output the body for a HEAD request */
  if (type == WebServer::GET)
  {





    //Statustabelle       

    server << "<html><head><title>Heizungssteuerung</title><style type='text/css'></style></head><body><table border='1'><tbody><tr><td width='120' align='center'><font color='red'>Temp SOLL</font></td>";
    server << "<td width='115' align='center'>Temp IST</td><td width='120' align='center'>Datum</td><td width='120' align='center'>Zeit</td><td width='120' align='center'>Wochentag</td><td width='90' align='center'>Status</td></tr>";
    server << "<tr><td align='center'><font color='red'>";
    server << tempSoll;
    server << "</font></td><td align='center'>";
    server << tempIst;
    server << "</td><td align='center'>";
    server << day();
    server << " / ";
    server << month();
    server << " / ";
    server << year();
    server << "</td><td align='center'>";
    server << hour();
    server << ":";

    if (minute() < 10)
      server << "0";

    server <<minute();
    server << ":";

    if (second() < 10)
      server << "0";

    server << second();
    server << "</td><td align='center'>"; 
    switch (weekday()) {
    case 1:
      server << "SO";
      break;
    case 2:
      server << "MO";
      break;
    case 3:
      server << "DI";
      break;
    case 4:
      server << "MI";
      break;
    case 5:
      server << "DO";
      break;
    case 6:
      server << "FR";
      break;
    case 7:
      server << "SA";
      break;
    }

    server << "</td><td align='center'> ";
    if (Heizung == 0) {
      server << "AUS";
    }
    if (Heizung == 1) {
      server << "MIN";
    }
    if (Heizung == 2) {
      server << "EIN";
    } 
    server << "</td></tr></tbody></table>";     
    server << "<br>";


    //Misc


    server << "<br> t1="; //print individual tempsensors for debugging
    server << t1;
    server << "<br> t2=";
    server << t2;
    server << "<br> t3=";
    server << t3;
    server << "<br>";

    server << "<form METHOD=POST><p>Modus:</p><p><input type=radio name=m value=0 ";

    if (modus == 0) {
      server <<"checked=checked";
    }

    server <<"> Auto <br><input type=radio name=m value=1 ";        


    if (modus == 1) {
      server <<"checked=checked";
    }

    server <<"> Tag <br><input type=radio name=m value=2 ";

    if (modus == 2) {
      server <<"checked=checked";
    }


    server <<"> Nacht <br><input type=radio name=m value=3 ";

    if (modus == 3) {
      server <<"checked=checked";
    }

    server <<"> Frostschutz</p>";



    if (modusResetTime > 0) {
      server << "Handbetrieb: ";
      server << modusResetTime;
      server << "h uebrig!";
      server << "<br><br>";
    }
    server << "<br>";
    server << "<table border=1>";
    server << "<tr><td>Temp Tag</td><td>Temp Nacht</td><td>Temp Frostschutz</td></tr>";
    server << "<tr><td><input type='text' name='t' value=";
    server << tempTag;
    server <<" size='2' maxlength='2'/></td><td><input type='text' name='n' value=";
    server << tempNacht;
    server <<" size='2' maxlength='2'/></td><td><input type='text' name='f' value=";
    server << tempFrost;
    server <<" size='2' maxlength='2'/></td></tr>";
    server << "</table>";
    server << "</br>";



    server <<"<input type=radio name=s value=0 ";

    if (sommerzeit == 0) {
      server <<"checked=checked";
    }

    server <<"> Winterzeit <br><input type=radio name=s value=1 ";        


    if (sommerzeit == 1) {
      server <<"checked=checked";
    }

    server <<"> Sommerzeit <br>";
    server << "<br>";

    //Time Checkboxes


    server << "<table border=1>";
    server <<"<td></td>";
    
    
    for (int k = 0; k < 24; k++) { //print header of table with hours
      server <<"<td";
      
      
      if (hour() ==  k) {
        server <<" bgcolor=00FF99";
      }


      server <<">";
      server <<k;
      server <<"</td>";
    }


    for (int j = 0; j < 7; j++) {
      switch (j) {
      case 0:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Sonntag</td>";
        break;
      case 1:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Montag</td>";
        break;
      case 2:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Dienstag</td>";
        break;
      case 3:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Mittwoch</td>";
        break;
      case 4:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Donnerstag</td>";
        break;
      case 5:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Freitag</td>";
        break;
      case 6:
        server <<"<tr><td";
        if (weekday() == j+1) {
          server <<" bgcolor=00FF99";
        }
        server <<">Samstag</td>";
      }
      

      for (int k = 0; k < 24; k++) {
        server <<"<td>";
        server <<"<input type=checkbox ";
        server <<"value=";
        server <<24 * j + k;
        server <<" name=x";


        if (timeTable[24 * j + k] == 1) {
          server <<" checked=checked";
        }

        server << "></td>";

      }
      

      server << "</tr>";

    }
    server << "</table>";
    server << "<input type=submit value=Senden>";
    server << "</form>";

    //html footer   
    server << "</html>";

    delay(10);


  }
}







//main loop

void setup(){

  
  Serial.begin(9600);


  Serial1.begin(9600);
  clearLCD();
  backlightOn();

  Serial2.begin(2400);
  Serial2.write(0x77); // Clear all decimal points
  Serial2.write((uint8_t)0x00); //write a 0x00 (without uint8_t an error is thrown)
  Serial2.write(0x7A); // Brightness 00 is brightest
  Serial2.write((uint8_t)0x00);
  Serial2.print("v"); // Reset current display
  Serial2.write(0x77);
  Serial2.write(0x22);

  pinMode(ofenEin, OUTPUT); // high to turn oven on
  pinMode(ofenAbsenkung, OUTPUT); // high to throttle power

  pinMode(ledAuto, OUTPUT); // leds that indicate the mode
  pinMode(ledTag, OUTPUT);
  pinMode(ledNacht, OUTPUT);
  pinMode(ledFrost, OUTPUT);


  pinMode(btnAuto, INPUT); // buttons to change the mode
  pinMode(btnTag, INPUT);
  pinMode(btnNacht, INPUT);
  pinMode(btnFrost, INPUT);



  // Start up the library
  sensors.begin();

  sensors.setResolution(sens1, TEMPERATURE_PRECISION);
  sensors.setResolution(sens2, TEMPERATURE_PRECISION);
  sensors.setResolution(sens3, TEMPERATURE_PRECISION);








  Ethernet.begin(mac, ip, gateway, gateway); // nur wenn dhcp inaktiv


  /* register our default command (activated with the request of
   * http://x.x.x.x */
  webserver.setDefaultCommand(&hzCmd);

  /* start the server to wait for connections */
  webserver.begin();








  for (int i=0; i < 168; i++) { // read data from eprom
    timeTable[i] = EEPROM.read(i);
  }

  tempTag = EEPROM.read(168);

  tempNacht = EEPROM.read(169);

  tempFrost = EEPROM.read(170);

  modus = EEPROM.read(171);

  sommerzeit = EEPROM.read(172);



  ////////////////////////////////////////////////////////////

  //Try to get the date and time

  if (sommerzeit == 1) {
    timeZoneOffset = 7200;
  }
  if (sommerzeit == 0) {
    timeZoneOffset = 3600;
  }


  int trys=0;
  while(!getTimeAndDate() && trys<10) {
    trys++;
  }

  ////////////////////////////////////////////////////////////  

readTempSensors();

heatingLastControl = now();

}

void loop() {

  if (sommerzeit == 1) {
    timeZoneOffset = 7200;
  }
  if (sommerzeit == 0) {
    timeZoneOffset = 3600;
  }

  char buff[64];
  int len = 64;
  // process incoming connections one at a time forever
  webserver.processConnection(buff, &len);

  writeEprom();        
  updateTemp();
  
  if (now() >= heatingLastControl + heatingControlInterval) controlHeating();
  
  updateTime();
  readTempSensors();
  resetModus();
  userInterface();
  updateLcd();


  int segTemp = int(tempIst*10);
  if (segTemp >= 0) { // sparkfun serial enabled 7 segment display cannot handle negative numbers
  if(segTemp < 100) Serial2.print("0"); // without trailing 0 7segment display will display false data
  if(segTemp < 10) Serial2.print("0");
  //Serial.println(segTemp);
  Serial2.print(segTemp);
  Serial2.print("C");
  }
  else {
  Serial2.print("Errr");
  }

}





void updateLcd(){


  lcdPosition(0,0);

  switch (weekday()) {
  case 1:
    Serial1.print("So,");
    break;
  case 2:
    Serial1.print("Mo,");
    break;
  case 3:
    Serial1.print("Di,");
    break;
  case 4:
    Serial1.print("Mi,");
    break;
  case 5:
    Serial1.print("Do,");
    break;
  case 6:
    Serial1.print("Fr,");
    break;
  case 7:
    Serial1.print("Sa,");
    break;
  }

  if (day() < 10) Serial1.print("0");
  Serial1.print(day());
  Serial1.print(".");
  if (month() < 10) Serial1.print("0");
  Serial1.print(month());
  Serial1.print(".");
  Serial1.print(year());

  lcdPosition(1,0);

  if (hour() < 10) Serial1.print("0");
  Serial1.print(hour());
  Serial1.print(":");
  if (minute() < 10) Serial1.print("0");
  Serial1.print(minute());
  Serial1.print(" ");
  
  lcdPosition(1,6);


  if (modusResetTime > 0) {
    Serial1.print("H:");
    if (modusResetTime < 10) Serial1.print (" ");
    Serial1.print (modusResetTime);
  }
  else {
    Serial1.print ("     "); ///overwrite
  }

  lcdPosition(1,12);
  if (tempSoll < 10) Serial1.print(" ");
  Serial1.print(tempSoll);
  Serial1.print("*C");

}







void userInterface(){



  if (digitalRead(btnAuto) == HIGH) {

    modusResetTime = 0;
    modus = 0;
  }

  if (digitalRead(btnTag) == HIGH){

    if (modus != 1) modusResetTime = 0;
    modus = 1;
    modusLastUpdate = now();
    if (lastKeyStateTag == false) {

      if (modusResetTime < 24) {
        modusResetTime ++;     
      }
      else modusResetTime = 0;
      lastKeyStateTag = true;
    }



  } 

  if (digitalRead(btnTag) == LOW) lastKeyStateTag = false;

  if (digitalRead(btnNacht) == HIGH) {
    if (modus != 2) modusResetTime = 0;
    modus = 2;
    modusLastUpdate = now();
    if (lastKeyStateNacht == false) {
      if (modusResetTime < 24) {
        modusResetTime ++;     
      }
      else modusResetTime = 0;
      lastKeyStateNacht = true;
    }



  } 

  if (digitalRead(btnNacht) == LOW) lastKeyStateNacht = false;

  if (digitalRead(btnFrost) == HIGH) {
    modusResetTime = 0;
    modus = 3;
  }


  if (modus == 0){
    analogWrite(ledAuto, 255);
    if (timeTable[24 * (weekday() - 1) + hour()] == 1) { //wenn Tagmodus akiv
      analogWrite(ledTag, 5);
      analogWrite(ledNacht, 0);
    }
    else {
      analogWrite(ledTag, 0);
      analogWrite(ledNacht, 1);
    }
    analogWrite(ledFrost, 0);
  }

  if (modus == 1){
    analogWrite(ledAuto, 0);
    analogWrite(ledTag, 255);
    analogWrite(ledNacht, 0);
    analogWrite(ledFrost, 0);
  }


  if (modus == 2){
    analogWrite(ledAuto, 0);
    analogWrite(ledTag, 0);
    analogWrite(ledNacht, 80);
    analogWrite(ledFrost, 0);
  }



  if (modus == 3){
    analogWrite(ledAuto, 0);
    analogWrite(ledTag, 0);
    analogWrite(ledNacht, 0);
    analogWrite(ledFrost, 255);
  }


}





void resetModus(){

  if (modus != 3 && modus != 0)
    if(now() - modusLastUpdate == 3600) {
      modusResetTime --;
      modusLastUpdate = now();
    }

  if (modus != 3 && modus != 0 && modusResetTime <= 0) modus = 0;
}




void readTempSensors(){


  if (now() - tempLastUpdate > tempMeasureInterval) { // read temperature every 1 second
    tempIstOld = tempIstNew;
    sensors.requestTemperatures();
    t1 = sensors.getTempC(sens1);
    t2 = sensors.getTempC(sens2);
    t3 = sensors.getTempC(sens3);
    
    if (t1 > -127 && t2 > -127 && t3 > -127) { //sometimes the sensors deliver invalid data, it's a hardware problem, i'll fix this later
    tempIstNew = ((t1 + t2 + t3)/3); //calculate average roomtemp from three sensors
    tempIst = ((tempIstOld + tempIstNew) / 2) ; //average temperature
    tempLastUpdate = now();
    }
  } 

}







void controlHeating(){

     
     currentHour = 24 * (weekday() - 1) + hour();     
     if (currentHour == 167) nextHour = 0;
     if (currentHour != 167) nextHour = currentHour + 1;     
     nextHourState = (timeTable[nextHour]);
     
     
     if (currentHour == 0) prevHour = 167;
     if (currentHour != 0) prevHour = currentHour - 1;     
     prevHourState = (timeTable[prevHour]);
     
     
  
        if (modus == 0 && nextHourState == 1 && tempIst <= tempSoll - hyst) { //if in auto mode only turn on heating if next hour is still in day mode (e.g. to prevent heating from turning on shortly before night mode)
        Heizung = 2;
        digitalWrite(ofenEin, HIGH);
        digitalWrite(ofenAbsenkung, LOW);
        }
       
      
      if (modus == 0 && prevHourState == 0 && tempIst <= tempSoll) { //heat up room in the first hour)
        Heizung = 2;
        digitalWrite(ofenEin, HIGH);
        digitalWrite(ofenAbsenkung, LOW);
        }
       
      
      
  
      if (timeTable[24 * (weekday() - 1) + hour()] == 0 && tempIst <= tempSoll - hyst) { //if we are in nigth mode
      Heizung = 2;
      digitalWrite(ofenEin, HIGH);
      digitalWrite(ofenAbsenkung, LOW);
        
      }
  
      if (modus != 0 && tempIst <= tempSoll - hyst) {
      Heizung = 2;
      digitalWrite(ofenEin, HIGH);
      digitalWrite(ofenAbsenkung, LOW);
      }
  

    
  
  
  if (tempIst >= tempSoll && Heizung == 2) { // oven will throttle power
  Heizung = 1;
  digitalWrite(ofenAbsenkung, HIGH);
  }
  
  if (tempIst <= tempSoll && Heizung == 1) { // oven will give full power
  Heizung = 2;
  digitalWrite(ofenAbsenkung, LOW);
  }
  
  
 
  if (tempIst >= tempSoll + hyst) {
    Heizung = 0;
  digitalWrite(ofenEin, LOW);
  digitalWrite(ofenAbsenkung, LOW);
  }
  
  
  
  heatingLastControl = now();
  
  
}





////////////////////////////////////////////////
///////////////////////////////////////////////


void updateTime(){
  // Update the time via NTP server as often as the time you set at the top
  if(now()-ntpLastUpdate > ntpSyncTime) {
    int trys=0;
    while(!getTimeAndDate() && trys<10){
      trys++;
    }
    if(trys<10){
      Serial.println("ntp server update success");
    }
    else{
      Serial.println("ntp server update failed");
    }
  }
  // Display the time if it has changed by more than a second.
  if( now() != prevDisplay){
    prevDisplay = now();
    //clockDisplay();  
  }
}


///////////////////////////////////////////////



void updateTemp(){
  switch (modus) {
  case 0: 
    {

      if (timeTable[24 * (weekday() - 1) + hour()] == 1) { //wenn Tagmodus akiv
        tempSoll = tempTag;
      }
      else {
        tempSoll = tempNacht;
      }
      break;

    }
  case 1: 
    {
      tempSoll = tempTag;
      break;
    }
  case 2: 
    {
      tempSoll = tempNacht;
      break;
    }
  case 3: 
    {
      tempSoll = tempFrost;
      break;
    }
  }
}



void writeEprom(){
  if (epromRefresh == true) {


    for (int i = 0; i < 168; i++) { 
      if (timeTable[i] == 1 && EEPROM.read(i) != 1) {
        EEPROM.write(i, 1);
      }
      if (timeTable[i] == 0 && EEPROM.read(i) != 0) {
        EEPROM.write(i, 0);
      }   
    }

    if (tempTag != EEPROM.read(168)) {
      EEPROM.write(168, tempTag);
    }

    if (tempTag != EEPROM.read(169)) {
      EEPROM.write(169, tempNacht);
    }

    if (tempTag != EEPROM.read(170)) {
      EEPROM.write(170, tempFrost);
    }

    if (modus != EEPROM.read(171)) {
      EEPROM.write(171, modus);
    }

    if (sommerzeit != EEPROM.read(172)) {
      EEPROM.write(172, sommerzeit);
    }

  }  
  epromRefresh = false;

}


///////////////////////////////////////////////




// Do not alter this function, it is used by the system
int getTimeAndDate() {
  int flag=0;
  Udp.begin(localPort);
  sendNTPpacket(timeServer);
  delay(1000);
  if (Udp.parsePacket()){
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer
    unsigned long highWord, lowWord, epoch;
    highWord = word(packetBuffer[40], packetBuffer[41]);
    lowWord = word(packetBuffer[42], packetBuffer[43]);  
    epoch = highWord << 16 | lowWord;
    epoch = epoch - 2208988800 + timeZoneOffset;
    flag=1;
    setTime(epoch);
    ntpLastUpdate = now();
  }
  return flag;
}

// Do not alter this function, it is used by the system
unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;                  
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}

// Clock display of the time and date (Basic)
void clockDisplay(){
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

// Utility function for clock display: prints preceding colon and leading 0
void printDigits(int digits){
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}



void lcdPosition(int row, int col) {
  Serial1.write(0xFE);   //command flag
  Serial1.write((col + row*64 + 128));    //position 
  delay(LCDdelay);
}
void clearLCD(){
  Serial1.write(0xFE);   //command flag
  Serial1.write(0x01);   //clear command.
  delay(LCDdelay);
}
void backlightOn() {  //turns on the backlight
  Serial1.write(0x7C);   //command flag for backlight stuff
  Serial1.write(157);    //light level.
  delay(LCDdelay);
}
void backlightOff(){  //turns off the backlight
  Serial1.write(0x7C);   //command flag for backlight stuff
  Serial1.write(128);     //light level for off.
  delay(LCDdelay);
}
void serCommand(){   //a general function to call the command flag for issuing all other commands   
  Serial1.write(0xFE);
}


