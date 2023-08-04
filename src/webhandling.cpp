// 
// 
// 

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
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include <IotWebConfTParameter.h>
#include <IotWebConfOptionalGroup.h>

#include "common.h"
#include "webhandling.h"


// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "NMEA-TemperaturMonitor";

// -- Method declarations.
void handleRoot();

// -- Callback methods.
void wifiConnected();

DNSServer dnsServer;
WebServer server(80);

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");


    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    // -- Initializing the configuration.
    iotWebConf.init();

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
    page.replace("{v}", "Temperatur monitor");
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

            page += "<tr><td align=left>Cabin:</td><td>" + String(gCabinTemperature) + "°C" + "</td></tr>";
            page += "<tr><td align=left>Outside:</td><td>" + String(gOutsideTemperature) + "°C" + "</td></tr>";

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