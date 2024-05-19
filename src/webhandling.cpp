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
#include <DNSServer.h>
#include <IotWebConfAsyncClass.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRoot.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "favicon.h"
#include <vector>

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
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void convertParams();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;
bool gSaveParams = false;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);
AsyncUpdateServer AsyncUpdater;


IotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);

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
        AsyncWebRequestWrapper asyncWebRequestWrapper(request);
        iotWebConf.handleConfig(&asyncWebRequestWrapper);
        }
    );
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(200, "image/x-icon", favicon_ico, sizeof(favicon_ico));
        request->send(response);
        }
    );
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });
    server.onNotFound([](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper(request);
        iotWebConf.handleNotFound(&asyncWebRequestWrapper);
        }
    );

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

void handleData(AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonVariant& json_ = response->getRoot();

	json_["rssi"] = WiFi.RSSI();
	Sensor* _sensor = &Sensor1;
	uint8_t _i = 1;
	while (_sensor != nullptr) {
		if (_sensor->isActive()) {
			json_["sensor" + String(_i)] = _sensor->GetSensorValue();
		}
		_sensor = (Sensor*)_sensor->getNext();
		_i++;
	}

	response->setLength();
	request->send(response);
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

void handleRoot(AsyncWebServerRequest* request){
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    std::string content_;
    MyHtmlRootFormatProvider fp_;

	content_ += fp_.getHtmlHead(iotWebConf.getThingName()).c_str();
	content_ += fp_.getHtmlStyle().c_str();
	content_ += fp_.getHtmlHeadEnd().c_str();
	content_ += fp_.getHtmlScript().c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRow().c_str();
	content_ += fp_.getHtmlTableCol().c_str();
	content_ += String(F("<fieldset align=left style=\"border: 1px solid\">\n")).c_str();
	content_ += String(F("<table border=\"0\" align=\"center\" width=\"100%\">\n")).c_str();
	content_ += String(F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr>\n")).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();
	content_ += fp_.getHtmlFieldset("Temperature").c_str();
	content_ += fp_.getHtmlTable().c_str();
	Sensor* _sensor = &Sensor1;
	uint8_t _i = 1;
	while (_sensor != nullptr) {
		if (_sensor->isActive()) {
			content_ += fp_.getHtmlTableRowSpan(String(TempSourceNames[_sensor->GetSourceId()]), "no data", "sensor" + String(_i)).c_str();
		}
		_sensor = (Sensor*)_sensor->getNext();
		_i++;
	}
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();
	content_ += fp_.getHtmlFieldset("Network").c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowText("MAC Address:", WiFi.macAddress()).c_str();
	content_ += fp_.getHtmlTableRowText("IP Address:", WiFi.localIP().toString().c_str()).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();
	content_ += fp_.addNewLine(2).c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.").c_str();
	content_ += fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlTableColEnd().c_str();
	content_ += fp_.getHtmlTableRowEnd().c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlEnd().c_str();

    std::shared_ptr<uint16_t> pos_ = std::make_shared<uint16_t>(0);

    AsyncWebServerResponse* response = request->beginChunkedResponse("text/html", [content_, pos_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

        std::string chunk_ = "";
		size_t len_ = min(content_.length() - *pos_, maxLen);
		if (len_ > 0) {
			chunk_ = content_.substr(*pos_, len_ - 1);
			chunk_.copy((char*)buffer, chunk_.length());
			*pos_ += len_;
		}
		if (*pos_ <= content_.length())
			return chunk_.length();
		else
			return 0;

        });
    response->addHeader("Server", "ESP Async Web Server");
    request->send(response);

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