# ESP8266-NodeMCU---KAIFA-SmartMeter-Reader-STV-

# Überblick

Ziel der Umsetzung: 
Auslesen der M-Bus Kundenschnittstelle am KAIFA SmartMeter Zähler.

Realisierung:
Der ESP8266 NodeMCU stellt eine WiFi Verbindung zur Verfügung.
Mittels Handy kann man einen HotSpot aufmachen und sich mit
Eine Verbindung herstellen:
 Hotspot am Handy einrichten - Einstellungen/Mobilfunknetz/Tethering&mobiler Hotspot/Persönlicher Hotspot/Hotspot konfigurieren
 Hotspot-Name: Kaifa (kann im Code geändert werden)
 Hotspot-Passwort: 12345678  (kann im Code geändert werden)
 Unter Persönlicher Hotspot findet man den Punkt "Verbundene Geräte". 
 Hier sollte nun eine 1 stehen. Draufklicken um die IP Adresse abzulesen. 
 Mit dieser IP wird im Browser vom Handy die Webseite aufgemacht.
Auf der Webseite kann man den Key, den man vom Netzbetreiber erhalten hat, eingeben.
Mit dem Link "Zählerwerte anzeigen" werden die Daten vom Zähler angezeigt.


# Unterstützte SmartMeter

* Kaifa MA110M 1-Phasenzähler
* Kaifa MA309M 3-Phasenzähler



# Netzbetreiber

* [Tinetz](https://www.tinetz.at/)


# Für den Nachbau benötige Teile

* Handy
* Stromversorgung: herkömmliches USB-Netzteil
* MICRO USB Adapterplatine: Ein USB-Kabel mit Micro USB Stecker wird hier angesteckt. 
 - Die Adapterplatine ist verbunden mit: Step Down Stromversorgungsmodul, Pegelwandler und M-Bus Adapter.
* Step Down Stromversorgungsmodul: Eingangsspannungen werden durch einem AMS1117 von 4.75V bis 12V auf 3.3V heruntergeregelt, bei einem Stromausgang von max. 0.8A
* Pegelwandler: Der bidirektionale Logik Pegelwandler wandelt 5V Signale auf 3,3V und 3,3V auf 5V. Er ist daher verbunden mit: ESP8266 und M-Bus Adapter.
* Controller: ESP8266 Node-MCU mit WiFi und CP2102 Chip. Die Stromversorgung erfolgt über das Step Down Stromversorgungsmodul. Spannung: 3.3V
* M-Bus Adapter: Um die Kundenschnittstelle auslesen zu können, benötigt man einen M-Bus Adapter. Dieser setzt die 32V Signale um, auf TTL Pegel.
 - Den Adapter kann man sich entweder selber bauen, oder als fertige Platine im Internet bestellen.
 - Selber bauen: https://pc-projekte.lima-city.de/MBus-Konverter.html
 - Fertig bestellen: https://www.mikroe.com/m-bus-slave-click
* Stecker Kundenschnittstelle: RJ12 Kabel



# Software Installation

Der Controller wird mit Arduino programmiert.
Am Arduino das Board NodeMCU 1.0 (ESP-12E Module) und entsprechenden COM-Port auswählen


