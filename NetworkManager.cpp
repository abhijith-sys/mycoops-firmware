#include "NetworkManager.h"
#include "Config.h"

static const char *PREFS_NAMESPACE = "wifi";
static const char *KEY_SSID        = "wifi_ssid";
static const char *KEY_PASSWORD    = "wifi_password";
static const byte  DNS_PORT        = 53;

NetworkManager::NetworkManager()
    : _server(80),
      _setupMode(false),
      _reconnectInProgress(false),
      _handlersRegistered(false),
      _lastReconnectAttempt(0),
      _failedAttempts(0) {
}

void NetworkManager::begin() {
    loadCredentials();

    if (!hasCredentials()) {
        Serial.println("No WiFi credentials in Preferences — entering Setup Mode");
        _portalMessage = "";
        enterSetupMode();
        return;
    }

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(_ssid);

    if (connectSta(WIFI_CONNECT_TIMEOUT_MS)) {
        Serial.print("WiFi connected, IP: ");
        Serial.println(WiFi.localIP());
        _failedAttempts = 0;
        return;
    }

    _failedAttempts = 1;
    Serial.println("WiFi connect timed out — will keep retrying in background");

    if (_failedAttempts >= WIFI_MAX_FAILED_ATTEMPTS) {
        _portalMessage = "";
        enterSetupMode();
    }
}

void NetworkManager::loop() {
    if (_setupMode) {
        _dns.processNextRequest();
        _server.handleClient();
        return;
    }

    if (isConnected()) {
        _failedAttempts = 0;
        return;
    }

    tryReconnect();
}

bool NetworkManager::isConnected() {
    return !_setupMode && WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::isSetupMode() {
    return _setupMode;
}

void NetworkManager::loadCredentials() {
    _prefs.begin(PREFS_NAMESPACE, true);
    _ssid     = _prefs.getString(KEY_SSID, "");
    _password = _prefs.getString(KEY_PASSWORD, "");
    _prefs.end();
}

void NetworkManager::saveCredentials(const String &ssid, const String &password) {
    _prefs.begin(PREFS_NAMESPACE, false);
    _prefs.putString(KEY_SSID, ssid);
    _prefs.putString(KEY_PASSWORD, password);
    _prefs.end();
    _ssid     = ssid;
    _password = password;
}

bool NetworkManager::hasCredentials() const {
    return _ssid.length() > 0;
}

bool NetworkManager::connectSta(unsigned long timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(250);
    }

    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::tryReconnect() {
    unsigned long now = millis();

    if (_reconnectInProgress) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("WiFi reconnected, IP: ");
            Serial.println(WiFi.localIP());
            _reconnectInProgress = false;
            _failedAttempts      = 0;
            return;
        }

        if (now - _lastReconnectAttempt < WIFI_CONNECT_TIMEOUT_MS) {
            return;  // still waiting
        }

        _reconnectInProgress = false;
        _failedAttempts++;
        Serial.print("WiFi reconnect failed (attempt ");
        Serial.print(_failedAttempts);
        Serial.print("/");
        Serial.print(WIFI_MAX_FAILED_ATTEMPTS);
        Serial.println(")");

        if (_failedAttempts >= WIFI_MAX_FAILED_ATTEMPTS) {
            Serial.println("Max WiFi failures reached — entering Setup Mode");
            _portalMessage = "";
            enterSetupMode();
        }
        return;
    }

    if (now - _lastReconnectAttempt < WIFI_RETRY_INTERVAL_MS) {
        return;
    }

    _lastReconnectAttempt = now;
    _reconnectInProgress  = true;
    Serial.println("WiFi reconnect attempt...");
    WiFi.disconnect();
    WiFi.begin(_ssid.c_str(), _password.c_str());
}

String NetworkManager::softApSsid() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[24];
    snprintf(buf, sizeof(buf), "%s%02X%02X", WIFI_SETUP_AP_PREFIX, mac[4], mac[5]);
    return String(buf);
}

void NetworkManager::enterSetupMode() {
    _setupMode           = true;
    _reconnectInProgress = false;
    startPortal();
}

void NetworkManager::startPortal() {
    stopPortal();

    // AP+STA so WiFi.scanNetworks() works while SoftAP is up.
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();

    String apSsid = softApSsid();
    WiFi.softAP(apSsid.c_str());
    delay(100);

    IPAddress apIp = WiFi.softAPIP();
    _dns.start(DNS_PORT, "*", apIp);

    if (!_handlersRegistered) {
        _server.on("/", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/save", HTTP_POST, [this]() { handleSave(); });
        _server.onNotFound([this]() { handleNotFound(); });
        _handlersRegistered = true;
    }
    _server.begin();

    Serial.println("=== WiFi Setup Mode ===");
    Serial.print("SoftAP SSID: ");
    Serial.println(apSsid);
    Serial.print("Portal URL:  http://");
    Serial.println(apIp);
}

void NetworkManager::stopPortal() {
    _server.stop();
    _dns.stop();
    WiFi.softAPdisconnect(true);
}

void NetworkManager::handleRoot() {
    _server.send(200, "text/html", buildPortalHtml());
}

void NetworkManager::handleSave() {
    if (!_server.hasArg("ssid")) {
        _portalMessage = "SSID is required.";
        _server.send(400, "text/html", buildPortalHtml());
        return;
    }

    String ssid = _server.arg("ssid");
    ssid.trim();
    String password = _server.hasArg("password") ? _server.arg("password") : "";

    if (ssid.length() == 0) {
        _portalMessage = "SSID is required.";
        _server.send(400, "text/html", buildPortalHtml());
        return;
    }

    saveCredentials(ssid, password);
    Serial.print("Saved credentials for SSID: ");
    Serial.println(ssid);

    _server.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GrowOS Setup</title>"
        "<style>body{font-family:sans-serif;margin:1.5rem;background:#111;color:#eee}"
        "p{line-height:1.5}</style></head><body>"
        "<h2>Connecting...</h2>"
        "<p>Trying <b>" + ssid + "</b>. Device will restart on success.</p>"
        "</body></html>");
    delay(500);

    stopPortal();
    _setupMode = false;

    if (connectSta(WIFI_CONNECT_TIMEOUT_MS)) {
        Serial.println("STA connect OK — restarting");
        delay(500);
        ESP.restart();
    }

    Serial.println("STA connect failed after save — returning to Setup Mode");
    _portalMessage = "Could not connect to \"" + ssid + "\". Check password and try again.";
    _failedAttempts = 0;
    enterSetupMode();
}

void NetworkManager::handleNotFound() {
    // Captive-portal probes + unknown paths → redirect to the config page.
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    _server.send(302, "text/plain", "");
}

String NetworkManager::buildPortalHtml() {
    int n = WiFi.scanNetworks();

    String html;
    html.reserve(2048 + (n > 0 ? n * 80 : 0));

    html += F(
        "<!DOCTYPE html><html><head>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GrowOS WiFi Setup</title>"
        "<style>"
        "body{font-family:sans-serif;margin:1.2rem;background:#111;color:#eee}"
        "h1{font-size:1.25rem;margin:0 0 1rem}"
        "label{display:block;margin:.75rem 0 .25rem;font-size:.85rem;color:#aaa}"
        "input,select,button{width:100%;box-sizing:border-box;padding:.6rem;border-radius:6px;"
        "border:1px solid #444;background:#1a1a1a;color:#eee;font-size:1rem}"
        "button{margin-top:1.25rem;background:#2d6a4f;border-color:#2d6a4f;font-weight:600}"
        ".msg{background:#3d1f1f;border:1px solid #833;padding:.75rem;border-radius:6px;margin-bottom:1rem}"
        ".hint{font-size:.8rem;color:#888;margin-top:.5rem}"
        "</style></head><body>"
        "<h1>GrowOS WiFi Setup</h1>"
    );

    if (_portalMessage.length() > 0) {
        html += F("<div class=msg>");
        html += _portalMessage;
        html += F("</div>");
    }

    html += F(
        "<form method=POST action=/save>"
        "<label for=scan>Nearby networks</label>"
        "<select id=scan "
        "onchange=\"document.getElementById('ssid').value=this.value\">"
        "<option value=\"\">— Select a network —</option>"
    );

    for (int i = 0; i < n; i++) {
        String name = WiFi.SSID(i);
        if (name.length() == 0) {
            continue;
        }
        html += F("<option value=\"");
        // Minimal HTML escape for quotes / markup in SSID
        for (unsigned j = 0; j < name.length(); j++) {
            char c = name[j];
            if (c == '"') {
                html += F("&quot;");
            } else if (c == '<') {
                html += F("&lt;");
            } else if (c == '&') {
                html += F("&amp;");
            } else {
                html += c;
            }
        }
        html += F("\">");
        html += name;
        html += F(" (");
        html += String(WiFi.RSSI(i));
        html += F(" dBm");
        if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) {
            html += F(", locked");
        }
        html += F(")</option>");
    }

    html += F(
        "</select>"
        "<label for=ssid>SSID (required)</label>"
        "<input id=ssid name=ssid type=text required "
        "placeholder=\"Select above or type a hidden SSID\">"
        "<label for=password>Password</label>"
        "<input id=password name=password type=password placeholder=\"WiFi password\">"
        "<button type=submit>Save &amp; Connect</button>"
        "</form>"
        "<p class=hint>After a successful connection the device restarts and joins your network.</p>"
        "</body></html>"
    );

    WiFi.scanDelete();
    return html;
}
