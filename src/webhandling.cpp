#include <Arduino.h>
#include <ArduinoOTA.h>
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
// WebSerial High Performance Mode - sendet Nachrichten sofort
#define WSL_HIGH_PERF
#include <WebSerial.h>

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
void handleWebSerialCommand(uint8_t* data, size_t len);
void handleReboot(AsyncWebServerRequest* request);
void handleConfigSavedPage(iotwebconf::WebRequestWrapper* webRequestWrapper);
void convertParams();
void configSaved();
void wifiConnected();

bool gParamsChanged = true;
bool gSaveParams = false;
bool ShouldReboot = false;
uint8_t APModeOfflineTime = 0;

// WebSerial state tracking
static int previousConnectionCount = 0;
static unsigned long lastConnectionChange = 0;
static bool waitingToSendHistory = false;

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
        [](const char* updatePath) { AsyncUpdater.setup(&server, updatePath); },
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
    
    if (APModeOfflineTime > 0) {
        APModeTimer.start(APModeOfflineTime * 60 * 1000);
    }

    // WebSerial Initialisierung
    WebSerial.begin(&server, "/webserial");

    // Handler für eingehende Nachrichten
    WebSerial.onMessage(handleWebSerialCommand);

    // Callback für Live-Updates registrieren
    eventLogger.setLiveCallback([](const EventLogger::Event& event_) {
        WebSerial.println(eventLogger.formatEvent(event_));
        }
    );

    server.onNotFound([](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper_(request);
        iotWebConf.handleNotFound(&asyncWebRequestWrapper_);
        });

    eventLogger.log(F("Web server successfully initialized"));
}

void wifiLoop() {
    // -- doLoop should be called as frequently as possible.
    iotWebConf.doLoop();
    WebSerial.loop();

    // Connection Count prüfen
    int currentConnectionCount = WebSerial.getConnectionCount();

    // Verbindungsänderung erkannt
    if (currentConnectionCount != previousConnectionCount) {
        if (currentConnectionCount > 0) {
            // Neue Verbindung
            waitingToSendHistory = true;
            lastConnectionChange = millis();
        }
        previousConnectionCount = currentConnectionCount;
    }


    // Nach 500ms Historie senden
    if (waitingToSendHistory && currentConnectionCount > 0 && (millis() - lastConnectionChange) >= 500) {
        waitingToSendHistory = false;

        WebSerial.println();
        WebSerial.println("=== WebSerial ===");
        WebSerial.println("Type 'help' for available commands");
        WebSerial.println();

        auto events_ = eventLogger.getEvents();
        if (!events_.empty()) {
            WebSerial.println("=== Event History (" + String(events_.size()) + " events) ===");
            for (const auto& event_ : events_) {
                WebSerial.println(eventLogger.formatEvent(event_));
            }
            WebSerial.println();
        }
        WebSerial.println("=== Live Events ===");
        WebSerial.println();
    }
    
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
    
    if (AsyncUpdater.isFinished()) {
        eventLogger.log(F("Firmware update finished"));
        
        // Watchdog vor Reboot nur deaktivieren, wenn er aktiv ist
        esp_err_t wdt_status_ = esp_task_wdt_status(NULL);
        if (wdt_status_ == ESP_OK) {
            eventLogger.log(F("Disabling watchdog before reboot"));
            esp_task_wdt_deinit();
        }
        
        ShouldReboot = true;
    }
    
    if (ShouldReboot) {
        eventLogger.log(F("Rebooting..."));
        delay(1000);
        ESP.restart();
    }
}

void wifiConnected() {
    // ArduinoOTA konfigurieren
    ArduinoOTA.setHostname(thingName);
    
    // Optional: Passwort setzen (sollte mit IotWebConf-Passwort übereinstimmen)
    // ArduinoOTA.setPassword("admin");
    
    ArduinoOTA.onStart([]() {
        String type_;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type_ = "sketch";
        } else {  // U_SPIFFS
            type_ = "filesystem";
        }
        eventLogger.log("Start updating " + type_);
        
        // Watchdog deaktivieren während OTA
        // Zuerst alle Tasks vom Watchdog abmelden
        esp_err_t wdt_status_ = esp_task_wdt_status(NULL);
        if (wdt_status_ == ESP_OK) {
            // Task vom Watchdog abmelden (aktueller Task = loop Task)
            esp_task_wdt_delete(NULL);
            eventLogger.log(F("Current task unsubscribed from watchdog"));
        }
        
        // Auch loop2 Task vom Watchdog abmelden
        extern TaskHandle_t TaskHandle;
        if (TaskHandle != NULL) {
            esp_err_t loop2_status_ = esp_task_wdt_status(TaskHandle);
            if (loop2_status_ == ESP_OK) {
                esp_task_wdt_delete(TaskHandle);
                eventLogger.log(F("loop2 task unsubscribed from watchdog"));
            }
        }
        
        // Jetzt kann der Watchdog sicher deinitialisiert werden
        esp_task_wdt_deinit();
        eventLogger.log(F("Watchdog disabled for OTA"));
    });
    
    ArduinoOTA.onEnd([]() {
        eventLogger.log(F("End"));   
    });
    
    ArduinoOTA.onProgress([](unsigned int progress_, unsigned int total_) {
        eventLogger.logf("Progress: %u%%\r", (progress_ / (total_ / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error_) {
        eventLogger.logf("Error[%u]: ", error_);
        if (error_ == OTA_AUTH_ERROR) {
            eventLogger.log(F("Auth Failed"));
        } else if (error_ == OTA_BEGIN_ERROR) {
            eventLogger.log(F("Begin Failed")); 
        } else if (error_ == OTA_CONNECT_ERROR) {
            eventLogger.log(F("Connect Failed"));
        } else if (error_ == OTA_RECEIVE_ERROR) {
            eventLogger.log(F("Receive Failed"));
        } else if (error_ == OTA_END_ERROR) {
            eventLogger.log(F("End Failed"));
        }
        
        // Watchdog wieder aktivieren bei Fehler
        eventLogger.log(F("Re-enabling watchdog after error"));
        esp_task_wdt_config_t wdt_config_ = {
            .timeout_ms = 30000,
            .idle_core_mask = 0,
            .trigger_panic = true
        };
        esp_task_wdt_init(&wdt_config_);
        
        // Aktuelle Tasks wieder anmelden
        esp_task_wdt_add(NULL);
        
        extern TaskHandle_t TaskHandle;
        if (TaskHandle != NULL) {
            esp_task_wdt_add(TaskHandle);
        }
        eventLogger.log(F("Watchdog re-enabled"));
    });
    
    ArduinoOTA.begin();
    eventLogger.log(F("ArduinoOTA started"));
    eventLogger.log("IP address: " + WiFi.localIP().toString());
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
    response_->print(fp_.getHtmlTableRowText("Open <a href='/config'>configuration page</a>.", ""));
    response_->print(fp_.getHtmlTableRowText("Open <a href='/webserial' target='_blank'>WebSerial Console</a> for live logs.", ""));
    response_->print(fp_.getHtmlTableRowText("<div style='font-size: .6em;'>Version '" + String(Version) + "'</div>", ""));
    response_->print(fp_.getHtmlTableEnd());

    response_->print(fp_.getHtmlEnd());
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
    else if (request->hasParam("resetmax", true)) {
        eventLogger.log(F("Resetting maximum temperature values"));

        Sensor* sensor_ = &Sensor1;
        while (sensor_ != nullptr) {
            sensor_->ResetMaxTemperature();
            sensor_ = sensor_->getNext();
        }

        request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("resetalerts", true)) {
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
    }
    else {
        request->send(400, "text/plain", "Bad Request");
    }
}

void handleWebSerialCommand(uint8_t* data, size_t len) {

    // Kommando in String umwandeln
    String command_ = "";
    for (size_t i_ = 0; i_ < len; i_++) {
        command_ += (char)data[i_];
    }
    command_.trim();

    // Leere Kommandos ignorieren
    if (command_.length() == 0) {
        return;
    }

    eventLogger.logf("WebSerial: %s", command_.c_str());

    // Kommandos verarbeiten
    if (command_ == "help") {
        WebSerial.println("=== Available Commands ===");
        WebSerial.println("  help     - Show this help");
        WebSerial.println("  reboot   - Reboot the device");
        WebSerial.println("  clear    - Clear event buffer");
        WebSerial.println("  status   - Show system status");
        WebSerial.println("  history  - Show all buffered events");
		WebSerial.println("  verbose  - Toggle verbose logging (On/Off)");
    }
    else if (command_ == "reboot") {
        WebSerial.println(">>> Rebooting device...");
        eventLogger.log("Reboot requested via WebSerial");
        delay(100);
        ShouldReboot = true;
    }
    else if (command_ == "clear") {
        eventLogger.clear();
        WebSerial.println(">>> Event buffer cleared");
    }
    else if (command_ == "status") {
        WebSerial.println("=== System Status ===");
        WebSerial.println("Version: " + String(Version));
        WebSerial.println("WiFi SSID: " + String(WiFi.SSID()));
        WebSerial.println("WiFi RSSI: " + String(WiFi.RSSI()) + " dBm");
        WebSerial.println("IP Address: " + WiFi.localIP().toString());

        unsigned long uptime_ = millis() / 1000;
        unsigned long days_ = uptime_ / 86400;
        unsigned long hours_ = (uptime_ % 86400) / 3600;
        unsigned long minutes_ = (uptime_ % 3600) / 60;
        unsigned long seconds_ = uptime_ % 60;

        WebSerial.printf("Uptime: %lud %02lu:%02lu:%02lu", days_, hours_, minutes_, seconds_);
        WebSerial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
        WebSerial.println("Events in buffer: " + String(eventLogger.count()) + "/" + String(EVENT_BUFFER_SIZE));
    }
    else if (command_ == "history") {
        auto events_ = eventLogger.getEvents();
        WebSerial.println("=== Event History (" + String(events_.size()) + " events) ===");
        if (events_.empty()) {
            WebSerial.println("(No events in buffer)");
        }
        else {
            for (const auto& event_ : events_) {
                WebSerial.println(eventLogger.formatEvent(event_));
            }
        }
    }
    else if (command_ == "verbose") {
		debugMode = !debugMode;
        WebSerial.println(">>> Verbose logging is now " + String(debugMode ? "enabled" : "disabled"));
	}
    else {
        WebSerial.println(">>> Unknown command: " + command_);
        WebSerial.println(">>> Type 'help' for available commands");
    }

	WebSerial.flush();
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
