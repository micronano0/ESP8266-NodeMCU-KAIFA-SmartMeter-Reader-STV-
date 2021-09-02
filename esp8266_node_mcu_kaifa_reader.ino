
#include "esp8266_obis.h"

#include <Crypto.h>
#include <AES.h>
#include <GCM.h>


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>


ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80


String ip = "(IP unset)";

GCM<AES128> *gcmaes128 = 0;


int receiveBufferIndex = 0;                       // Current position of the receive buffer
const static int receiveBufferSize = 1024;      // Size of the receive buffer
byte receiveBuffer[receiveBufferSize];        // Stores the packet currently being received


byte systitle[8]; // von 11 bis 18
byte ic[4];       // von 23 bis 26
byte iv[12];      // systitle + ic

String keystring =        "";
char char_keystring[] =   "00000000000000000000000000000000";
byte key[16] =  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

byte nachricht1[227];
byte nachricht2[227];                                 // angenommen Block 2 wird so gross wie Block 1

int nachricht1_startbyte = 0;                       // muss 0x68 sein
int nachricht1_endebyte = 0;                      // muss 0xFA sein
int nachricht1_laenge = 0;                      // muss 0x16 sein

int nachricht1_start_offset = 6;
int nachricht2_laenge;

float voltage_l1,voltage_l2,voltage_l3,current_l1,current_l2,current_l3, active_power_plus, active_power_minus, active_energy_plus, active_energy_minus, reactive_energy_plus, reactive_energy_minus;
uint16_t year;
uint8_t month, day, hour, minute, second;
char timestamp[21]; // 0000-00-00T00:00:00Z
char zaehlernummer[20], devicename[20]; 
String werte, fehler;

String Hex8_2_String(uint8_t *data, int length) // prints 8-bit data in hex with leading zeroes
{
  String strg = "";
  char tmp2[16];
  for (int i = 0; i < length; i++) {
    sprintf(tmp2, "%.2X", data[i]);
    strg += tmp2;
  }
  return strg;
}

char convertCharToHex(char ch)
{
  char returnType;
  switch(ch)
  {
    case '0':   returnType = 0;break;
    case '1':   returnType = 1;break;
    case '2':   returnType = 2;break;
    case '3':   returnType = 3;break;
    case '4':   returnType = 4;break;
    case '5':   returnType = 5;break;
    case '6':   returnType = 6;break;
    case '7':   returnType = 7;break;
    case '8':   returnType = 8;break;
    case '9':   returnType = 9;break;
    case 'A': case 'a':   returnType = 10;break;
    case 'B': case 'b':   returnType = 11;break;
    case 'C': case 'c':   returnType = 12;break;
    case 'D': case 'd':   returnType = 13;break;
    case 'E': case 'e':   returnType = 14;break;
    case 'F': case 'f':   returnType = 15;break;
    default:    returnType = -1;break;
  }
  return returnType;
}


void abbruch()
{
    receiveBufferIndex = 0;
} 

void PrintHex8(uint8_t *data, int length) // prints 8-bit data in hex with leading zeroes
{
  char tmp[16];
  for (int i = 0; i < length; i++) {
    sprintf(tmp, "0x%.2X", data[i]);
    Serial.print(tmp); Serial.print(" ");
  }
}

uint16_t swap_uint16(uint16_t val) {  return (val << 8) | (val >> 8); }
uint16_t swap_uint4(uint16_t val)  {  return (val << 4) ; }
uint32_t swap_uint32(uint32_t val) {  val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF); return (val << 16) | (val >> 16); }


void setup() {
  Serial.begin(2400, SERIAL_8E1);
  gcmaes128 = new GCM<AES128>();
  
  // ----------------------------------------------------------------
  Serial.setDebugOutput(true);
  Serial.println();

  wifiMulti.addAP("Kaifa", "12345678");
  wifiMulti.addAP("p30", "Passw123");   // add Wi-Fi networks you want to connect to

  Serial.println("Connecting ...");
  int timeout = 0;
  while (wifiMulti.run() != WL_CONNECTED && timeout < 5000) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
    timeout += 500;
  }
  WiFi.mode(WIFI_STA);
  WiFi.hostname("Zaehlerlesen");
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());                 
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());               
  ip = WiFi.localIP().toString().c_str();

  if (MDNS.begin("Zaehlerlesen")) {              
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/", HTTP_GET, handleHome);        
  server.on("/Home", HTTP_GET, handleHome);               
  server.on("/Info", handleInfo);               
  server.on("/KeyForm", handleKeyForm);    
  server.on("/SetKey", HTTP_POST, handleSetKey);               
  server.on("/GetWerte", handleGetWerte);               
  server.onNotFound(handleNotFound);           

  server.begin();                            // Actually start the server
  Serial.println("HTTP server started");
  // ----------------------------------------------------------------
}


String HomeHTML() {
  //  ptr +="";
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Input</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Kundenschnittstelle</h1>\n";
  ptr += "<div style=\"text-align:center\"><h3>Willkommen</h3></div>\n";
  ptr += "</br>\n";
  ptr += "<h4>Auswahl: </h4>\n";
  ptr += "<BLOCKQUOTE><a href=\"GetWerte\">&nbsp;Z&auml;hlerwerte anzeigen</a>\n";
  ptr += "</br></br>\n";
  ptr += "<a href=\"KeyForm\">Key eingeben</a>\n";
  ptr += "</br></br>\n";
  ptr += "<a href=\"Info\">Info</a>\n";
  ptr += "</BLOCKQUOTE></body>\n";
  ptr += "</html>\n";
  return ptr;
}
String InfoHTML() {
  //  ptr +="";
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Input</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Kundenschnittstelle</h1>\n";
  ptr += "<div style=\"text-align:center\"><h3>Info</h3></div>\n";

  ptr += "<strong>Die Kundenschnittstelle</strong>\n";
  ptr += "</br>Die Kundenschnittstelle ist per Default nicht aktiviert. Sie muss vom Stromnetzbetreiber freigeschaltet werden. Man erh&auml;lt nach der kostenlosen Freischaltung einen pers&ouml;nlichen Key per Post zugesandt.\n";
  ptr += "</br>Nachdem eine Verbindung &uuml;ber die Kundenschnittstelle hergestellt worden ist, muss man den Key eingeben. Erst dann werden die Z&auml;hlerdaten entschl&uuml;sselt und k&ouml;nnen angezeigt werden.\n";

  ptr += "<strong></br></br>Der Key</strong>\n";
  ptr += "</br>Der Key ist 32 Zeichen lang (16 Bytes). Alle Zeichen werden hintereinander eingegeben (001122334a5bccda01ff usw.) Wird der Key falsch eingeben kann keine Dekodierung stattfinden und die Seite Z&auml;hler lesen bleibt leer. \n";

  ptr += "</br></br><strong>Z&auml;hlerwerte</strong>\n";
  ptr += "</br>Z&auml;hlerwerte werden durch Klick auf den Link \"Z&auml;hlerwerte lesen\" (nach Eingabe eines g&uuml;ltigen Key's) einmal angezeigt. Um weitere Werte zu erhalten, muss der Link nochmals angeklickt werden.\n";

  ptr += "</br></br><strong>Eine Verbindung herstellen</strong>\n";
  ptr += "</br>Hotspot am Handy einrichten - Einstellungen/Mobilfunknetz/Tethering&mobiler Hotspot/Pers&ouml;nlicher Hotspot/Hotspot konfigurieren\n";
  ptr += "</br>Hotspot-Name: Kaifa\n";
  ptr += "</br>Hotspot-Passwort: 12345678\n";
  ptr += "</br>Unter Pers&ouml;nlicher Hotspot findet man den Punkt \"Verbundene Ger&auml;te\". Hier sollte nun eine 1 stehen. Draufklicken um die IP Adresse abzulesen. Mit dieser IP wird im Browser vom Handy die Webseite aufgemacht.\n";

  ptr += "</br></br><strong>Sicherheit</strong>\n";
  ptr += "</br>Die Kundenschnittstelle ist per Default deaktiviert. Wird sie aktiviert, dann erh&auml;lt der Kunde einen pers&ouml;nlichen Key. Sollte sich ein Unbefugter &uuml;ber den Hotspot zum Z&auml;hler verbinden, dann kann maximal der Key ver&auml;ndert werden.  \n";
  ptr += " Somit k&ouml;nnen, bis zur Eingabe des korrekten Key's keine Daten &uuml;ber die Weboberfl&auml;che gelesen werden. Die Funktion vom Z&auml;hler ist in keinster Weise betroffen.\n";

  
  ptr += "</br></br><strong>Infos zur Hardware</strong>\n";

  ptr += "</br><u>Stromversorgung:</u>\n";
  ptr += "</br>herk&ouml;mmliches USB-Netzteil\n";

  ptr += "</br></br><u>MICRO USB Adapterplatine:</u>\n";
  ptr += "</br>Ein USB-Kabel mit Micro USB Stecker wird hier angesteckt. Die Adapterplatine ist verbunden mit: Step Down Stromversorgungsmodul, Pegelwandler und M-Bus Adapter.\n";

  ptr += "</br></br><u>Step Down Stromversorgungsmodul:</u>\n";
  ptr += "</br>Eingangsspannungen werden durch einem AMS1117 von 4.75V bis 12V auf 3.3V heruntergeregelt, bei einem Stromausgang von max. 0.8A\n";

  ptr += "</br></br><u>Pegelwandler:</u>\n";
  ptr += "</br>Der bidirektionale Logik Pegelwandler wandelt 5V Signale auf 3,3V und 3,3V auf 5V. Er ist daher verbunden mit: ESP8266 und M-Bus Adapter\n";

  ptr += "</br></br><u>Controller:</u>\n";
  ptr += "</br>Es handelt sich hierbei um einen ESP8266 Node-MCU mit WiFi und CP2102 Chip. Die Stromversorgung erfolgt &uuml;ber das Step Down Stromversorgungsmodul. Spannung: 3.3V\n";

  ptr += "</br></br><u>M-Bus Adapter:</u>\n";
  ptr += "</br>Um die Kundenschnittstelle auslesen zu k&ouml;nnen, ben&ouml;tigt man einen M-Bus Adapter. Dieser setzt die 32V Signale um, auf TTL Pegel.\n";
  ptr += "</br>Den Adapter kann man sich entweder selber bauen, oder als fertige Platine im Internet bestellen\n";
  ptr += "</br>Selber bauen: <a href=\"https://pc-projekte.lima-city.de/MBus-Konverter.html\" target=_blank>https://pc-projekte.lima-city.de/MBus-Konverter.html</a>\n";
  ptr += "</br>Fertig bestellen: <a href=\"https://www.mikroe.com/m-bus-slave-click\" target=_blank>https://www.mikroe.com/m-bus-slave-click</a>\n";

  ptr += "</br></br><u>Stecker Kundenschnittstelle:</u>\n";
  ptr += "</br>Man ben&ouml;tigt ein Kabel mit einem RJ12 Stecker.\n";

  ptr += "</br></br><u>Links zum Thema:</u>\n";
  ptr += "</br><a href=\"https://www.tinetz.at/kundenservice/smart-meter/\" target=_blank>Tinetz SmartMeter</a>\n";
  ptr += "</br><a href=\"https://github.com/DomiStyle/esphome-dlms-meter\" target=_blank>Z&auml;hler in HomeAssistant einbinden (DomiStyle)</a>\n";
  ptr += "</br><a href=\"https://www.photovoltaikforum.com/\" target=_blank>photovoltaikforum</a>\n";
  ptr += "</br><a href=\"https://pc-projekte.lima-city.de/MBus-Konverter.html\" target=_blank>M-Bus Adapter Schaltung</a>\n";
  ptr += "</br><a href=\"https://www.mikroe.com/m-bus-slave-click\" target=_blank>M-Bus Adapter kaufen</a>\n";
  ptr += "</br><a href=\"https://diyi0t.com/esp8266-nodemcu-tutorial/\" target=_blank>ESP8266 NodeMCU Tutorial</a>\n";

  ptr += "<BLOCKQUOTE></br></br><a href=\"Home\">Home</a></BLOCKQUOTE>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

String SetKeyHTML() {
  //  ptr +="";
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Input</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Kundenschnittstelle</h1>\n";
  ptr += "<div style=\"text-align:center\"><h3>Key Eingabe</h3></div>\n";
  ptr += "<form action=\"/SetKey\" method=\"POST\">";
  ptr += "</br><input type=\"text\" name=\"kkey\" size=\"35\" maxlength=\"32\" value=\"" + keystring + "\">\n";
  ptr += "</br>\n";
  ptr += "</br><div style=\"text-align:center\"><input type=\"submit\" value=\"&uuml;bernehmen\"></div>\n";
  ptr += "</form>\n";
  ptr += "</br>\n";
  ptr += "<h4>Auswahl: </h4>\n";
  ptr += "<BLOCKQUOTE><a href=\"GetWerte\">&nbsp;Z&auml;hlerwerte anzeigen</a>\n";
  ptr += "</br></br><a href=\"Home\">Home</a>\n";
  ptr += "</BLOCKQUOTE></body>\n";
  ptr += "</html>\n";
  return ptr;
}

String SetKeyOKHTML() {
  //  ptr +="";
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Input</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Kundenschnittstelle</h1>\n";
  ptr += "<div style=\"text-align:center\"><h3>Key erhalten</h3></div>\n";
  ptr += "</br><div style=\"text-align:center\">" + server.arg("kkey") + "</div>\n";
  ptr += "</br>\n";
  ptr += "<h4>Auswahl: </h4>\n";
  ptr += "<BLOCKQUOTE><a href=\"GetWerte\">&nbsp;Z&auml;hlerwerte anzeigen</a>\n";
  ptr += "</br></br><a href=\"Home\">Home</a>\n";
  ptr += "</BLOCKQUOTE></body>\n";
  ptr += "</html>\n";
  return ptr;
}

String ZaehlerDatenHTML() {
  //  ptr +="";
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Input</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Kundenschnittstelle</h1>\n";
  ptr += "<div style=\"text-align:center\"><h3>Z&auml;hlerwerte anzeigen</h3></div>\n";
  ptr += werte;
  ptr += "<h4>Auswahl: </h4>\n";
  ptr += "<BLOCKQUOTE><a href=\"GetWerte\">Z&auml;hlerwerte anzeigen</a>\n";
  ptr += "</br></br><a href=\"Home\">Home</a>\n";
  ptr += "</BLOCKQUOTE></body>\n";
  ptr += "</html>\n";
  return ptr;
}

String ErrorHTML() {
  //  ptr +="";
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Input</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Kundenschnittstelle</h1>\n";
  ptr += "<div style=\"text-align:center\"><h3>ERROR</h3></div>\n";
  ptr += "<div style=\"text-align:center\">" + fehler + "</div>\n";
  ptr += "<h4>Auswahl: </h4>\n";
  ptr += "<BLOCKQUOTE><a href=\"GetWerte\">Z&auml;hlerwerte anzeigen</a>\n";
  ptr += "</br></br><a href=\"Home\">Home</a>\n";
  ptr += "</BLOCKQUOTE></body>\n";
  ptr += "</html>\n";
  return ptr;
}



void handleGetWerte() {  server.send(200, "text/html", ZaehlerDatenHTML()); }
void handleKeyForm()  {  server.send(200, "text/html", SetKeyHTML()); }
void handleHome()     {  server.send(200, "text/html", HomeHTML()); }
void handleInfo()     {  server.send(200, "text/html", InfoHTML()); }
void handleSetKey()   {                         
  if ( ! server.hasArg("kkey") || server.arg("kkey") == NULL ) { server.send(400, "text/plain", "400: Invalid Request"); return;  }

  //Serial.println(server.arg("kkey"));
  keystring = server.arg("kkey");
 
      keystring.toCharArray(char_keystring, keystring.length()+1);

      if (keystring.length() != 32) {
        Serial.println("ERROR - key Eingabefehler!");
        fehler = "ERROR - key Eingabefehler!";
        server.send(200, "text/html", ErrorHTML());
      } else {
        for(char i = 0; i < 16; i++) {
          byte extract;
          char a = keystring[2*i];
          char b = keystring[2*i + 1];
          extract = convertCharToHex(a)<<4 | convertCharToHex(b);
          if (extract == -1)  {
            Serial.print("ERROR - key Eingabefehler!");
            server.send(200, "text/html", ErrorHTML());
            return abbruch();
          } else {
            key[i] = extract;
            //Serial.print("key von der Eingabe: ");Serial.print(key[i], HEX); Serial.print(" ");
              server.send(200, "text/html", SetKeyOKHTML());

          }
        } 
      }
}

void handleNotFound() { server.send(404, "text/plain", "404: Seite nicht gefunden"); }



void loop() {
  server.handleClient();                     

  static unsigned long lastRxTime = 0;    // time when last character was received

  if (Serial.available())  {

    lastRxTime = millis();                // set the last time that a character was received

    if (receiveBufferIndex >= sizeof(receiveBuffer)) {      // if the buffer is full
      Serial.print("buffer overflow; data discarded");
      receiveBufferIndex = 0;
      lastRxTime = 0;
    }
    else {
      receiveBuffer[receiveBufferIndex++] = Serial.read();
    }
  }

  if (lastRxTime != 0 && millis() - lastRxTime >= 100)  { 

            //Serial.print("\n\n=~=~=~= timeout, received ---> "); Serial.print(receiveBufferIndex); Serial.print(" Bytes\n\n");

    nachricht1_startbyte = receiveBuffer[0], DEC; // 104 ... 0x68
    nachricht1_laenge = receiveBuffer[1], DEC; // 250 ... 0xFA
    nachricht1_endebyte = receiveBuffer[(nachricht1_start_offset + nachricht1_laenge - 1)], DEC;
    nachricht2_laenge = receiveBuffer[27 + sizeof nachricht1 + 2 + 1], DEC;              // zeigt auf 0x26 (38 DEZ)
    nachricht2_laenge = nachricht2_laenge - 5;                                          // 33 Zeichen lang; 3 bytes cosem layer und 2 bytes am ende (checksum, endebyte) weglassen


    if (receiveBufferIndex >= 300 && nachricht1_startbyte == 104 && nachricht1_endebyte == 22) {
      if (nachricht2_laenge > 35) {
        Serial.print("\n\n\n\nKaifa MA309 Nachricht - gueltig");
      } else {
        Serial.print("\n\n\n\nKaifa MA110 Nachricht - gueltig");
      }

      werte = ""; // HTML Ausgabe der gelesenen Werte löschen

      // memcpy um den IV zu bekommen
      memcpy(systitle, receiveBuffer + 11, sizeof systitle);            // SysTitle holen
      memcpy(ic, receiveBuffer + 23, sizeof ic);                       // IC holen
      memcpy(iv, systitle, sizeof systitle);                          // IV aus SysTitle und IC erstellen
      memcpy(iv + sizeof systitle, ic, sizeof ic);
      // IV wurde korrekt zusammengestellt

      memcpy(nachricht1, receiveBuffer + 27, sizeof nachricht1 );    // liegt im Array von 0 bis 226     // Nachricht1 kopieren
      memcpy(nachricht2, receiveBuffer + 27 + sizeof nachricht1 + 2 + 9, nachricht2_laenge );           // Nachricht2 kopieren; encrypted payload beginnt bei 9

      Serial.print("\n\nkey: ");
      PrintHex8(key, sizeof key);
      //Serial.print("\nkey vom Server: "); Serial.print(keystring);

      Serial.print("\niv:  ");
      PrintHex8(iv, sizeof iv);

          //Serial.print("\n\nNachricht1 Anfang Ende: "); Serial.print(nachricht1[0], HEX); Serial.print(" "); Serial.print(nachricht1[226], HEX);
          //Serial.print("\nNachricht2 Anfang Ende: "); Serial.print(nachricht2[0], HEX); Serial.print(" "); Serial.print(nachricht2[nachricht2_laenge - 1], HEX);

      byte ciphertxt[227 + nachricht2_laenge];
      memcpy(ciphertxt, nachricht1, 227);
      memcpy(ciphertxt + 227, nachricht2, nachricht2_laenge);

            //Serial.print("\n\nHEX ciphertxt: \n");
            //PrintHex8(ciphertxt,sizeof ciphertxt); Serial.print("\n");

          //Serial.print("\nciphertxt Länge: "); Serial.print(sizeof ciphertxt);
          //Serial.print("\n\nciphertxt Nachricht1 Anfang Ende: "); Serial.print(ciphertxt[0], HEX); Serial.print(" "); Serial.print(ciphertxt[226], HEX);
          //Serial.print("\nciphertxt Nachricht2 Anfang Ende: "); Serial.print(ciphertxt[227], HEX); Serial.print(" "); Serial.print(ciphertxt[sizeof ciphertxt - 1], HEX);

      uint16_t payloadLength;
      memcpy(&payloadLength, &receiveBuffer[20], 2); // die Payload Länge wird im Header mitgegeben !!! Copy payload length Byte[20] und Byte[21] ==> in payLoadLength steht 0x01 0x09
      //Serial.print("\n&receiveBuffer[20]: "); Serial.print(receiveBuffer[20]); Serial.print("\n&receiveBuffer[21]: "); Serial.print(receiveBuffer[21]);
      // HEX ==> DEC
      payloadLength = swap_uint16(payloadLength) - 5; 
      //Serial.print("\npayloadLength: "); Serial.print(payloadLength);

      byte plaintext[payloadLength];

      gcmaes128->setKey(key, gcmaes128->keySize());
      gcmaes128->setIV(iv, 12);
      gcmaes128->decrypt(plaintext, ciphertxt, payloadLength);

      //Serial.print("\n\nHEX decoded: \n");
      //PrintHex8(buffer,sizeof ciphertxt); Serial.print("\n");

      //String obis = Hex8_2_String(plaintext,payloadLength);
      //Serial.print("\n\nObis String: \n");  Serial.print(obis);




// ********************************************************************************************************************************
// ab hier habe ich den Code von domistyle verwendet/verändert/ergänzt => https://github.com/DomiStyle/esphome-dlms-meter/   
// ********************************************************************************************************************************
      
      if (plaintext[0] != 0x0F || plaintext[5] != 0x0C) {
        Serial.print("\n\nERROR - Obis Code fehlerhaft\n");
        return abbruch();
      } 
              //Serial.print("\n\nObis[0] = "); Serial.print(plaintext[0]); Serial.print("\tObis[5] = "); Serial.print(plaintext[5]);

      int currentPosition = DECODER_START_OFFSET;   // = 20

      do
      {
              if (plaintext[currentPosition + OBIS_TYPE_OFFSET] != DataType::OctetString) // wenn pos 20 ungleich 09
              {
                Serial.print("\n\ERROR - Unsupported OBIS header type");
                receiveBufferIndex = 0;
              }
              byte obisCodeLength = plaintext[currentPosition + OBIS_LENGTH_OFFSET]; // obisCodeLength an pos 21 = 6 Zeichen
      
              if (obisCodeLength != 0x06)                                            // wenn ungleich 06
              {
                Serial.print("\n\ERROR - Unsupported OBIS header length");
                receiveBufferIndex = 0;
              }
      
              byte obisCode[obisCodeLength]; // obisCode Array mit der Länge 6 wird erstellt
      
              memcpy(&obisCode[0], &plaintext[currentPosition + OBIS_CODE_OFFSET], obisCodeLength); // Copy OBIS code to array / currentPosition + OBIS_CODE_OFFSET = 22
      
              currentPosition += obisCodeLength + 2;          // Advance past code, position and type
              byte dataType = plaintext[currentPosition];  // neues byte dataType wird mit 0 befüllt ( plaintext[26]
      
             currentPosition++; // Advance past data type
      
              byte dataLength = 0x00;
      
              CodeType codeType = CodeType::Unknown;
 
            if(obisCode[OBIS_A] == Medium::Electricity)
            {
               // Compare C and D against code
                if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L1, 2) == 0)       { codeType = CodeType::VoltageL1;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L2, 2) == 0)  { codeType = CodeType::VoltageL2;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L3, 2) == 0)  { codeType = CodeType::VoltageL3;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L1, 2) == 0)  { codeType = CodeType::CurrentL1;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L2, 2) == 0)  { codeType = CodeType::CurrentL2;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L3, 2) == 0)  { codeType = CodeType::CurrentL3;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_POWER_PLUS, 2)     == 0) { codeType = CodeType::ActivePowerPlus;     }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_POWER_MINUS, 2)    == 0) { codeType = CodeType::ActivePowerMinus;    }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_ENERGY_PLUS, 2)    == 0) { codeType = CodeType::ActiveEnergyPlus;    }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_ENERGY_MINUS, 2)   == 0) { codeType = CodeType::ActiveEnergyMinus;   }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_REACTIVE_ENERGY_PLUS, 2)  == 0) { codeType = CodeType::ReactiveEnergyPlus;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_REACTIVE_ENERGY_MINUS, 2) == 0) { codeType = CodeType::ReactiveEnergyMinus; }
                else {  Serial.print("\n\nERROR: Unsupported OBIS code");  }
            }
            else if(obisCode[OBIS_A] == Medium::Abstract)
            {
                if(memcmp(&obisCode[OBIS_C], ESPDM_TIMESTAMP, 2)          == 0) {   codeType = CodeType::Timestamp;   }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_SERIAL_NUMBER, 2) == 0) { codeType = CodeType::SerialNumber;  }
                else if(memcmp(&obisCode[OBIS_C], ESPDM_DEVICE_NAME, 2)   == 0) { codeType = CodeType::DeviceName;    }
                else {  Serial.print("\n\nERROR: Unsupported OBIS code");   }
            }
            else  {     Serial.print("\n\nERROR: Unsupported OBIS medium");  return abbruch();   }

            uint8_t uint8Value;
            uint16_t uint16Value;
            uint32_t uint32Value;
            float floatValue;

            active_power_plus = 0; 
            active_power_minus = 0;
            active_energy_plus = 0;
            active_energy_minus = 0;
            reactive_energy_plus = 0;
            reactive_energy_minus = 0;
            voltage_l1 = 0;
            voltage_l2 = 0;
            voltage_l3 = 0;
            current_l1 = 0;
            current_l2 = 0;
            current_l3 = 0;
            
            
 
            switch(dataType)
            {
                case DataType::DoubleLongUnsigned: // 0x06
                    dataLength = 4;

                    memcpy(&uint32Value, &plaintext[currentPosition], 4); // Copy bytes to integer
                    uint32Value = swap_uint32(uint32Value); // Swap bytes

                    floatValue = uint32Value; // Ignore decimal digits for now

                    if(codeType == CodeType::ActivePowerPlus){
                        active_power_plus = floatValue;
                        Serial.print("\nWirkleistung Bezug +P (W):\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Wirkleistung Bezug +P (W):&nbsp;</strong>" + String(active_power_plus);
                    }
                    if(codeType == CodeType::ActivePowerMinus){
                        active_power_minus = floatValue;
                        Serial.print("\nWirkleistung Lieferung -P (W):\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Wirkleistung Lieferung -P (W):&nbsp;</strong>" + String(active_power_minus);
                    }
                    if(codeType == CodeType::ActiveEnergyPlus){
                        active_energy_plus = floatValue;
                        Serial.print("\nWirkenergie Bezug +A (Wh):\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Wirkenergie Bezug +A (Wh):&nbsp;</strong>" + String(active_energy_plus);
                    }
                    if(codeType == CodeType::ActiveEnergyMinus){
                        active_energy_minus = floatValue;
                        Serial.print("\nWirkenergie Lieferung -A (Wh):\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Wirkenergie Lieferung -A (Wh):&nbsp;</strong>" + String(active_energy_minus);
                    }
                    if(codeType == CodeType::ReactiveEnergyPlus){
                        reactive_energy_plus = floatValue;
                        Serial.print("\nBlindleistung Bezug +R (Wh):\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Blindleistung Bezug +R (Wh):&nbsp;</strong>" + String(reactive_energy_plus);
                    }
                    if(codeType == CodeType::ReactiveEnergyMinus){
                        reactive_energy_minus = floatValue;
                        Serial.print("\nBlindleistung Lieferung -R (Wh):\t"); Serial.print(floatValue);
                        werte += "</br><strong>Blindleistung Lieferung -R (Wh):&nbsp;</strong>" + String(reactive_energy_minus);
                    }
                break;

                
                case DataType::LongUnsigned: // 0x12
                    dataLength = 2;

                    memcpy(&uint16Value, &plaintext[currentPosition], 2); // Copy bytes to integer
                    uint16Value = swap_uint16(uint16Value); // Swap bytes

                    if(plaintext[currentPosition + 5] == Accuracy::SingleDigit)
                        floatValue = uint16Value / 10.0; // Divide by 10 to get decimal places
                    else if(plaintext[currentPosition + 5] == Accuracy::DoubleDigit)
                        floatValue = uint16Value / 100.0; // Divide by 100 to get decimal places
                    else
                        floatValue = uint16Value; // No decimal places
                    
                    if(codeType == CodeType::VoltageL1) {
                        voltage_l1 = floatValue; 
                        Serial.print("\nSpannung L1 (V):\t\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Spannung L1 (V):&nbsp;</strong>" + String(voltage_l1);
                    }
                    if(codeType == CodeType::VoltageL2) {
                        voltage_l2 = floatValue;
                        Serial.print("\nSpannung L2 (V):\t\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Spannung L2 (V):&nbsp;</strong>" + String(voltage_l2);
                    } 
                    if(codeType == CodeType::VoltageL3){
                        voltage_l3 = floatValue;
                        Serial.print("\nSpannung L3 (V):\t\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Spannung L3 (V):&nbsp;</strong>" + String(voltage_l3);
                    }
                    if(codeType == CodeType::CurrentL1){
                        current_l1 = floatValue;
                        Serial.print("\nStrom L1 (A):\t\t\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Strom L1 (A):&nbsp;</strong>" + String(current_l1);
                    }
                    if(codeType == CodeType::CurrentL2){
                        current_l2 = floatValue;
                        Serial.print("\nStrom L2 (A):\t\t\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Strom L2 (A):&nbsp;</strong>" + String(current_l2);
                    }
                    if(codeType == CodeType::CurrentL3){
                        current_l3 = floatValue;
                        Serial.print("\nStrom L3 (A):\t\t\t\t"); Serial.print(floatValue);
                        werte += "</br><strong>Strom L3 (A):&nbsp;</strong>" + String(current_l3);
                    }
                break;

                
                case DataType::OctetString: // 0x09
                    dataLength = plaintext[currentPosition];
                    currentPosition++; // Advance past string length

                    if(codeType == CodeType::Timestamp) // Handle timestamp generation
                    {
                        memcpy(&uint16Value, &plaintext[currentPosition], 2);
                        year = swap_uint16(uint16Value);

                        memcpy(&month, &plaintext[currentPosition + 2], 1);
                        memcpy(&day, &plaintext[currentPosition + 3], 1);

                        memcpy(&hour, &plaintext[currentPosition + 5], 1);
                        memcpy(&minute, &plaintext[currentPosition + 6], 1);
                        memcpy(&second, &plaintext[currentPosition + 7], 1);

                        sprintf(timestamp, "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute, second);

                        Serial.print("\n\nDatum/Zeit:\t\t\t"); Serial.print(timestamp);
                        werte += "</br><strong>Datum Zeit:&nbsp;</strong>" + String(timestamp);
                    }
                   
                    if(codeType == CodeType::SerialNumber) // Handle SerialNumber generation
                    {
                      dataLength = plaintext[currentPosition-1];
                      memcpy(&uint8Value, &plaintext[currentPosition-1], 1); // Copy byte to integer
                      int zaehlernummerlen = 2*uint8Value;
                      uint8Value = swap_uint16(uint16Value); // Swap bytes
                       memcpy(&zaehlernummer, &plaintext[currentPosition-1], zaehlernummerlen); // Copy byte to integer
                        Serial.print("\nZaehlernummer:\t\t\t\t"); Serial.print(zaehlernummer);
                       werte += "</br><strong>Z&auml;hlernummer:&nbsp;</strong>" + String(zaehlernummer);
                    }
                    if(codeType == CodeType::DeviceName) // Handle DeviceName generation
                    {
                      dataLength = plaintext[currentPosition-1];
                      memcpy(&uint8Value, &plaintext[currentPosition-1], 1); // Copy byte to integer
                      int devicenamelen = 2*uint8Value;
                      uint8Value = swap_uint16(uint16Value); // Swap bytes
                       memcpy(&devicename, &plaintext[currentPosition-1], devicenamelen); // Copy byte to integer
                        Serial.print("\nDevicename:\t\t\t\t"); Serial.print(devicename);
                        werte += "</br><strong>Devicename:&nbsp;</strong>" + String(devicename);
                    }
                break;

                
                default:
                    Serial.print("\n\nERROR: Unsupported OBIS data type");
                    return abbruch();
                break;
            }


            currentPosition += dataLength; // Skip data length
            currentPosition += 2; // Skip break after data

            if(plaintext[currentPosition] == 0x0F) // There is still additional data for this type, skip it
                currentPosition += 6; // Skip additional data and additional break; this will jump out of bounds on last frame
      
       }
        while (currentPosition <= payloadLength); // Loop until arrived at end

// ********************************************************************************************************************************
  
    } else {
      Serial.print("\n\nERROR - Nachricht fehlerhaft ==> warte auf naechsten Block\n");
    }
    receiveBufferIndex = 0;
    lastRxTime = 0;
  }
}
