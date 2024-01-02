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

#include <DNSServer.h>

#include <IotWebConf.h>

#include "common.h"
#include "webhandling.h"


// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "NMEA-DS1820";

tTemperatur gTemperaturs[4] = {
    {N2kts_SeaTemperature, 0.0},
    {N2kts_OutsideTemperature, 0.0},
    {N2kts_InsideTemperature, 0.0},
    {N2kts_EngineRoomTemperature, 0.0} };

// -- Method declarations.
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;

DNSServer dnsServer;
WebServer server(80);

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

char InstanceValue[NUMBER_LEN];
char SIDValue[NUMBER_LEN];
iotwebconf::ParameterGroup InstanceGroup = iotwebconf::ParameterGroup("InstanceGroup", "NMEA 2000 Settings");
iotwebconf::NumberParameter InstanceParam = iotwebconf::NumberParameter("Instance", "InstanceParam", InstanceValue, NUMBER_LEN, "255", "1..255", "min='1' max='254' step='1'");
iotwebconf::NumberParameter SIDParam = iotwebconf::NumberParameter("SID", "SIDParam", SIDValue, NUMBER_LEN, "255", "1..254", "min='1' max='255' step='1'");

char TempSourceValue1[STRING_LEN];
iotwebconf::ParameterGroup TempSourceGroup = iotwebconf::ParameterGroup("TemperaturGroup", "Temperatur source");
iotwebconf::SelectParameter TempSource1 = iotwebconf::SelectParameter("Sensor 1",
    "TempSource1",
    TempSourceValue1,
    STRING_LEN,
    (char*)TempSourceValues,
    (char*)TempSourceNames,
    sizeof(TempSourceValues) / STRING_LEN,
    STRING_LEN,
    TempSourceNames[gTemperaturs[0].Source]
);

char TempSourceValue2[STRING_LEN];
iotwebconf::SelectParameter TempSource2 = iotwebconf::SelectParameter("Sensor 2",
    "TempSource2",
    TempSourceValue2,
    STRING_LEN,
    (char*)TempSourceValues,
    (char*)TempSourceNames,
    sizeof(TempSourceValues) / STRING_LEN,
    STRING_LEN,
    TempSourceNames[gTemperaturs[1].Source]
);

char TempSourceValue3[STRING_LEN];
iotwebconf::SelectParameter TempSource3 = iotwebconf::SelectParameter("Sensor 3",
    "TempSource3",
    TempSourceValue3,
    STRING_LEN,
    (char*)TempSourceValues,
    (char*)TempSourceNames,
    sizeof(TempSourceValues) / STRING_LEN,
    STRING_LEN,
    TempSourceNames[gTemperaturs[2].Source]
);

char TempSourceValue4[STRING_LEN];
iotwebconf::SelectParameter TempSource4 = iotwebconf::SelectParameter("Sensor 4",
    "TempSource4",
    TempSourceValue4,
    STRING_LEN,
    (char*)TempSourceValues,
    (char*)TempSourceNames,
    sizeof(TempSourceValues) / STRING_LEN,
    STRING_LEN,
    TempSourceNames[gTemperaturs[3].Source]
);

void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");


    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    InstanceGroup.addItem(&InstanceParam);
    InstanceGroup.addItem(&SIDParam);
    iotWebConf.addParameterGroup(&InstanceGroup);

    TempSourceGroup.addItem(&TempSource1);

    if (gDeviceCount >= 2) {
        TempSourceGroup.addItem(&TempSource2);
    }

    if (gDeviceCount >= 3) {
        TempSourceGroup.addItem(&TempSource3);
    }

    if (gDeviceCount >= 4) {
        TempSourceGroup.addItem(&TempSource4);
    }
    iotWebConf.addParameterGroup(&TempSourceGroup);

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

            for (uint8_t i = 0; i < gDeviceCount; i++) {
                page += "<tr><td align=left>" + String(TempSourceNames[gTemperaturs[i].Source]) + ":</td><td>" + String(gTemperaturs[i].Value) + "&deg;C" + "</td></tr>";
            }

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
    gTemperaturs[0].Value = tN2kTempSource(atoi(TempSourceValue1));
    gTemperaturs[1].Value = tN2kTempSource(atoi(TempSourceValue2));
    gTemperaturs[2].Value = tN2kTempSource(atoi(TempSourceValue3));
    gTemperaturs[3].Value = tN2kTempSource(atoi(TempSourceValue4));
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}