#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "webhandling.h"
#include "favicon.h"
#include "neotimer.h"

#include <DNSServer.h>
#include <IotWebConfAsyncTab.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRootTab.h>
#include <RebootManager.h>
#include <N2kAlertTypes.h>

#include <EventLogger.h>

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
void handleWebCommand(const String& command);
void handleReboot(AsyncWebServerRequest* request);
void handleConfigSavedPage(iotwebconf::WebRequestWrapper* webRequestWrapper);
void convertParams();
void configSaved();
void wifiConnected();

bool gParamsChanged = true;
bool gSaveParams = false;
bool ShouldReboot = false;
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
        sensor_->ResetMaxTemperature();
        sensor_ = sensor_->getNext();
    }
}

void wifiInit() {
    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    // === TABS KONFIGURIEREN ===
    // System Tab soll an letzter Stelle sein
    iotWebConf.setSystemTabPosition(-1);  // -1 = letzte Position
    iotWebConf.setSystemTabName("System");

    iotWebConf.addParameterGroup(&Sensor1, "Sensors");
    Sensor1.setNext(&Sensor2);
    iotWebConf.addParameterGroup(&Sensor2, "Sensors");
    
    Sensor2.setNext(&Sensor3);
    iotWebConf.addParameterGroup(&Sensor3, "Sensors");
    
    Sensor3.setNext(&Sensor4);
    iotWebConf.addParameterGroup(&Sensor4, "Sensors");

    iotWebConf.addParameterGroup(&Config, "NMEA");

    iotWebConf.addSystemParameter(&APModeOfflineParam);

    // CustomHtmlFormatProvider mit Tabs-Vektor erstellen
    auto customProvider = new CustomHtmlFormatProvider(iotWebConf.getTabsVector());
    customProvider->setContainerWidth(700, 700);
    iotWebConf.setHtmlFormatProvider(customProvider);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) {
            AsyncUpdater.onBegin([](const String& message) {
                LOG_INFO("%s", message.c_str());
                });
            AsyncUpdater.onEnd([](const String& message) { LOG_OK("%s", message.c_str()); });
            AsyncUpdater.OnError([](const String& message) {
                LOG_ERROR("%s", message.c_str());
                });
            AsyncUpdater.onProgress([](const String& message) { LOG_INFO("%s", message.c_str()); });
            AsyncUpdater.onLog([](const String& message) { WEB_LOG_INFO("%s", message.c_str()); });
            AsyncUpdater.setup(&server, updatePath);
        },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);
    iotWebConf.setConfigSavedPage(&handleConfigSavedPage);

    iotWebConf.getApTimeoutParameter()->visible = true;

    iotWebConf.init();

    convertParams();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { handleRoot(request); });

    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* asyncWebRequestWrapper_ = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(asyncWebRequestWrapper_);
        });

    IconHandler::registerHandlers(&server, favicon_ico, sizeof(favicon_ico));

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });
    server.on("/post", HTTP_ANY, [](AsyncWebServerRequest* request) { handlePost(request); });
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) { handleReboot(request); });

      // EventLogger Web initialisieren
    bool webOk_ = eventLogger.beginWeb(&server, "/events");
    if (!webOk_) {
        eventLogger.log("EventLogger.beginWeb failed", EventLogger::Status::ERROR, 1);
    }

    eventLogger.setCommandCallback([](const String& command_) {
        handleWebCommand(command_);
        });

    server.onNotFound([](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper_(request);
        iotWebConf.handleNotFound(&asyncWebRequestWrapper_);
        });
    
    if (APModeOfflineTime > 0) {
        APModeTimer.start(APModeOfflineTime * 60 * 1000);
    }
}

void wifiLoop() {
    iotWebConf.doLoop();
   
    if (gSaveParams) {
        eventLogger.log(F("Parameters are changed,save them"));

        iotWebConf.saveConfig();
        gSaveParams = false;
    }

    if (APModeTimer.done()) {
        eventLogger.log(F("AP mode offline time reached"));
        iotWebConf.goOffLine();
        APModeTimer.stop();
    }
    
    if (ShouldReboot || AsyncUpdater.isFinished()) {
        LOG_DEBUG("Rebooting the device...");
        delay(1000);
        ESP.restart();
    }
}

void wifiConnected() {
    WiFi.setSleep(false);
    LOG_INFO("WiFi modem sleep disabled");  
}

void handleData(AsyncWebServerRequest* request) {
    String json_ = "{";
    json_ += "\"rssi\":" + String(WiFi.RSSI());
    Sensor* sensor_ = &Sensor1;
    uint8_t i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            json_ += ",\"sensor" + String(i_) + "\":\"" + String(sensor_->GetSensorValue(), 1) + "\"";
            json_ += ",\"sensor" + String(i_) + "_max\":\"" + String(sensor_->GetMaxTemperature(), 1) + "\"";

            time_t maxTime_ = sensor_->GetMaxTemperatureTime();
            if (maxTime_ > 0) {
                struct tm timeinfo_;
                localtime_r(&maxTime_, &timeinfo_);
                char timeStr_[25];
                strftime(timeStr_, sizeof(timeStr_), "%d.%m.%Y %H:%M UTC", &timeinfo_);
                json_ += ",\"sensor" + String(i_) + "_max_time\":\"" + String(timeStr_) + "\"";
            }
            else {
                json_ += ",\"sensor" + String(i_) + "_max_time\":\"--\"";
            }
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
                s_ += "   var elem" + String(i_) + " = document.getElementById('sensor" + String(i_) + "'); \n";
                s_ += "   if(elem" + String(i_) + ") elem" + String(i_) + ".innerHTML = jsonData.sensor" + String(i_) + " + \"&deg;C\" \n";
                s_ += "   var elemMax" + String(i_) + " = document.getElementById('sensor" + String(i_) + "_max'); \n";
                s_ += "   if(elemMax" + String(i_) + ") elemMax" + String(i_) + ".innerHTML = jsonData.sensor" + String(i_) + "_max + \"&deg;C (\" + jsonData.sensor" + String(i_) + "_max_time + \")\" \n";
            }
            sensor_ = (Sensor*)sensor_->getNext();
            i_++;
        }
        s_ += F("}\n");
        
        s_ += F("function resetMaxValues() {\n");
        s_ += F("   if(!confirm('Reset all maximum temperature values?')) return;\n");
        s_ += F("   fetch('/post', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'resetmax=true'})\n");
        s_ += F("   .then(response => {\n");
        s_ += F("       if(response.ok) { \n");
        s_ += F("           alert('Max values reset successfully');\n");
        s_ += F("           requestData();\n");
        s_ += F("       } else {\n");
        s_ += F("           alert('Reset failed!');\n");
        s_ += F("       }\n");
        s_ += F("   })\n");
        s_ += F("   .catch(error => {\n");
        s_ += F("       console.error('Error:', error);\n");
        s_ += F("       alert('Reset failed!');\n");
        s_ += F("   });\n");
        s_ += F("}\n");
        
        s_ += F("function resetAlerts() {\n");
        s_ += F("   if(!confirm('Reset all pending alerts?')) return;\n");
        s_ += F("   fetch('/post', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'resetalerts=true'})\n");
        s_ += F("   .then(response => {\n");
        s_ += F("       if(response.ok) { \n");
        s_ += F("           alert('Alerts reset successfully');\n");
        s_ += F("           location.reload();\n");
        s_ += F("       } else {\n");
        s_ += F("           alert('Reset failed!');\n");
        s_ += F("       }\n");
        s_ += F("   })\n");
        s_ += F("   .catch(error => {\n");
        s_ += F("       console.error('Error:', error);\n");
        s_ += F("       alert('Reset failed!');\n");
        s_ += F("   });\n");
        s_ += F("}\n");
        
        return s_;
    }
};

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    AsyncResponseStream* response_ = request->beginResponseStream("text/html");
    MyHtmlRootFormatProvider fp_;

    // === 3 Tabs erstellen ===
    fp_.addTab("Current", "current");
    fp_.addTab("Alerts", "alerts");
    fp_.addTab("System", "system");

    // Header
    response_->print(fp_.getHtmlHead(iotWebConf.getThingName()));
    response_->print(fp_.getIconLinks());
    response_->print(fp_.getHtmlStyle());
    response_->print(fp_.getHtmlHeadEnd());
    response_->print(fp_.getHtmlScript());
    response_->print(fp_.getHtmlTabButtons());

    // === TAB 1: Current Values
    response_->print(fp_.getHtmlTabContentStart("current"));
    response_->print(F("<fieldset align=left style=\"border: 1px solid\"><table border=\"0\" align=\"center\" width=\"100%\">"));
    response_->print(F("<tr><td align=\"left\">RSSI:</td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr></table></fieldset>"));

    response_->print(fp_.getHtmlFieldset("Current Temperature"));
    response_->print(fp_.getHtmlTable());

    Sensor* sensor_ = &Sensor1;
    uint8_t i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            response_->print(fp_.getHtmlTableRowSpan(String(sensor_->GetLocationValue()) + ": ", "no data", "sensor" + String(i_)));
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }

    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlTabContentEnd());

    // === TAB 2: Alerts ===
    response_->print(fp_.getHtmlTabContentStart("alerts"));
    response_->print(fp_.getHtmlFieldset("Pending Alerts"));
    response_->print(fp_.getHtmlTable());

    bool hasAlerts_ = false;
    sensor_ = &Sensor1;
    i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            if (sensor_->Alert.isAlert()) {
                response_->print(fp_.getHtmlTableRowText(
                    String("Sensor ") + String(i_) + " Alert:",
                    String(N2kEnumAlertTypeToStr(sensor_->Alert.GetAlertType())) + " - " +
                    String(N2kEnumAlertTypeToStr(sensor_->Alert.GetAlertState()))
                ));
                hasAlerts_ = true;
            }
            if (sensor_->FaultAlert.isAlert()) {
                response_->print(fp_.getHtmlTableRowText(
                    String("Sensor ") + String(i_) + " Fault:",
                    String(N2kEnumAlertTypeToStr(sensor_->FaultAlert.GetAlertType())) + " - " +
                    String(N2kEnumAlertTypeToStr(sensor_->FaultAlert.GetAlertState()))
                ));
                hasAlerts_ = true;
            }
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }

    if (!hasAlerts_) {
        response_->print(fp_.getHtmlTableRowText("Status:", "No pending alerts"));
    }

    response_->print(fp_.getHtmlTableEnd());
    
    // Reset Alerts Button
    if (hasAlerts_) {
        response_->print(F("<div style='text-align:center;margin-top:10px;'>"));
        response_->print(F("<button onclick='resetAlerts()' style='width:auto;padding:10px 20px;'>Reset All Alerts</button>"));
        response_->print(F("</div>"));
    }
    
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlTabContentEnd());

    // === TAB 3: System ===
    response_->print(fp_.getHtmlTabContentStart("system"));

    // Maximum Temperature
    response_->print(fp_.getHtmlFieldset("Maximum Temperature"));
    response_->print(fp_.getHtmlTable());

    sensor_ = &Sensor1;
    i_ = 1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            response_->print(fp_.getHtmlTableRowSpan(String(sensor_->GetLocationValue()) + " Max: ", "no data", "sensor" + String(i_) + "_max"));
        }
        sensor_ = (Sensor*)sensor_->getNext();
        i_++;
    }

    response_->print(fp_.getHtmlTableEnd());

    // Reset Button
    response_->print(F("<div style='text-align:center;margin-top:10px;'>"));
    response_->print(F("<button onclick='resetMaxValues()' style='width:auto;padding:10px 20px;'>Reset Max Values</button>"));
    response_->print(F("</div>"));

    response_->print(fp_.getHtmlFieldsetEnd());

    // System Status
    response_->print(fp_.getHtmlFieldset("System Status"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Number of reboots:", String(RebootManager::getRebootCount()), "rebootCount"));
    response_->print(fp_.getHtmlTableRowSpan("Last reboot reason:", RebootManager::getLastRebootReasonText(), "rebootReason"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());

    // Network
    response_->print(fp_.getHtmlFieldset("Network"));
    response_->print(fp_.getHtmlTable());

    uint64_t chipid_ = ESP.getEfuseMac();
    char macStr_[18];
    snprintf(macStr_, sizeof(macStr_), "%02X:%02X:%02X:%02X:%02X:%02X",
        (uint8_t)(chipid_ >> 40), (uint8_t)(chipid_ >> 32),
        (uint8_t)(chipid_ >> 24), (uint8_t)(chipid_ >> 16),
        (uint8_t)(chipid_ >> 8), (uint8_t)(chipid_));
    response_->print(fp_.getHtmlTableRowText("MAC Address:", String(macStr_)));

    String ipAddress_;
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        ipAddress_ = WiFi.softAPIP().toString();
    }
    else {
        ipAddress_ = WiFi.localIP().toString();
    }
    if (ipAddress_ == "0.0.0.0" || ipAddress_.isEmpty()) {
        ipAddress_ = "Not connected";
    }
    response_->print(fp_.getHtmlTableRowText("IP Address:", ipAddress_));

    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());

    response_->print(fp_.getHtmlTabContentEnd());

    // Footer
    response_->print(fp_.addNewLine(2));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration."));
    response_->print(fp_.getHtmlTableRowText("Go to <a href = 'events'>event log</a> to view the event history."));
    response_->print(fp_.getHtmlTableRowText("<div style='font-size: .6em;'>Version '" + String(Version) + "'</div>", ""));
    response_->print(fp_.getHtmlTableEnd());

    response_->print(fp_.getHtmlEnd());
    request->send(response_);
}

void convertParams() {
    gN2KInstance = Config.GetInstance();
    gN2KSID = Config.GetSID();
    APModeOfflineTime = atoi(APModeOfflineValue);
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}

void handlePost(AsyncWebServerRequest* request) {
    if (request->hasParam("reset", true)) {
        Config.applyDefaultValue();
        resetAllSensors();

        iotWebConf.saveConfig();
        LOG_OK("Factory defaults applied");

        request->send(200, "text/plain", "OK");
        return;
    }

    if (request->hasParam("resetmax", true)) {

        Sensor* sensor_ = &Sensor1;
        while (sensor_ != nullptr) {
            sensor_->ResetMaxTemperature();
            sensor_ = sensor_->getNext();
        }
        LOG_OK("Maximum temperature values reset");

        request->send(200, "text/plain", "OK");
        return;
    }
    
    if (request->hasParam("resetalerts", true)) {
        eventLogger.log(F("Resetting all alerts"));

        Sensor* sensor_ = &Sensor1;
        while (sensor_ != nullptr) {
            if (sensor_->isActive()) {
                sensor_->Alert.ResetAlert();
                sensor_->FaultAlert.ResetAlert();
            }
            sensor_ = sensor_->getNext();
        }

        request->send(200, "text/plain", "OK");
        return;
    }

    request->send(400, "text/plain", "Bad Request");
}

void handleWebCommand(const String& command) {
    String cmd_ = command;
    cmd_.trim();
    cmd_.toLowerCase();

    if (cmd_.length() == 0) {
        return;
    }

    eventLogger.logf("Web command: %s", cmd_.c_str());

    if (cmd_ == "help") {
        WEB_LOG_INFO_AT(0, "=== Available Commands ===");
        WEB_LOG_INFO_AT(1, "help     - Show this help");
        WEB_LOG_INFO_AT(1, "reboot   - Reboot the device");
        WEB_LOG_INFO_AT(1, "clear    - Clear event buffer");
        WEB_LOG_INFO_AT(1, "status   - Show system status");
        WEB_LOG_INFO_AT(1, "history  - Show all buffered events");
        WEB_LOG_INFO_AT(1, "verbose  - Toggle verbose logging (On/Off)");
        return;
    }

    if (cmd_ == "reboot") {
        WEB_LOG_INFO_AT(0, "Rebooting device...");
        eventLogger.log("Reboot requested via web command");
        delay(100);
        ShouldReboot = true;
        return;
    }

    if (cmd_ == "clear") {
        eventLogger.clear();
        WEB_LOG_INFO_AT(0, "Event buffer cleared");
        return;
    }

    if (cmd_ == "status") {
        WEB_LOG_INFO_AT(0, "=== System Status ===");
        WEB_LOG_INFO_AT(1, "Version: %s", Version);
        WEB_LOG_INFO_AT(1, "WiFi SSID: %s", WiFi.SSID().c_str());
        WEB_LOG_INFO_AT(1, "WiFi RSSI: %d dBm", WiFi.RSSI());
        WEB_LOG_INFO_AT(1, "IP Address: %s", WiFi.localIP().toString().c_str());

        unsigned long uptime_ = millis() / 1000;
        unsigned long days_ = uptime_ / 86400;
        unsigned long hours_ = (uptime_ % 86400) / 3600;
        unsigned long minutes_ = (uptime_ % 3600) / 60;
        unsigned long seconds_ = uptime_ % 60;

        WEB_LOG_INFO_AT(1, "Uptime: %lud %02lu:%02lu:%02lu", days_, hours_, minutes_, seconds_);
        WEB_LOG_INFO_AT(1, "Free Heap: %u bytes", ESP.getFreeHeap());
        WEB_LOG_INFO_AT(1, "Events in buffer: %u/%u",
            static_cast<unsigned int>(eventLogger.count()), static_cast<unsigned int>(EVENT_BUFFER_SIZE));
        return;
    }

    if (cmd_ == "history") {
        auto events_ = eventLogger.getEvents();
        WEB_LOG_INFO_AT(0, "=== Event History (%u events) ===", static_cast<unsigned int>(events_.size()));
        if (events_.empty()) {
            WEB_LOG_INFO_AT(1, "(No events in buffer)");
        }
        else {
            for (const auto& event_ : events_) {
                String formattedEvent_ = eventLogger.formatEvent(event_);
                WEB_LOG_INFO_AT(1, "%s", formattedEvent_.c_str());
            }
        }
        return;
    }

    if (cmd_ == "verbose") {
        debugMode = !debugMode;
        WEB_LOG_INFO_AT(0, "Verbose logging is now %s", debugMode ? "enabled" : "disabled");
        return;
    }

    WEB_LOG_INFO_AT(0, "Unknown command: %s", cmd_.c_str());
    WEB_LOG_INFO_AT(0, "Type 'help' for available commands");
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
    ShouldReboot = true;
}

void handleConfigSavedPage(iotwebconf::WebRequestWrapper* webRequestWrapper) {
    webRequestWrapper->sendHeader("Location", "/");
    webRequestWrapper->send(302, "text/plain", "");
}
