# NMEA2000-DS1820 Temperatur Monitor

## Table of contents
- [NMEA2000-DS1820 Temperatur Monitor](#nmea2000-ds1820-temperatur-monitor)
	- [Table of contents](#table-of-contents)
	- [Description](#description)
	- [Schema](#schema)
	- [NMEA 2000](#nmea-2000)
	- [Librarys](#librarys)
	- [Part list](#part-list)
	- [Settings](#settings)
		- [NMEA 2000 Settings](#nmea-2000-settings)
			- [Instance](#instance)
			- [SID](#sid)
		- [Sensor](#sensor)
			- [Source](#source)
			- [Threshold (°C)](#threshold-c)
			- [Method](#method)
			- [Alert Description](#alert-description)
			- [Temporary silence time (minutes)](#temporary-silence-time-minutes)
	- [WiFi](#wifi)
		- [Default Password](#default-password)
			- [Default IP address](#default-ip-address)
		- [OTA](#ota)
		- [Configuration options](#configuration-options)
	- [Blinking codes](#blinking-codes)
	- [Reset](#reset)

## Description
A temperature sensor system that can accommodate up to 4 DS1820 sensors. Each sensor can be configured to measure temperature from -50°C (-58°F) up to 125°C (257°F). You can select the temperature source for each sensor (e.g., alternator, shaft seal, oil filter), and an alarm threshold can be set to trigger predefined alarms when the temperature exceeds the threshold. The system communicates via the NMEA 2000 protocol, sending temperature values and alarms as NMEA 2000 messages over an NMEA bus. Configuration is done through a web interface, and real-time values can be viewed on a website. Additionally, there's a link on the configuration page for convenient firmware updates.

## Schema
<img src="sch/schema.png" width="600" alt="schema">

## NMEA 2000
Depending on the temperature source, one of the following PNGs are sent

- 130310, // Environmental Parameters - DEPRECATED
- 130312, // Temperature - DEPRECATED
- 130316, // Temperature, Extended Range

The device is also capable of sending alerts. In this case these PGN's are used
- 126983, // Alert
- 126984, // Alert response
- 126985, // Alert text


## Librarys
The Software has been created using Visual Studio with the addon Visual Micro. In order to build it you als need some libraries.
- [OneWire (2.3.7)](https://github.com/PaulStoffregen/OneWire)
- [DallasTemperature(3.9.0)](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [NMEA2000](https://github.com/ttlappalainen/NMEA2000)
- [NMEA200_ESP32](https://github.com/ttlappalainen/NMEA2000_esp32)
- [NMEA2000_AlertMessages](https://github.com/minou65/NMEA2000-AlertMessages)
- [AsyncTCP (1.1.1)](https://github.com/me-no-dev/AsyncTCP.git)
- [ESPAsyncWebServer (1.2.3)](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ArduionJSON (6.21.2)](https://github.com/bblanchon/ArduinoJson)
- [Webserial (1.4.0)](https://github.com/ayushsharma82/WebSerial)
- [IotWebConf](https://github.com/minou65/IotWebConf)
- [IotWebConfAsync](https://github.com/minou65/IotWebConfAsync)
- [IotWebRoot](https://github.com/minou65/IotWebRoot)

## Part list
| Part | Value | Supplier |
| --- | --- | --- |
| D1 | 1N4001 | Reichelt |
| ESP1 | ESP32DEVKITV1 | [ebay](https://www.ebay.ch/itm/204191675506?var=504772734176) |
| IC1 | R-78E05-1.0 | Reichelt |
| IC2 | INA226 | [ebay](https://www.ebay.ch/itm/314750066199) |
| IC3 | MCP2562 | Reichelt oder [ebay](https://www.ebay.ch/itm/364610349378) |
| R1 | 4.7k | Reichelt |
| X1 | 2 x AKL 057-02 | Reichelt |
| X2 - X4 | PSS 254/3G | Reichelt |

Some pictures from the assambled hardware

<img title="picture 1" src="img/20230723_085806591_iOS.jpg" width="600">
<img title="picture 1" src="img/20230723_085811033_iOS.jpg" width="600">
<img title="picture 1" src="img/20230723_085825516_iOS.jpg" width="600">


## Settings
### NMEA 2000 Settings
#### Instance
This should be unique at least on one device. May be best to have it unique over all devices sending this PGN. Depending on the number of sensors connected, between 1 and 4 instances are used, starting with the number set here.

#### SID
Sequence identifier. In most cases you can use just 255 for SID. The sequence identifier field is used to tie different PGNs data together to same sampling or calculation time.

### Sensor
Depending on the number of sensors connected, the following settings can be made for each sensor.

#### Source
One of the following temperature sources can be selected
- Sea water temperature
- Outside temperature
- Inside temperature
- Engine room temperature
- Main cabin temperature
- Live well temperature
- Bait well temperature
- Refrigeration temperature
- Heating system temperature
- Freezer temperature
- Exhaust gas temperature
- Shaft seal temparature

#### Threshold (°C)
Threshold in °C

#### Method
Method with which the threshold value is compared to the current value
- equal
- lower then
- greater then

#### Alert Description
A description for the alarm

#### Temporary silence time (minutes)
This sensor supports the Temporary silence mode. With this parameter you can set the time how long the alert should be silent.

## WiFi

### Default Password
When not connected to an AP the default password is 123456789

#### Default IP address
When in AP mode, the default IP address is 192.168.4.1

### OTA
OTA is enabled, use default IP address or if connected to a AP the correct address.
Port is the default port.

### Configuration options
After the first boot, there are some values needs to be set up.
These items are maked with __*__ (star) in the list below.

You can set up the following values in the configuration page:

-  __Thing name__ - Please change the name of the device to
a name you think describes it the most. It is advised to
incorporate a location here in case you are planning to
set up multiple devices in the same area. You should only use
english letters, and the "_" underscore character. Thus, must not
use Space, dots, etc. E.g. `lamp_livingroom` __*__
- __AP password__ - This password is used, when you want to
access the device later on. You must provide a password with at least 8,
at most 32 characters.
You are free to use any characters, further more you are
encouraged to pick a password at least 12 characters long containing
at least 3 character classes. __*__
- __WiFi SSID__ - The name of the WiFi network you want the device
to connect to. __*__
- __WiFi password__ - The password of the network above. Note, that
unsecured passwords are not supported in your protection. __*__

## Blinking codes
Prevoius chapters were mentioned blinking patterns, now here is a
table summarize the menaning of the blink codes.

- __Rapid blinking__ (mostly on, interrupted by short off periods) -
Entered Access Point mode. This means the device create an own WiFi
network around it. You can connect to the device with your smartphone
(or WiFi capable computer).
- __Alternating on/off blinking__ - Trying to connect the configured
WiFi network.
- __Mostly off with occasional short flash__ - The device is online.

## Reset
When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
password to buld an AP. (E.g. in case of lost password)

Reset pin is GPIO 13
