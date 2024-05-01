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
    }

    if (gDeviceCount >= 3) {
        Sensor2.setNext(&Sensor3);
        iotWebConf.addParameterGroup(&Sensor3);
    }

    if (gDeviceCount >= 4) {
        Sensor3.setNext(&Sensor4);
        iotWebConf.addParameterGroup(&Sensor4);
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

void handleRoot()
{
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }

    String page = HTML_Start_Doc;
    page.replace("{v}", iotWebConf.getThingName());
    page += "<style>";
    page += ".de{background-color:#ffaaaa;} .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} .c{text-align: center;} div,input,select{padding:5px;font-size:1em;} input{width:95%;} select{width:100%} input[type=checkbox]{width:auto;scale:1.5;margin:10px;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} fieldset{border-radius:0.3rem;margin: 0px;}";
    // page.replace("center", "left");
    page += "</style>";
    page += "<meta http-equiv=refresh content=15 />";
    page += HTML_Start_Body;
    page += "<table border=0 align=center>";
    page += "<tr><td>";

        page += HTML_Start_Fieldset;
        page += HTML_Fieldset_Legend;
        page.replace("{l}", "Temperaturs");
            page += HTML_Start_Table;

            Sensor* _sensor = &Sensor1;
            while (_sensor != nullptr) {
                if (_sensor->isActive()) {
                    if (_sensor->Alert.isAlert()) {
                        page += "<tr><td align=left style=\"color:red;\">" + String(TempSourceNames[_sensor->GetSourceId()]) + ":</td><td style=\"color:red;\">" + String(_sensor->GetSensorValue()) + "&deg;C" + "</td></tr>";
                    }
                    else {
                        page += "<tr><td align=left>" + String(TempSourceNames[_sensor->GetSourceId()]) + ":</td><td>" + String(_sensor->GetSensorValue()) + "&deg;C" + "</td></tr>";
                    }
                    
                }

                _sensor = (Sensor*)_sensor->getNext();
            }

        page += HTML_End_Table;
        page += HTML_End_Fieldset;

        page += HTML_Start_Fieldset;
        page += HTML_Fieldset_Legend;
        page.replace("{l}", "Network");
        page += HTML_Start_Table;

        page += "<tr><td align=left>MAC Address:</td><td>" + String(WiFi.macAddress()) + "</td></tr>";
        page += "<tr><td align=left>IP Address:</td><td>" + String(WiFi.localIP().toString().c_str()) + "</td></tr>";

        page += HTML_End_Table;
        page += HTML_End_Fieldset;

        page += "<br>";
        page += "<br>";

        page += HTML_Start_Table;
        page += "<tr><td align=left>Go to <a href = 'config'>configure page</a> to change configuration.</td></tr>";
        // page += "<tr><td align=left>Go to <a href='setruntime'>runtime modification page</a> to change runtime data.</td></tr>";

        page += "<tr><td><font size=1>Version: " + String(Version) + "</font></td></tr>";
        page += HTML_End_Table;
        page += HTML_End_Body;

        page += HTML_End_Doc;


    server.send(200, "text/html", page);
}

void convertParams() {

    gN2KSource[TemperaturDevice] = Config.Source();
    gN2KSource[AlarmDevice] = Config.SourceAlert();

    UpdateAlertSystem();
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}