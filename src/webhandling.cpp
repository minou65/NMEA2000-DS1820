#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>

#include "webhandling.h"
#include "favicon.h"
#include "neotimer.h"

#include <DNSServer.h>
#include <IotWebConfAsyncTab.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRootTab.h>
#include <RebootManager.h>
#include <N2kAlertTypes.h>

extern void UpdateAlertSystem();
extern const char* N2kEnumAlertTypeToStr(tN2kAlertType enumVal);
extern const char* N2kEnumAlertTypeToStr(tN2kAlertState enumVal);

const char thingName[] = "NMEA-DS1820";

Sensor Sensor1 = Sensor("sensor1", "Sensor 1");
Sensor Sensor2 = Sensor("sensor2", "Sensor 2");
Sensor Sensor3 = Sensor("sensor3", "Sensor 3");
Sensor Sensor4 = Sensor("sensor4", "Sensor 4");

NMEAConfig Config = NMEAConfig();

const char html_form_end[] PROGMEM = R"=====(
</br><form action='/reboot' method='get'><button type='submit'>Reboot</button></form>
</br><form action='/' method='get' style='display:inline;'>
  <button type='submit'>Home</button>
  </br>
</form>
</br><form action='/post' method='post' style='display:inline;' onsubmit="event.preventDefault(); postReset();">
  <button type='submit'>Reset to factory defaults</button>
</form>
<script>
function postReset() {
    fetch('/post', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'reset=true'
    })
    .then(response => {
        if (response.ok) { 
            window.location.reload(true);  // Hard reload
        }
    })
    .catch(error => {
        console.error('Reset fehlgeschlagen:', error);
        alert('Reset fehlgeschlagen!');
    });
}
</script>
)=====";

class CustomHtmlFormatProvider : public AsyncTabHtmlFormatProvider {
public:
    CustomHtmlFormatProvider(std::vector<AsyncTabInfo>* tabs, int minWidth = 500, int maxWidth = 600)
        : AsyncTabHtmlFormatProvider(tabs, minWidth, maxWidth) {
    }

protected:
    String getFormEnd() override {
        return AsyncTabHtmlFormatProvider::getFormEnd() + String(FPSTR(html_form_end));
    }
};
CustomHtmlFormatProvider* customHtmlFormatProvider = nullptr;

void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void handlePost(AsyncWebServerRequest* request);
void handleReboot(AsyncWebServerRequest* request);
void convertParams();
void configSaved();
void wifiConnected();

bool gParamsChanged = true;
bool gSaveParams = false;
bool gShouldReboot = false;
uint8_t APModeOfflineTime = 0;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);
AsyncUpdateServer AsyncUpdater;
Neotimer APModeTimer = Neotimer();

// TAB-VERSION verwenden!
AsyncIotWebConfTab iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);

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

    // === TABS KONFIGURIEREN ===
    // System Tab soll an letzter Stelle sein
    iotWebConf.setSystemTabPosition(-1);  // -1 = letzte Position
    iotWebConf.setSystemTabName("System");

    // Parameter Groups zu Tabs hinzufügen - REIHENFOLGE GEÄNDERT!
    // 1. Sensors Tab (als erstes)
    iotWebConf.addParameterGroup(&Sensor1, "Sensors");
    Sensor1.setNext(&Sensor2);
    iotWebConf.addParameterGroup(&Sensor2, "Sensors");
    
    Sensor2.setNext(&Sensor3);
    iotWebConf.addParameterGroup(&Sensor3, "Sensors");
    
    Sensor3.setNext(&Sensor4);
    iotWebConf.addParameterGroup(&Sensor4, "Sensors");

    // 2. NMEA Tab (als zweites)
    iotWebConf.addParameterGroup(&Config, "NMEA");
    
    // 3. System Tab kommt automatisch an letzter Stelle (wegen setSystemTabPosition(-1))

    // System Parameter (wird automatisch im System Tab angezeigt)
    iotWebConf.addSystemParameter(&APModeOfflineParam);

    // CustomHtmlFormatProvider mit Tabs-Vektor erstellen
    auto customProvider = new CustomHtmlFormatProvider(iotWebConf.getTabsVector());
    customProvider->setContainerWidth(700, 700);
    iotWebConf.setHtmlFormatProvider(customProvider);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { AsyncUpdater.setup(&server, updatePath); },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    iotWebConf.init();

    convertParams();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { handleRoot(request); });

    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* asyncWebRequestWrapper_ = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(asyncWebRequestWrapper_);
        });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response_ = request->beginResponse_P(200, "image/x-icon", favicon_ico, sizeof(favicon_ico));
        request->send(response_);
        });

    server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(200, "image/png", favicon_ico, sizeof(favicon_ico));
        request->send(response);
        });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });
    server.on("/post", HTTP_ANY, [](AsyncWebServerRequest* request) { handlePost(request); });
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) { handleReboot(request); });
    
    server.onNotFound([](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper_(request);
        iotWebConf.handleNotFound(&asyncWebRequestWrapper_);
        });

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
        gShouldReboot = true;
    }
    
    if (gShouldReboot) {
        Serial.println(F("Rebooting..."));
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
			json_ += ",\"sensor" + String(i_) + "_max\":" + String(sensor_->GetMaxTemperature(), 2);
		}
		sensor_ = (Sensor*)sensor_->getNext();
		i_++;
	}
	json_ += "}";
	request->send(200, "application/json", json_);
}

class MyHtmlRootFormatProvider : public TabHtmlRootFormatProvider {
protected:
    virtual String getScriptInner() override {
        String s_ = TabHtmlRootFormatProvider::getScriptInner();
        s_.replace("{millisecond}", "5000");
        
        s_ += F("function updateData(jsonData) {\n");
        s_ += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
        
        Sensor* sensor_ = &Sensor1;
        uint8_t i_ = 1;
        while (sensor_ != nullptr) {
            if (sensor_->isActive()) {
                s_ += "   document.getElementById('sensor" + String(i_) + "').innerHTML = jsonData.sensor" + String(i_) + " + \"&deg;C\" \n";
                s_ += "   document.getElementById('sensor" + String(i_) + "_max').innerHTML = jsonData.sensor" + String(i_) + "_max + \"&deg;C (\" + jsonData.sensor" + String(i_) + "_max_time + \")\" \n";
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

    AsyncResponseStream* response_ = request->beginResponseStream("text/html", 1024);
    MyHtmlRootFormatProvider fp_;

    // === 3 Tabs erstellen ===
    fp_.addTab("Current", "current");
    fp_.addTab("Alerts", "alerts");
    fp_.addTab("System", "system");

    // === TAB 1: Current Values (RSSI + Temperatures) ===
    String currentTab_ = "";
    currentTab_ += F("<fieldset align=left style=\"border: 1px solid\"><table border=\"0\" align=\"center\" width=\"100%\">");
    currentTab_ += F("<tr><td align=\"left\">RSSI:</td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr></table></fieldset>");
    
    currentTab_ += fp_.getHtmlFieldset("Current Temperature");
    currentTab_ += fp_.getHtmlTable();
    Sensor* sensor_ = &Sensor1;
    uint8_t i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            currentTab_ += fp_.getHtmlTableRowSpan(String(sensor_->GetLocationValue()) + ": ", "no data", "sensor" + String(i_));
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }
    currentTab_ += fp_.getHtmlTableEnd();
    currentTab_ += fp_.getHtmlFieldsetEnd();
    fp_.addString(currentTab_, "current");

    // === TAB 2: Alerts ===
    String alertsTab_ = "";
    alertsTab_ += fp_.getHtmlFieldset("Pending Alerts");
    alertsTab_ += fp_.getHtmlTable();

    bool hasAlerts_ = false;
    sensor_ = &Sensor1;
    i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            if (sensor_->Alert.isAlert()) {
                alertsTab_ += fp_.getHtmlTableRowText(
                    String("Sensor ") + String(i_) + " Alert:",
                    String(N2kEnumAlertTypeToStr(sensor_->Alert.GetAlertType())) + " - " +
                    String(N2kEnumAlertTypeToStr(sensor_->Alert.GetAlertState()))
                );
                hasAlerts_ = true;
            }
            if (sensor_->FaultAlert.isAlert()) {
                alertsTab_ += fp_.getHtmlTableRowText(
                    String("Sensor ") + String(i_) + " Fault:",
                    String(N2kEnumAlertTypeToStr(sensor_->FaultAlert.GetAlertType())) + " - " +
                    String(N2kEnumAlertTypeToStr(sensor_->FaultAlert.GetAlertState()))
                );
                hasAlerts_ = true;
            }
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }
    if (!hasAlerts_) {
        alertsTab_ += fp_.getHtmlTableRowText("Status:", "No pending alerts");
    }
    alertsTab_ += fp_.getHtmlTableEnd();
    alertsTab_ += fp_.getHtmlFieldsetEnd();
    fp_.addString(alertsTab_, "alerts");

    // === TAB 3: System (Max Values + Network + System Status) ===
    String systemTab_ = "";
    
    // Maximum Temperature
    systemTab_ += fp_.getHtmlFieldset("Maximum Temperature");
    systemTab_ += fp_.getHtmlTable();
    sensor_ = &Sensor1;
    i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            systemTab_ += fp_.getHtmlTableRowSpan(String(sensor_->GetLocationValue()) + " Max: ", "no data", "sensor" + String(i_) + "_max");
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }
    systemTab_ += fp_.getHtmlTableEnd();
    systemTab_ += fp_.getHtmlFieldsetEnd();

    // System Status
    systemTab_ += fp_.getHtmlFieldset("System Status");
    systemTab_ += fp_.getHtmlTable();
    systemTab_ += fp_.getHtmlTableRowSpan("Number of reboots:", String(RebootManager::getRebootCount()), "rebootCount");
    systemTab_ += fp_.getHtmlTableRowSpan("Last reboot reason:", RebootManager::getLastRebootReasonText(), "rebootReason");
    systemTab_ += fp_.getHtmlTableEnd();
    systemTab_ += fp_.getHtmlFieldsetEnd();

    // Network
    systemTab_ += fp_.getHtmlFieldset("Network");
    systemTab_ += fp_.getHtmlTable();
    systemTab_ += fp_.getHtmlTableRowText("MAC Address:", WiFi.macAddress());
    systemTab_ += fp_.getHtmlTableRowText("IP Address:", WiFi.localIP().toString());
    systemTab_ += fp_.getHtmlTableEnd();
    systemTab_ += fp_.getHtmlFieldsetEnd();

    fp_.addString(systemTab_, "system");

    // === HTML Ausgabe ===
    response_->print(fp_.getHtmlHead(iotWebConf.getThingName()));
    response_->print(F("<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"/apple-touch-icon.png\">\n"));
    response_->print(F("<link rel=\"apple-touch-icon\" sizes=\"96x96\" href=\"/apple-touch-icon.png\">\n"));
    response_->print(fp_.getHtmlStyle());
    response_->print(fp_.getHtmlHeadEnd());
    response_->print(fp_.getHtmlScript());
    response_->print(fp_.getHtmlTabs());

    // Footer
    response_->print(fp_.addNewLine(2));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowText("Go to <a href='config'>configure page</a> to change configuration."));
    response_->print(fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)));
    response_->print(fp_.getHtmlTableEnd());
    
    response_->print(fp_.getBodyEnd());
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

void handlePost(AsyncWebServerRequest* request) {
    if (request->hasParam("reset", true)) {
        Serial.println(F("Resetting to factory defaults..."));

        Config.applyDefaultValue();
        resetAllSensors();

        iotWebConf.saveConfig();

        request->send(200, "text/plain", "OK");
    }
    else {
        request->send(400, "text/plain", "Bad Request");
    }
}

void handleReboot(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response_ = request->beginResponse(200, "text/html",
        "<html>"
        "<head>"
        "<meta http-equiv=\"refresh\" content=\"15; url=/\">"
        "<title>Rebooting...</title>"
        "</head>"
        "<body>"
        "Please wait while the device is rebooting...<br>"
        "You will be redirected to the homepage shortly."
        "</body>"
        "</html>");
    request->client()->setNoDelay(true); // Disable Nagle's algorithm so the client gets the response immediately
    request->send(response_);
    gShouldReboot = true;
}
