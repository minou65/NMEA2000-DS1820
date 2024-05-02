#define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>
#if ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>      
#endif

#include <time.h>
//needed for library


#include "common.h"
#include "webhandling.h"
#include "IotWebRoot.h"

extern void UpdateAlertSystem();


// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "NMEA-DS1820";

Sensor Sensor1 = Sensor("sensor1");
Sensor Sensor2 = Sensor("sensor2");
Sensor Sensor3 = Sensor("sensor3");
Sensor Sensor4 = Sensor("sensor4");

NMEAConfig Config = NMEAConfig();

class CustomHtmlFormatProvider : public iotwebconf::OptionalGroupHtmlFormatProvider {
protected:
    virtual String getFormEnd() {
        String _s = OptionalGroupHtmlFormatProvider::getFormEnd();
        _s += F("</br>Return to <a href='/'>home page</a>.");
        return _s;
    }
};
CustomHtmlFormatProvider customHtmlFormatProvider;

// -- Method declarations.
void handleData();
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;
bool gSaveParams = false;

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");


    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL); 
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.setHtmlFormatProvider(&customHtmlFormatProvider);

    iotWebConf.addParameterGroup(&Config);

    iotWebConf.addParameterGroup(&Sensor1);
    Sensor1.setActive(true);

    if (gDeviceCount >= 2) {
        Sensor1.setNext(&Sensor2);
        iotWebConf.addParameterGroup(&Sensor2);
        Sensor2.setActive(true);
    }

    if (gDeviceCount >= 3) {
        Sensor2.setNext(&Sensor3);
        iotWebConf.addParameterGroup(&Sensor3);
        Sensor3.setActive(true);
    }

    if (gDeviceCount >= 4) {
        Sensor3.setNext(&Sensor4);
        iotWebConf.addParameterGroup(&Sensor4);
        Sensor4.setActive(true);
    }

    // -- Define how to handle updateServer calls.
    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
        [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;
    
    // -- Initializing the configuration.
    iotWebConf.init();

    // NMEAGroup.visible = false;

    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.on("/data", HTTP_GET, []() { handleData(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");
}

void wifiLoop() {
    // -- doLoop should be called as frequently as possible.
    iotWebConf.doLoop();
    ArduinoOTA.handle();
    
    if (gSaveParams) {
        Serial.println(F("Parameters are changed,save them"));

        Config.SetSource(gN2KSource[TemperaturDevice]);
        Config.SetSourceAlert(gN2KSource[AlarmDevice]);

        iotWebConf.saveConfig();
        gSaveParams = false;
    }
}

void wifiConnected() {
    ArduinoOTA.begin();
}

void handleData() {
	String _response = "{";
    _response += "\"rssi\":\"" + String(WiFi.RSSI()) + "\",";
    Sensor* _sensor = &Sensor1;
    uint8_t _i = 1;
    while (_sensor != nullptr) {
        if (_sensor->isActive()) {
            _response += "\"sensor" + String(_i) + "\":\"" + String(_sensor->GetSensorValue(), 2) + "\"";
        }
        _sensor = (Sensor*)_sensor->getNext();

        if (_sensor != nullptr) {
            _response += ",";
        }
        _i++;
    }
    _response += "}";
	server.send(200, "text/plain", _response);
}

class MyHtmlRootFormatProvider : public HtmlRootFormatProvider {
protected:
    virtual String getScriptInner() {
        String _s = HtmlRootFormatProvider::getScriptInner();
        _s.replace("{millisecond}", "5000");
        _s += F("function updateData(jsonData) {\n");
        _s += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
		Sensor* _sensor = &Sensor1;
        uint8_t _i = 1;
        while (_sensor != nullptr) {
            if (_sensor->isActive()) {
				_s += "   document.getElementById('sensor" + String(_i) + "').innerHTML = jsonData.sensor" + String(_i) + " + \"&deg;C\" \n";
			}
			_sensor = (Sensor*)_sensor->getNext();
			_i++;
		}

        _s += F("}\n");
        
        return _s;
    }
};

void handleRoot()
{
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }

    MyHtmlRootFormatProvider rootFormatProvider;

    String _response = "";
    _response += rootFormatProvider.getHtmlHead(iotWebConf.getThingName());
    _response += rootFormatProvider.getHtmlStyle();
    _response += rootFormatProvider.getHtmlHeadEnd();
    _response += rootFormatProvider.getHtmlScript();

    _response += rootFormatProvider.getHtmlTable();
    _response += rootFormatProvider.getHtmlTableRow() + rootFormatProvider.getHtmlTableCol();

    _response += F("<fieldset align=left style=\"border: 1px solid\">\n");
        _response += F("<table border=\"0\" align=\"center\" width=\"100%\">\n");
        _response += F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr>\n");
        _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlFieldsetEnd();

    _response += rootFormatProvider.getHtmlFieldset("Temperature");
        _response += rootFormatProvider.getHtmlTable();
        Sensor* _sensor = &Sensor1;
        uint8_t _i = 1;
        while (_sensor != nullptr) {
            if (_sensor->isActive()) {
                _response += rootFormatProvider.getHtmlTableRowSpan(String(TempSourceNames[_sensor->GetSourceId()]),  "no data", "sensor" + String(_i));
            }
            _sensor = (Sensor*)_sensor->getNext();
            _i++;
        }
        _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlFieldsetEnd();

    _response += rootFormatProvider.getHtmlFieldset("Network");
        _response += rootFormatProvider.getHtmlTable();
        _response += rootFormatProvider.getHtmlTableRowText("MAC Address:", WiFi.macAddress());
        _response += rootFormatProvider.getHtmlTableRowText("IP Address:", WiFi.localIP().toString().c_str());
        _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlFieldsetEnd();

    _response += rootFormatProvider.addNewLine(2);

    _response += rootFormatProvider.getHtmlTable();
        _response += rootFormatProvider.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.");
        _response += rootFormatProvider.getHtmlTableRowText(rootFormatProvider.getHtmlVersion(Version));
        _response += rootFormatProvider.getHtmlTableEnd();

        _response += rootFormatProvider.getHtmlTableColEnd() + rootFormatProvider.getHtmlTableRowEnd();
        _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlEnd();

    server.send(200, "text/html", _response);
}

void convertParams() {

    gN2KInstance = Config.Instance();
    gN2KSID = Config.SID();

    gN2KSource[TemperaturDevice] = Config.Source();
    gN2KSource[AlarmDevice] = Config.SourceAlert();

    UpdateAlertSystem();
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}