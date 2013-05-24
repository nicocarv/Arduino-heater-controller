#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define TIME_HEADER 'T' // Header tag for serial time sync message: Ethernet Time Set
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display


/**********************************************************************************************************************
 *                                   Ethernet Conf.
 ***********************************************************************************************************************/

byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192,168,1, 177};
//IPAddress gateway//(130,192,58, 17);
//(130,192,164, 254);
//IPAddress subnet(255, 255, 255, 0);

const int MAX_PAGENAME_LEN = 10; // max characters in page name 8
char pageName[MAX_PAGENAME_LEN+1]; // additional character for terminating null
EthernetServer server(80);

/**********************************************************************************************************************
 *                                   Other Public variables.
 ***********************************************************************************************************************/
int tempActual=15;
int tempTarget=15;
int tempMin=15;
int TempTable[5][5];
int Block;
int t=0;
boolean EpromRefresh=false;
boolean Riscalda=false;
const int Buffer_length = 90;
char buffer[Buffer_length];

/**********************************************************************************************************************
 *                                                           Main loop
 ***********************************************************************************************************************/


void setup()
{
  //Serial.begin(9600); //if comented, there is no debug possible (to save ram memory)
  lcd.init();
  lcd.backlight(); //turn on light
  LCDinit();
  //Ethernet.begin(mac, ip, gateway, subnet);
  Ethernet.begin(mac, ip);
  server.begin();
  // Serial.print(memoryFree());//Serial.println("Bytes of RAM"); // print the free memory
  // Serial.print(' '); // print a space  
  setTime(14,33,0,11,5,12); //just to start, it can be overwrited later through the explorer

  for  (int i = 0; i < 5; i++) { //get saved info from the EEPROM
    for  (int j = 0; j < 5; j++) {
      TempTable[i][j]=EEPROM.read(i*5+j);
    }
  }
  tempMin=EEPROM.read(28); //random acces but away from the last array
  delay(2000);
  // Serial.println(F("Ready..."));

}

void loop()
{
  EpromRefresh=false; //flag
  LCD();
  tempActual=Thermister(analogRead(2)); //calculate the actual temperature 
  EthernetClient client = server.available();
  EthernetLoop(client); //send pages, update tempTable and set the Alarm.
  ControLoop(); //check temperature and turn ON/Off the heating
  if (EpromRefresh) refreshEEPROM();
  Alarm.delay(1000);
  t++;
}


void refreshEEPROM(){

  for  (int i = 0; i < 5; i++) { 
    for  (int j = 0; j < 5; j++) {
      EEPROM.write(i*5+j, TempTable[i][j]);
    }
  }
}


/******************************************************************************
 * LCD control
 ******************************************************************************/

void LCD(){
  if (t==3){    //refresh every 3 sec
    lcd.clear();
    lcd.print(day());
    lcd.print("/");
    lcd.print(month());
    lcd.print("/");
    lcd.print(year());
    lcd.print(" ");
    lcd.print(hour());
    lcd.print(":");
    if (minute()<10) {
      lcd.print("0");
    }
    lcd.print(minute());
    lcd.setCursor(0, 1);
    lcd.print("TA:");
    lcd.print(tempActual);
    lcd.print("C ");
    lcd.print("TO:");
    lcd.print(tempTarget);
    lcd.print("C ");
    if (Riscalda) {
      lcd.print("ON");
    }
    else {
      lcd.print("OF");
    }
    t=0;
  }
}

void LCDinit(){

  lcd.clear();
  lcd.print(F("Ready"));

}

/******************************************************************************
 * Alarm functions
 ******************************************************************************/

//check that HoraIni:MinIni <= hora:minuto <HoraFin:MinFin
boolean IsIn(int HoraIni, int MinIni, int HoraFin, int MinFin, int hora, int minuto){
  if (hora>HoraIni && hora<HoraFin) {
    return true;
  }
  else if (hora==HoraIni && minuto>=MinIni && hora<HoraFin) {//  it could be that Hora ini = Hora fin
    return true;
  } //if equal the initial one, it belongs to this block, not to the last one
  else if (hora>HoraIni && hora==HoraFin && minuto<MinFin) {  
    return true;
  } //if equal the last, it belong to the next block
  else if (hora==HoraIni && hora==HoraFin && minuto>=MinIni && minuto<MinFin){
    return true;
  }
  else {
    return false;
  }
}

void Alarma(){
  int hora=hour();
  int minuto=minute();
  // Serial.print(F("Hora en func Alarma= "));
  // Serial.print(hora);
  // Serial.print(F(":"));
  // Serial.println(minuto);

  for  (int i = 0; i < 5; i++) { //check in what block are we in order to set the alarm (or between what blocks)

    if (i==0 && IsIn(0,0,TempTable[i][0],TempTable[i][1],hora,minuto)){
      // Serial.print(F("Entre al 1 IF para i= "));
      // Serial.println(i);
      SetearAlarma(0,tempMin,TempTable[i][0], TempTable[i][1]);
      break;
    } //solo para el primero

    else if (IsIn(TempTable[i][0],TempTable[i][1],TempTable[i][2],TempTable[i][3],hora,minuto)){ //dentro del bloque
      // Serial.print(F("Entre al 2 IF para i= "));
      // Serial.println(i);
      SetearAlarma(i+1,TempTable[i][4],TempTable[i][2], TempTable[i][3]);
      break;
    }

    else if (i!=4 && IsIn(TempTable[i][2],TempTable[i][3],TempTable[i+1][0],TempTable[i+1][1],hora,minuto)){  //exeptfor the last one. we are at the right of the block
      // Serial.print(F("Entre al 3 IF para i= "));
      // Serial.println(i);
      SetearAlarma(0,tempMin,TempTable[i+1][0], TempTable[i+1][1]);
      break;
    }

    else if (i==4 && IsIn(TempTable[i][2],TempTable[i][3],23,60,hora,minuto)){//equivalente a 00:00
      // Serial.print(F("Entre al 4 IF para i= "));
      // Serial.println(i);
      SetearAlarma(0,tempMin,0, 0);
      break;
    }

    else {
      // Serial.print(F("NO entre para i= ")); //No es vdd q hay error, simplemente tiene q pasar al diguiente i++ del for
      // Serial.println(i);
    }  
  }
}

void SetearAlarma(int bloque, int temp, int hora, int minuto){
  Block=bloque;
  tempTarget=temp;
  // Serial.print(F("Alarma setead para las: ")); 
  // Serial.print(hora);
  // Serial.print(":");
  // Serial.println(minuto);
  Alarm.alarmOnce(hora, minuto, 0,  Alarma); //siempre con segundo 0
}


/******************************************************************************
 * Control Loop
 ******************************************************************************/


void ControLoop(){//Turn On/Off heating with an interval of 1 degree. If not it will On/Off to fast
  // Serial.print(F(" temp target: "));
  // Serial.print(tempTarget);
  // Serial.print(F(" bloque: "));
  // Serial.println(Block);
  if ( tempActual<tempTarget-0.5 ){
    digitalWrite(9,HIGH);
    Riscalda=true;
    // Serial.println(F("Heating ON"));
  }
  else if (tempActual>tempTarget+0.5 ){
    digitalWrite(9,LOW);
    Riscalda=false;
    // Serial.println(F("Heating OFF"));
  }
  else{
    // Serial.println(F("Heating on last state, we are in the 1 deg range"));
  }

} 

/******************************************************************************
 * Ethernet Loop
 ******************************************************************************/

void EthernetLoop(EthernetClient client){
  if (client) {
    int type = 0;
    while (client.connected()) {
      // Serial.println(F("conectado"));
      if (client.available()) {
        // GET, POST, or HEAD
        memset(pageName,0, sizeof(pageName)); // clear the pageName
        if(client.readBytesUntil('/', pageName,sizeof(pageName))){
          // Serial.println(pageName);
          if(strcmp(pageName,"POST ") == 0){
            char c1;
            char c2;
            char c3;
            // Serial.println(F("entre al Post"));
            //Serial.println(memoryFree());

            c1 = client.read();
            c2 = client.read();
            c3 = client.read();
            while(client.available()){   //buscamos los BLK y luego asignamos los valores
              if (c1=='B' && c2=='L' && c3=='K') {
                int block = client.parseInt(); // the block number
                int index = client.parseInt(); // starting (hour or min) or ending (hour or min) or temp index
                int value= client.parseInt();

                if (block==9 && index==9){
                  tempMin=value;
                  EEPROM.write(28, value);
                }
                else {
                  TempTable[block][index]=value;
                }
                // Serial.print(block);
                // Serial.print(index);
                // Serial.print("= ");
                // Serial.println(value);
              }
              c1=c2;
              c2=c3;
              c3=client.read();
            }
            EpromRefresh=true;
            Alarma(); //Solo si recibe el POST o suena una alarma, debe resetear la alarma
          }
          else if (strcmp(pageName,"GET ") == 0){ //Set the time from a Unix time stamp in the browser Ex: http://192.168.1.177/T1360591980
            char c = client.read() ;
            // Serial.print(c);
            if( c == TIME_HEADER ) {
              time_t t=processSyncMessage(client);  //we set the time from the client!
              //RTC.set(t); // set the RTC Pero en esta version no hay RTC
              setTime(t);
            }
          }          
          SendPage(client);
        }
        break;
      } //del if
    }
    // give the web browser time to receive the data before closing it
    // Serial.println(memoryFree());//Serial.println("Bytes of RAM"); // print the free memory
    for  (int i = 0; i < 5; i++) { //rescatamos las temperaturas de la EEPROM
      for  (int j = 0; j < 5; j++) {
        EEPROM.write(i*5+j, TempTable[i][j]);
      }
    }
    Alarm.delay(1000);
    client.stop();
  }
}


/*****************************************************************************
 * Ethernet Time Set
 *****************************************************************************/
#define TIME_MSG_LEN 11 // time sync to PC is HEADER followed by Unix time_t

time_t processSyncMessage(EthernetClient client) {
  // return the time if a valid sync message is received on the serial port.
  // time message consists of a header and ten ascii digits
  char c;
  time_t pctime = 0;
  for(int i=0; i < TIME_MSG_LEN -1; i++){
    c = client.read();
    if( c >= '0' && c <= '9'){
      pctime = (10 * pctime) + (c - '0') ; // convert digits to a number
    }
  }
  // Serial.println(pctime);
  return pctime;
}


/*****************************************************************************
 * Temperature calculator
 *****************************************************************************/

double Thermister(int RawADC) { //calcula temp per thermistor 10k
  double Temp;
  long multi;
  multi = (long)1024 * (long)10000;
  Temp = log((((multi)/RawADC) - 10000));
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  Temp = Temp - 273.15;            // Convert Kelvin to Celcius
  return Temp;
}

/*****************************************************************************
 * HTML code handling
 *****************************************************************************/

void SendPage(EthernetClient client)
{
  // Serial.println(F("Inicio send page"));
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html\n"));
  client.println(F("<html><head><title>Programmazione Termostato</title><style type='text/css'></style></head><body><h1>Programmazione Termostato</h1><table border='1'><tbody><tr><td width='120' align='center'><font color='red'>Temperatura attuale C</font></td>"));
  client.println(F("<td width='115' align='center'>Temperatura obiettivo C</td><td width='64' align='center'>Data</td><td width='54' align='center'>Ora</td><td width='90' align='center'>Fascia oraria</td><td width='90' align='center'>Stato</td></tr>"));
  FirsTable(client);
  Botones(client);
  client.println(F("<form name='form1' method='post' action=""><table width='465' border='1'><tr><td width='53'>Fascia oraria</td><td width='128' align='center'>Ora inizio</td><td width='128' align='center'>Ora fine</td><td width='128' align='center'>Temperatura obiettivo</td></tr>"));
  SecondTable1(client);
  client.println(F("<input type='submit' value='Aggiornare'></form></body></html>"));
  // Serial.println(F("End send page"));

}

void Botones(EthernetClient client)
{
  client.println(F("<br /><button checked='checked'>Programazione giornaliera</button> <button>Programazione settimanale</button><p>&nbsp;</p>"));
  //client.println(F("<input name='button'value='Programazione giornaliera' checked='checked' /> <input name='button2' value='Programazione settimanale' />"));
    
  }
  
  void FirsTable(EthernetClient client)
  {
  
    printC("<tr><td align='center'><font color='red'>",client);
    printI(tempActual,client);
    printC("</font></td><td align='center'>",client);
    printI(tempTarget,client);
    printC("</td><td align='center'>",client);
    printdate(day(),month(),year(),client);
    printC("</td><td align='center'>",client);
    printime(hour(),minute(),second(),client);
    printC("</td><td align='center'>F",client); 
    printI(Block,client);  
    printC("</td><td align='center'> ",client);
    if (Riscalda) {printC("ON",client);}
    else {printC("OFF",client);}
    printC("</td></tr></tbody></table>",client);     
    printflush(client);
  }  
  
  void SecondTable1(EthernetClient client)
  {
    //we have to send block and value together with their value if not the post goes wrong. That's why I created the function PrintC and printI
    for (int i = 0; i < 5; i++) {
      //  int i=1;
      printC("<tr><td>F",client);
      printI(i+1,client);
      printC("</td><td align='center'>",client);
      printflush(client); //para mandar block y value juntos
  
      printC("<input type='text' name='BLK",client);
      printI(i,client);
      printC("y0'",client);
      printC("value='",client);
      printI(TempTable[i][0],client);
      printC("' size='2' maxlength='2'/>",client);
      printflush(client);
  
      printC(": <input type='text' name='BLK",client);
      printI(i,client);
      printC("y1'",client);
      printC("value='",client);
      printI(TempTable[i][1],client);
      printC("' size='2' maxlength='2'/>",client);
      printflush(client);
  
      client.println(F("</td><td align='center'>"));
  
      printC("<input type='text' name='BLK",client);
      printI(i,client);
      printC("y2'",client);
      printC("value='",client);
      printI(TempTable[i][2],client);
      printC("' size='2' maxlength='2'/>",client);
      printflush(client);
  
      printC(": <input type='text' name='BLK",client);
      printI(i,client);
      printC("y3'",client);
      printC("value='",client);
      printI(TempTable[i][3],client);
      printC("' size='2' maxlength='2'/>",client);
      printflush(client);
  
      client.println(F("</td><td align='center'>"));
  
      printC("<input type='text' name='BLK",client);
      printI(i,client);
      printC("y4' value='",client);
      printI(TempTable[i][4],client);
      printC("' size='2' maxlength='2'/></td></tr>",client);
      
      printflush(client);
    }
    printflush(client); //ahora viene la ultima fila po lokoo! Con colores y weas asi shuper logoo.
  client.println(F("<tr><td bgcolor='#00CCCC'>F0</td><td bgcolor='#00CCCC' align='center'><input type='text' name='BLK5y0' value='00' size='2' maxlength='2' readonly='readonly'/> : <input type='text' name='BLK5y1' value='00' size='2' maxlength='2' readonly='readonly'/></td>"));
  client.println(F("<td bgcolor='#00CCCC' align='center'> <input type='text' name='BLK5y2' value='23' size='2' maxlength='2' readonly='readonly'/> : <input type='text' name='BLK5y4' value='59' size='2' maxlength='2' readonly='readonly'/></td><td bgcolor='#00CCCC' align='center'> <input type='text' name='BLK9y9' value='"));
  client.println(tempMin);
  client.println(F("' size='2' maxlength='2'/></td></tr></table>"));
}


/***********************************************************************
 * Print functions
 ***********************************************************************/

/* The idea of this print functions is to send just one packet and not many packets. it reduce time but you need a bigger buffer in memory  */

void printI(int num, EthernetClient client){
  if (strlen(buffer)<Buffer_length-6)      //we add the int value only if we have enough space!
  {
    sprintf(buffer,"%s%d",buffer,num); //we add the int value to the buffer
  }
  else {
    client.println(buffer); //we send the buffer though the net
    buffer[0]='\0'; //we reset the buffer
    sprintf(buffer,"%s%d",buffer,num);
  }
  //Serial.println(memoryFree());
}

void printC(char info[], EthernetClient client){
  int length_array1=strlen(buffer);
  int length_array2=strlen(info);
  if (Buffer_length-1>length_array1+length_array2) 
  {
    strcat(buffer,info); 
  } 
  else {
    client.println(buffer); 
    buffer[0]='\0';
    strcat(buffer,info);
  }
  //Serial.println(memoryFree());
}

void printime(int h, int m, int s, EthernetClient client) { //size 8 bytes
  if (strlen(buffer)<Buffer_length-9)
  {
    sprintf(buffer,"%s%02d:%02d:%02d",buffer,h,m,s); 
  }
  else {    
    client.println(buffer); 
    buffer[0]='\0';
    sprintf(buffer,"%s%02d:%02d:%02d",buffer,h,m,s);
  }
  //Serial.println(memoryFree());
}

void printdate (int d, int m, int y, EthernetClient client) { //size 10 bytes
  if (strlen(buffer)<Buffer_length-11)
  {
    sprintf(buffer,"%s%02d/%02d/%02d",buffer,d,m,y); 
  }
  else {    
    client.println(buffer); 
    buffer[0]='\0';
    sprintf(buffer,"%s%02d/%02d/%02d",buffer,d,m,y); 
  }
  //Serial.println(memoryFree());
}

void printflush(EthernetClient client){  //we send any information left
  //Serial.println(memoryFree());//Serial.println("Bytes of RAM");
  client.println(buffer);
  buffer[0]='\0';
}



/*************************************************************************
 * variables created by the build process when compiling the sketch
 **************************************************************************/

extern int __bss_end;
extern void *__brkval;

// function to return the amount of free RAM
int memoryFree(){
  int freeValue;
  if((int)__brkval == 0)
    freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else
    freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
}





