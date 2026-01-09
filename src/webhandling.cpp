#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "webhandling.h"
#include "favicon.h"
#include "neotimer.h"

#include <DNSServer.h>
#include <IotWebConfAsync.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRoot.h>

extern void UpdateAlertSystem();


// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "NMEA-DS1820";

Sensor Sensor1 = Sensor("sensor1", "Sensor 1");
Sensor Sensor2 = Sensor("sensor2", "Sensor 2");
Sensor Sensor3 = Sensor("sensor3", "Sensor 3");
Sensor Sensor4 = Sensor("sensor4", "Sensor 4");

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
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void convertParams();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;
bool gSaveParams = false;
uint8_t APModeOfflineTime = 0;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);
AsyncUpdateServer AsyncUpdater;
Neotimer APModeTimer = Neotimer();


AsyncIotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);

char APModeOfflineValue[STRING_LEN];
iotwebconf::NumberParameter APModeOfflineParam = iotwebconf::NumberParameter("AP offline mode after (minutes)", "APModeOffline", APModeOfflineValue, NUMBER_LEN, "0", "0..30", "min='0' max='30', step='1'");

void resetAllSensors() {
    Sensor* sensor_ = &Sensor1;
    while (sensor_ != nullptr) {
        sensor_->resetToDefaults();
        sensor_ = sensor_->getNext();
    }
}

void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");


    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL); 
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.setHtmlFormatProvider(&customHtmlFormatProvider);

    iotWebConf.addParameterGroup(&Config);

    iotWebConf.addParameterGroup(&Sensor1);

    Sensor1.setNext(&Sensor2);
    iotWebConf.addParameterGroup(&Sensor2);

    Sensor2.setNext(&Sensor3);
    iotWebConf.addParameterGroup(&Sensor3);

    Sensor3.setNext(&Sensor4);
    iotWebConf.addParameterGroup(&Sensor4);

    iotWebConf.addSystemParameter(&APModeOfflineParam);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { AsyncUpdater.setup(&server, updatePath); },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;
    
    // -- Initializing the configuration.
    iotWebConf.init();

    // NMEAGroup.visible = false;

    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { handleRoot(request); });
    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* asyncWebRequestWrapper_ = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(asyncWebRequestWrapper_);
        }
    );
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response_ = request->beginResponse_P(200, "image/x-icon", favicon_ico, sizeof(favicon_ico));
        request->send(response_);
        }
    );
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });
    server.onNotFound([](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper_(request);
        iotWebConf.handleNotFound(&asyncWebRequestWrapper_);
        }
    );

    WebSerial.begin(&server, "/webserial");

    if (APModeOfflineTime > 0) {
        APModeTimer.start(APModeOfflineTime * 60 * 1000);
    }

    Serial.println("Ready.");
}

void wifiLoop() {
    // -- doLoop should be called as frequently as possible.
    iotWebConf.doLoop();
    ArduinoOTA.handle();
    
    if (gSaveParams) {
        Serial.println(F("Parameters are changed,save them"));

        iotWebConf.saveConfig();
        gSaveParams = false;
    }

    if (APModeTimer.done()) {
        Serial.println(F("AP mode offline time reached"));
        iotWebConf.goOffLine();
        APModeTimer.stop();
    }
    
    if (AsyncUpdater.isFinished()) {
        Serial.println(F("Firmware update finished"));
        delay(1000);
        ESP.restart();
    }
}

void wifiConnected() {
    ArduinoOTA.begin();
}

void handleData(AsyncWebServerRequest* request) {
	String json_ = "{";
	json_ += "\"rssi\":" + String(WiFi.RSSI());
	Sensor* sensor_ = &Sensor1;
	uint8_t i_ = 1;
	while (sensor_ != nullptr) {
		if (sensor_->isActive()) {
			json_ += ",\"sensor" + String(i_) + "\":" + String(sensor_->GetSensorValue(), 2);
		}
		sensor_ = (Sensor*)sensor_->getNext();
		i_++;
	}
	json_ += "}";
	request->send(200, "application/json", json_);
}

class MyHtmlRootFormatProvider : public HtmlRootFormatProvider {
protected:
    virtual String getScriptInner() {
        String s_ = HtmlRootFormatProvider::getScriptInner();
        s_.replace("{millisecond}", "5000");
        s_ += F("function updateData(jsonData) {\n");
        s_ += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
		Sensor* sensor_ = &Sensor1;
        uint8_t i_ = 1;
        while (sensor_ != nullptr) {
            if (sensor_->isActive()) {
				s_ += "   document.getElementById('sensor" + String(i_) + "').innerHTML = jsonData.sensor" + String(i_) + " + \"&deg;C\" \n";
			}
			sensor_ = (Sensor*)sensor_->getNext();
			i_++;
		}

        s_ += F("}\n");
        
        return s_;
    }
};

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    AsyncResponseStream* response_ = request->beginResponseStream("text/html", 512);
    MyHtmlRootFormatProvider fp_;

    response_->print(fp_.getHtmlHead(iotWebConf.getThingName()));
    response_->print(fp_.getHtmlStyle());
    response_->print(fp_.getHtmlHeadEnd());
    response_->print(fp_.getHtmlScript());
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRow());
    response_->print(fp_.getHtmlTableCol());
    response_->print(F("<fieldset align=left style=\"border: 1px solid\">\n"));
    response_->print(F("<table border=\"0\" align=\"center\" width=\"100%\">\n"));
    response_->print(F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr>\n"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlFieldset("Temperature"));
    response_->print(fp_.getHtmlTable());

    Sensor* sensor_ = &Sensor1;
    uint8_t i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            response_->print(fp_.getHtmlTableRowSpan(String(sensor_->GetSourceName()) + ": ", "no data", "sensor" + String(i_)));
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }

    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlFieldset("Network"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowText("MAC Address:", WiFi.macAddress()));
    response_->print(fp_.getHtmlTableRowText("IP Address:", WiFi.localIP().toString()));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.addNewLine(2));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration."));
    response_->print(fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlTableColEnd());
    response_->print(fp_.getHtmlTableRowEnd());
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlEnd());

    response_->addHeader("Server", "ESP Async Web Server");
    request->send(response_);
}

void convertParams() {

    gN2KInstance = Config.GetInstance();
    gN2KSID = Config.GetSID();

    APModeOfflineTime = atoi(APModeOfflineValue);

    ArduinoOTA.setHostname(iotWebConf.getThingName());
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}