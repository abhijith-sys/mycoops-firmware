#include "GrowNetworkManager.h"
#include "Config.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

static const byte DNS_PORT = 53;

static String jsonEscape(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (unsigned i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n' || c == '\r') {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

GrowNetworkManager::GrowNetworkManager()
    : _server(80),
      _setupMode(false),
      _reconnectInProgress(false),
      _handlersRegistered(false),
      _mqttTestOk(false),
      _setupStep(1),
      _lastReconnectAttempt(0),
      _lastSetupStatusPrint(0),
      _failedAttempts(0) {
}

void GrowNetworkManager::begin() {
    ProvisioningStore::load(_cfg);

    if (!_cfg.hasWifi()) {
        Serial.println(F("No WiFi credentials — Setup Mode step 1 (WiFi)"));
        enterSetupMode(1);
        return;
    }

    Serial.print(F("Connecting to WiFi SSID: "));
    Serial.println(_cfg.wifiSsid);

    if (connectSta(WIFI_CONNECT_TIMEOUT_MS, false)) {
        Serial.print(F("WiFi connected, IP: "));
        Serial.println(WiFi.localIP());
        _failedAttempts = 0;

        if (!_cfg.hasMqtt()) {
            Serial.println(F("No MQTT host — Setup Mode step 2 (MQTT)"));
            enterSetupMode(2);
        }
        return;
    }

    _failedAttempts = 1;
    Serial.println(F("WiFi connect timed out — will keep retrying in background"));
    if (_failedAttempts >= WIFI_MAX_FAILED_ATTEMPTS) {
        enterSetupMode(1);
    }
}

void GrowNetworkManager::loop() {
    if (_setupMode) {
        _dns.processNextRequest();
        _server.handleClient();

        unsigned long now = millis();
        if (now - _lastSetupStatusPrint >= 5000) {
            _lastSetupStatusPrint = now;
            Serial.println();
            Serial.println(F("======== SETUP MODE ========"));
            Serial.print(F("Step: "));
            Serial.println(_setupStep == 1 ? F("1 WiFi") : F("2 MQTT"));
            Serial.print(F("SoftAP: "));
            Serial.println(_apSsid);
            Serial.println(F("Portal: http://192.168.4.1"));
            Serial.print(F("SoftAP clients: "));
            Serial.println(WiFi.softAPgetStationNum());
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print(F("STA IP: "));
                Serial.println(WiFi.localIP());
            }
            Serial.println(F("============================"));
        }
        return;
    }

    if (isConnected()) {
        _failedAttempts = 0;
        return;
    }
    tryReconnect();
}

bool GrowNetworkManager::isConnected() {
    return !_setupMode && WiFi.status() == WL_CONNECTED;
}

bool GrowNetworkManager::isSetupMode() {
    return _setupMode;
}

String GrowNetworkManager::setupApSsid() const {
    return _setupMode ? _apSsid : String();
}

int GrowNetworkManager::softApClientCount() const {
    return _setupMode ? (int)WiFi.softAPgetStationNum() : 0;
}

bool GrowNetworkManager::connectSta(unsigned long timeoutMs, bool keepAp) {
    if (keepAp) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
    }
    WiFi.begin(_cfg.wifiSsid.c_str(), _cfg.wifiPassword.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

void GrowNetworkManager::tryReconnect() {
    unsigned long now = millis();

    if (_reconnectInProgress) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(F("WiFi reconnected, IP: "));
            Serial.println(WiFi.localIP());
            _reconnectInProgress = false;
            _failedAttempts      = 0;
            return;
        }
        if (now - _lastReconnectAttempt < WIFI_CONNECT_TIMEOUT_MS) {
            return;
        }
        _reconnectInProgress = false;
        _failedAttempts++;
        Serial.print(F("WiFi reconnect failed (attempt "));
        Serial.print(_failedAttempts);
        Serial.print('/');
        Serial.print(WIFI_MAX_FAILED_ATTEMPTS);
        Serial.println(')');
        if (_failedAttempts >= WIFI_MAX_FAILED_ATTEMPTS) {
            enterSetupMode(1);
        }
        return;
    }

    if (now - _lastReconnectAttempt < WIFI_RETRY_INTERVAL_MS) {
        return;
    }

    _lastReconnectAttempt = now;
    _reconnectInProgress  = true;
    Serial.println(F("WiFi reconnect attempt..."));
    WiFi.disconnect();
    WiFi.begin(_cfg.wifiSsid.c_str(), _cfg.wifiPassword.c_str());
}

String GrowNetworkManager::softApSsid() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[24];
    snprintf(buf, sizeof(buf), "%s%02X%02X", WIFI_SETUP_AP_PREFIX, mac[4], mac[5]);
    return String(buf);
}

void GrowNetworkManager::enterSetupMode(int step) {
    _setupMode           = true;
    _setupStep           = step;
    _reconnectInProgress = false;
    _mqttTestOk          = false;
    _portalMessage       = "";
    _portalOkMessage     = "";
    bool keepSta = (step == 2 && WiFi.status() == WL_CONNECTED);
    startPortal(keepSta);
}

void GrowNetworkManager::startPortal(bool keepSta) {
    _server.stop();
    _dns.stop();

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    if (!keepSta) {
        WiFi.disconnect(false, true);
        delay(100);
    }

    _apSsid = softApSsid();
    IPAddress apIp(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, gateway, subnet);
    bool apOk = WiFi.softAP(_apSsid.c_str());
    delay(200);
    apIp = WiFi.softAPIP();
    _dns.start(DNS_PORT, "*", apIp);

    if (!_handlersRegistered) {
        _server.on("/", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/save-wifi", HTTP_POST, [this]() { handleSaveWifi(); });
        _server.on("/suggest", HTTP_GET, [this]() { handleSuggest(); });
        _server.on("/test-mqtt", HTTP_POST, [this]() { handleTestMqtt(); });
        _server.on("/test-backend", HTTP_POST, [this]() { handleTestBackend(); });
        _server.on("/save-mqtt", HTTP_POST, [this]() { handleSaveMqtt(); });
        _server.onNotFound([this]() { handleNotFound(); });
        _handlersRegistered = true;
    }
    _server.begin();
    _lastSetupStatusPrint = 0;

    Serial.println();
    Serial.println(F("=== Setup portal started ==="));
    Serial.print(F("SoftAP: "));
    Serial.println(apOk ? F("OK ") : F("FAIL "));
    Serial.println(_apSsid);
    Serial.print(F("http://"));
    Serial.println(apIp);
    Serial.println();
}

void GrowNetworkManager::stopPortal() {
    _server.stop();
    _dns.stop();
    WiFi.softAPdisconnect(true);
}

void GrowNetworkManager::handleRoot() {
    if (_setupStep == 2) {
        _server.send(200, "text/html", buildMqttPortalHtml());
    } else {
        _server.send(200, "text/html", buildWifiPortalHtml());
    }
}

void GrowNetworkManager::handleSaveWifi() {
    String ssid = _server.hasArg("ssid") ? _server.arg("ssid") : "";
    ssid.trim();
    String password = _server.hasArg("password") ? _server.arg("password") : "";

    if (ssid.length() == 0) {
        _portalMessage = "SSID is required.";
        _server.send(400, "text/html", buildWifiPortalHtml());
        return;
    }

    ProvisioningStore::saveWifi(ssid, password);
    _cfg.wifiSsid     = ssid;
    _cfg.wifiPassword = password;

    Serial.print(F("Saved WiFi SSID: "));
    Serial.println(ssid);
    Serial.println(F("Joining STA (SoftAP stays up)..."));

    // Keep SoftAP up; join STA on AP+STA. Single response after attempt.
    if (connectSta(WIFI_CONNECT_TIMEOUT_MS, true)) {
        Serial.print(F("STA OK, IP: "));
        Serial.println(WiFi.localIP());
        _setupStep       = 2;
        _portalMessage   = "";
        _portalOkMessage = "WiFi connected. Configure MQTT next.";
        _mqttTestOk      = false;
        _server.send(200, "text/html", buildMqttPortalHtml());
        return;
    }

    _portalMessage   = "Could not join \"" + ssid + "\". Check password and try again.";
    _portalOkMessage = "";
    _setupStep       = 1;
    _server.send(400, "text/html", buildWifiPortalHtml());
}

void GrowNetworkManager::handleSuggest() {
    String json = "{";
    bool sta = WiFi.status() == WL_CONNECTED;
    json += "\"sta_connected\":";
    json += sta ? "true" : "false";

    String staIp = sta ? WiFi.localIP().toString() : "";
    String gw    = sta ? WiFi.gatewayIP().toString() : "";
    json += ",\"sta_ip\":\"";
    json += staIp;
    json += "\",\"gateway_ip\":\"";
    json += gw;
    json += "\"";

    String suggested = gw;
    bool backendOk = false;
    String backendErr;
    if (sta && gw.length() > 0) {
        if (probeBackendHealth(gw, MQTT_BACKEND_HEALTH_PORT, backendErr)) {
            suggested = gw;
            backendOk = true;
        }
    }
    json += ",\"suggested_mqtt_host\":\"";
    json += suggested;
    json += "\",\"suggested_mqtt_port\":";
    json += String(MQTT_DEFAULT_PORT_LOCAL);
    json += ",\"suggested_backend_ok\":";
    json += backendOk ? "true" : "false";
    json += ",\"backend_health_port\":";
    json += String(MQTT_BACKEND_HEALTH_PORT);
    json += "}";
    _server.send(200, "application/json", json);
}

void GrowNetworkManager::handleTestMqtt() {
    String host = _server.hasArg("host") ? _server.arg("host") : "";
    host.trim();
    uint16_t port = (uint16_t)(_server.hasArg("port") ? _server.arg("port").toInt() : 0);
    String user = _server.hasArg("user") ? _server.arg("user") : "";
    String pass = _server.hasArg("pass") ? _server.arg("pass") : "";
    String mode = _server.hasArg("mode") ? _server.arg("mode") : "local";
    bool tls = (mode == "cloud") || (_server.hasArg("tls") && _server.arg("tls") == "1");

    if (host.length() == 0 || port == 0) {
        _mqttTestOk = false;
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"Host and port are required\"}");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        _mqttTestOk = false;
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"WiFi STA not connected — finish step 1 first\"}");
        return;
    }

    String err;
    bool ok = probeMqttConnect(host, port, user, pass, tls, err);
    _mqttTestOk = ok;
    if (ok) {
        _server.send(200, "application/json", "{\"ok\":true,\"error\":\"\"}");
    } else {
        String json = "{\"ok\":false,\"error\":\"";
        json += jsonEscape(err);
        json += "\"}";
        _server.send(200, "application/json", json);
    }
}

void GrowNetworkManager::handleTestBackend() {
    String host = _server.hasArg("host") ? _server.arg("host") : "";
    host.trim();
    uint16_t port = (uint16_t)(_server.hasArg("backend_port")
                                   ? _server.arg("backend_port").toInt()
                                   : MQTT_BACKEND_HEALTH_PORT);
    if (port == 0) {
        port = MQTT_BACKEND_HEALTH_PORT;
    }

    if (host.length() == 0) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"Host is required\"}");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"WiFi STA not connected\"}");
        return;
    }

    String err;
    bool ok = probeBackendHealth(host, port, err);
    if (ok) {
        _server.send(200, "application/json", "{\"ok\":true,\"error\":\"\"}");
    } else {
        String json = "{\"ok\":false,\"error\":\"";
        json += jsonEscape(err);
        json += "\"}";
        _server.send(200, "application/json", json);
    }
}

void GrowNetworkManager::handleSaveMqtt() {
    String host = _server.hasArg("host") ? _server.arg("host") : "";
    host.trim();
    uint16_t port = (uint16_t)(_server.hasArg("port") ? _server.arg("port").toInt() : 0);
    String user = _server.hasArg("user") ? _server.arg("user") : "";
    String pass = _server.hasArg("pass") ? _server.arg("pass") : "";
    String mode = _server.hasArg("mode") ? _server.arg("mode") : "local";
    bool tls = (mode == "cloud");

    if (host.length() == 0 || port == 0) {
        _portalMessage = "MQTT host and port are required.";
        _server.send(400, "text/html", buildMqttPortalHtml());
        return;
    }
    if (mode == "cloud" && user.length() == 0) {
        _portalMessage = "Cloud mode requires a username.";
        _server.send(400, "text/html", buildMqttPortalHtml());
        return;
    }

    // Re-run MQTT test on save if not already OK.
    String err;
    if (!_mqttTestOk) {
        if (!probeMqttConnect(host, port, user, pass, tls, err)) {
            _portalMessage = String("MQTT test failed: ") + err;
            _server.send(400, "text/html", buildMqttPortalHtml());
            return;
        }
        _mqttTestOk = true;
    }

    ProvisioningStore::saveMqtt(host, port, user, pass, tls, mode);
    _cfg.mqttHost = host;
    _cfg.mqttPort = port;
    _cfg.mqttUser = user;
    _cfg.mqttPass = pass;
    _cfg.mqttTls  = tls;
    _cfg.mqttMode = mode;

    _server.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GrowOS</title><style>body{font-family:sans-serif;margin:1.5rem;background:#111;color:#eee}</style>"
        "</head><body><h2>Saved</h2><p>MQTT OK. Device is restarting...</p></body></html>");
    delay(800);
    stopPortal();
    _setupMode = false;
    ESP.restart();
}

void GrowNetworkManager::handleNotFound() {
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    _server.send(302, "text/plain", "");
}

bool GrowNetworkManager::probeBackendHealth(const String &host, uint16_t port, String &errOut) {
    HTTPClient http;
    String url = "http://" + host + ":" + String(port) + "/health";
    http.setTimeout(5000);
    if (!http.begin(url)) {
        errOut = "Could not start HTTP client";
        return false;
    }
    int code = http.GET();
    http.end();
    if (code == 200) {
        errOut = "";
        return true;
    }
    if (code < 0) {
        errOut = String("HTTP error ") + HTTPClient::errorToString(code) + " — is backend on " +
                 host + ":" + String(port) + "?";
    } else {
        errOut = String("HTTP status ") + String(code) + " from " + url;
    }
    return false;
}

bool GrowNetworkManager::probeMqttConnect(const String &host, uint16_t port, const String &user,
                                          const String &pass, bool tls, String &errOut) {
    WiFiClient plain;
    WiFiClientSecure secure;
    PubSubClient mqtt;

    if (tls) {
        secure.setInsecure();
        secure.setTimeout(MQTT_TEST_TIMEOUT_MS / 1000);
        mqtt.setClient(secure);
    } else {
        plain.setTimeout(MQTT_TEST_TIMEOUT_MS);
        mqtt.setClient(plain);
    }
    mqtt.setServer(host.c_str(), port);
    mqtt.setSocketTimeout(MQTT_TEST_TIMEOUT_MS / 1000);

    String testId = String(MQTT_CLIENT_ID) + "-test";
    bool ok = false;
    if (user.length() > 0) {
        ok = mqtt.connect(testId.c_str(), user.c_str(), pass.c_str());
    } else {
        ok = mqtt.connect(testId.c_str());
    }

    if (ok) {
        mqtt.disconnect();
        errOut = "";
        return true;
    }
    errOut = String("MQTT connect failed to ") + host + ":" + String(port) +
             (tls ? " (TLS)" : "") + " — check host/port/firewall/credentials";
    return false;
}

String GrowNetworkManager::htmlEscape(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (unsigned i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"') {
            out += F("&quot;");
        } else if (c == '<') {
            out += F("&lt;");
        } else if (c == '&') {
            out += F("&amp;");
        } else if (c == '\n' || c == '\r') {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

String GrowNetworkManager::buildWifiPortalHtml() {
    int n = WiFi.scanNetworks();
    String html;
    html.reserve(3072 + (n > 0 ? n * 80 : 0));
    html += F(
        "<!DOCTYPE html><html><head>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GrowOS Setup — WiFi</title>"
        "<style>"
        "body{font-family:sans-serif;margin:1.2rem;background:#111;color:#eee}"
        "h1{font-size:1.25rem}label{display:block;margin:.75rem 0 .25rem;font-size:.85rem;color:#aaa}"
        "input,select,button{width:100%;box-sizing:border-box;padding:.6rem;border-radius:6px;"
        "border:1px solid #444;background:#1a1a1a;color:#eee;font-size:1rem}"
        "button{margin-top:1rem;background:#2d6a4f;border-color:#2d6a4f;font-weight:600}"
        ".msg{background:#3d1f1f;border:1px solid #833;padding:.75rem;border-radius:6px;margin-bottom:1rem}"
        ".ok{background:#1f3d2a;border:1px solid #385}.hint{font-size:.8rem;color:#888}"
        ".step{color:#8f8;font-size:.85rem;margin-bottom:.5rem}"
        "</style></head><body>"
        "<p class=step>Step 1 of 2 — WiFi</p>"
        "<h1>GrowOS WiFi Setup</h1>"
    );
    if (_portalMessage.length()) {
        html += F("<div class=msg>");
        html += _portalMessage;
        html += F("</div>");
    }
    if (_portalOkMessage.length()) {
        html += F("<div class=\"msg ok\">");
        html += _portalOkMessage;
        html += F("</div>");
    }
    html += F(
        "<form method=POST action=/save-wifi>"
        "<label for=scan>Nearby networks</label>"
        "<select id=scan onchange=\"document.getElementById('ssid').value=this.value\">"
        "<option value=\"\">— Select —</option>"
    );
    for (int i = 0; i < n; i++) {
        String name = WiFi.SSID(i);
        if (!name.length()) {
            continue;
        }
        html += F("<option value=\"");
        html += htmlEscape(name);
        html += F("\">");
        html += name;
        html += F(" (");
        html += String(WiFi.RSSI(i));
        html += F(" dBm)</option>");
    }
    html += F(
        "</select>"
        "<label for=ssid>SSID</label>"
        "<input id=ssid name=ssid type=text required placeholder=\"Network name\">"
        "<label for=password>Password</label>"
        "<input id=password name=password type=password>"
        "<button type=submit>Connect WiFi</button></form>"
        "<p class=hint>Device stays in setup after WiFi joins so you can configure MQTT next.</p>"
        "</body></html>"
    );
    WiFi.scanDelete();
    return html;
}

String GrowNetworkManager::buildMqttPortalHtml() {
    String html;
    html.reserve(4500);
    html += F(
        "<!DOCTYPE html><html><head>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GrowOS Setup — MQTT</title>"
        "<style>"
        "body{font-family:sans-serif;margin:1.2rem;background:#111;color:#eee}"
        "h1{font-size:1.25rem}label{display:block;margin:.75rem 0 .25rem;font-size:.85rem;color:#aaa}"
        "input,select,button{width:100%;box-sizing:border-box;padding:.6rem;border-radius:6px;"
        "border:1px solid #444;background:#1a1a1a;color:#eee;font-size:1rem;margin-bottom:.25rem}"
        "button{margin-top:.75rem;background:#2d6a4f;border-color:#2d6a4f;font-weight:600}"
        "button.secondary{background:#333;border-color:#555}"
        ".msg{background:#3d1f1f;border:1px solid #833;padding:.75rem;border-radius:6px;margin-bottom:1rem}"
        ".ok{background:#1f3d2a;border:1px solid #385}.hint{font-size:.8rem;color:#888}"
        ".step{color:#8f8;font-size:.85rem}.row{display:flex;gap:.5rem}.row>*{flex:1}"
        "#status{min-height:1.2rem;margin:.5rem 0;font-size:.9rem}"
        "</style></head><body>"
        "<p class=step>Step 2 of 2 — MQTT</p>"
        "<h1>Broker setup</h1>"
    );
    if (_portalMessage.length()) {
        html += F("<div class=msg>");
        html += _portalMessage;
        html += F("</div>");
    }
    if (_portalOkMessage.length()) {
        html += F("<div class=\"msg ok\">");
        html += _portalOkMessage;
        html += F("</div>");
    }

    String hostVal = _cfg.mqttHost;
    String portVal = _cfg.mqttPort ? String(_cfg.mqttPort) : String(MQTT_DEFAULT_PORT_LOCAL);
    String modeVal = _cfg.mqttMode.length() ? _cfg.mqttMode : "local";

    html += F("<div id=suggest class=hint>Loading suggestions...</div>");
    html += F("<div id=status></div>");
    html += F("<form id=mqttForm method=POST action=/save-mqtt>");
    html += F("<label for=mode>Mode</label><select id=mode name=mode>");
    html += (modeVal == "cloud") ? F("<option value=local>Local (LAN Mosquitto)</option>"
                                       "<option value=cloud selected>Cloud (TLS)</option>")
                                 : F("<option value=local selected>Local (LAN Mosquitto)</option>"
                                       "<option value=cloud>Cloud (TLS)</option>");
    html += F("</select>");
    html += F("<label for=host>MQTT host</label><input id=host name=host required value=\"");
    html += htmlEscape(hostVal);
    html += F("\" placeholder=\"192.168.x.x or broker.example.com\">");
    html += F("<label for=port>MQTT port</label><input id=port name=port type=number required value=\"");
    html += portVal;
    html += F("\">");
    html += F("<label for=user>Username (cloud / optional)</label>"
              "<input id=user name=user value=\"");
    html += htmlEscape(_cfg.mqttUser);
    html += F("\">");
    html += F("<label for=pass>Password</label>"
              "<input id=pass name=pass type=password value=\"");
    html += htmlEscape(_cfg.mqttPass);
    html += F("\">");
    html += F(
        "<div class=row>"
        "<button type=button class=secondary id=btnSuggest>Use suggested host</button>"
        "</div>"
        "<div class=row>"
        "<button type=button class=secondary id=btnTestMqtt>Test MQTT</button>"
        "<button type=button class=secondary id=btnTestBackend>Test backend</button>"
        "</div>"
        "<button type=submit>Save &amp; Finish</button>"
        "</form>"
        "<p class=hint>Local: use the PC running Docker/Mosquitto (same IP as the dashboard). "
        "Copy the suggested host from MycoMonitor before joining SoftAP if needed.</p>"
        "<script>"
        "const st=document.getElementById('status');"
        "function setStatus(ok,msg){st.style.color=ok?'#8f8':'#f88';st.textContent=msg;}"
        "function formBody(){const f=document.getElementById('mqttForm');"
        "return new URLSearchParams(new FormData(f)).toString();}"
        "async function loadSuggest(){"
        "try{const r=await fetch('/suggest');const j=await r.json();"
        "const el=document.getElementById('suggest');"
        "el.innerHTML='STA: '+(j.sta_ip||'—')+' | Gateway: '+(j.gateway_ip||'—')+"
        "'<br>Suggested MQTT: <b>'+(j.suggested_mqtt_host||'—')+':'+j.suggested_mqtt_port+'</b>'+"
        "' | Backend health: '+(j.suggested_backend_ok?'OK':'not detected');"
        "window.__suggest=j;"
        "if(!document.getElementById('host').value && j.suggested_mqtt_host){"
        "document.getElementById('host').value=j.suggested_mqtt_host;"
        "document.getElementById('port').value=j.suggested_mqtt_port;}"
        "}catch(e){document.getElementById('suggest').textContent='Suggest failed: '+e;}}"
        "document.getElementById('btnSuggest').onclick=()=>{"
        "const j=window.__suggest;if(!j||!j.suggested_mqtt_host){setStatus(false,'No suggestion yet');return;}"
        "document.getElementById('host').value=j.suggested_mqtt_host;"
        "document.getElementById('port').value=j.suggested_mqtt_port;"
        "document.getElementById('mode').value='local';setStatus(true,'Applied suggestion');};"
        "document.getElementById('mode').onchange=()=>{"
        "document.getElementById('port').value="
        "document.getElementById('mode').value==='cloud'?8883:1883;};"
        "document.getElementById('btnTestMqtt').onclick=async()=>{"
        "setStatus(true,'Testing MQTT...');"
        "try{const r=await fetch('/test-mqtt',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formBody()});"
        "const j=await r.json();setStatus(!!j.ok,j.ok?'MQTT OK':('MQTT failed: '+j.error));"
        "}catch(e){setStatus(false,String(e));}};"
        "document.getElementById('btnTestBackend').onclick=async()=>{"
        "setStatus(true,'Testing backend /health...');"
        "const host=document.getElementById('host').value;"
        "const body='host='+encodeURIComponent(host)+'&backend_port=4000';"
        "try{const r=await fetch('/test-backend',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body});"
        "const j=await r.json();setStatus(!!j.ok,j.ok?'Backend OK':('Backend failed: '+j.error));"
        "}catch(e){setStatus(false,String(e));}};"
        "loadSuggest();"
        "</script></body></html>"
    );
    return html;
}
