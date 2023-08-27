# NMEA2000-DS1820 Temperatur Monitor

## Table of contents
- [Description](#description)
- [NMEA 2000](#nmea2000)
- [Librarys](#libs)
- [Required hardware](#hardware)
- [Settings](#settings)
- [WiFi](#wifi)
	- [Default password](#wifipassword)
	- [Default IP address](#wifiipaddress)
	- [OTA](#wifiota)
	- [Configuration options](#wificonfiguration)
	- [Blinking codes](#wifiblinkingcodes)
	- [Reset](#wifireset)

## Description <a name="description"></a>

This device can monitor temperaturs and send the values over a NMEA 2000 bus. The values can also be viewed via a web interface, For that the Device should be connected to a wifi AP

The following temperature sources can be selected:

- Sea water temperature
- Outside temperature
- Inside temperature
- Engine room temperature
- Main cabin temperature
- Live well temperature
- Bait well temperature
- Refrigeration temperature
- Heating system temperature
- Dew point temperature
- Apparent wind chill temperature
- Theoretical wind chill temperature
- Heat index temperature
- Freezer temperature
- Exhaust gas temperature
- Shaft seal temparature"

## NMEA 2000 <a name="nmea2000"></a>

Depending on the temperature source, one of the following PNGs are sent

- 130310, // Environmental Parameters - DEPRECATED
- 130312, // Temperature - DEPRECATED
- 130316, // Temperature, Extended Range


## Librarys <a name="libs"></a>

The Software has been created using Visual Studio with the addon Visual Micro. In order to build it you als need some libraries.

- prampec/IotWebConf
- OneWire
- DallasTemperature
- NMEA2000
- NMEA200_ESP

## Required hardware <a name="hardware"></a>

The number of connected sensors is recognized automatically. A maximum of 4 DS1820 can be connected.

The following [schema](/sch/NMEA2000-DS1820.pdf) show you, how to put all together.

Some pictures from the assabled hardware

<img title="picture 1" src="/img/20230723_085806591_iOS.jpg">
<img title="picture 1" src="/img/20230723_085811033_iOS.jpg">
<img title="picture 1" src="/img/20230723_085825516_iOS.jpg">


## Settings <a name="settings"></a>

Depending on the number of connected sensors, the source can be selected for up to 4 sensors.

<img title="settings" src="/img/settings-temperatur.png">

## WiFi <a name="wifi"></a>

### Default Password <a name="wifipassword"></a>

When not connected to an AP the default password is 123456789

### Default IP address <a name="wifiipaddress"></a>

When in AP mode, the default IP address is 192.168.4.1

### OTA <a name="wifiota"></a>
OTA is enabled, use default IP address or if connected to a AP the correct address.
Port is the default port.

### Configuration options <a name="wificonfiguration"></a>

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

### Blinking codes <a name="wifiblinkingcodes"></a>

Prevoius chapters were mentioned blinking patterns, now here is a
table summarize the menaning of the blink codes.

- __Rapid blinking__ (mostly on, interrupted by short off periods) -
Entered Access Point mode. This means the device create an own WiFi
network around it. You can connect to the device with your smartphone
(or WiFi capable computer).
- __Alternating on/off blinking__ - Trying to connect the configured
WiFi network.
- __Mostly off with occasional short flash__ - The device is online.

### Reset <a name="wifireset"></a>

When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
password to buld an AP. (E.g. in case of lost password)

Reset pin is GPIO 36