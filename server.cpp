#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "conf.h"

IPAddress apIP(192, 168, 1, 1);
AsyncWebServer server(80);
EpromTags *config_ptr;
DNSServer dnsServer;

const char index_html_pre_form[] = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Dashcam Shutdown Delay</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h2>Dashcam Power Control</h2>
  <form action="/">
)rawliteral";

static String form_formatter(String a, String b, String c) {
  String line = a + ":<br><input type=\"text\" name=\"" + b + "\" value=\"" + c + "\"><br>";
  return line;
}
const char index_html_post_form[] = R"rawliteral( 
<br><br><input type="submit" style="height: 30px; width: 100px" value="Save">
</form>
<form action="/">
<br><br><br><br><br><input type="text" name="erase" value="Type Y E S in this box to erase"><br>
<input type="submit"  style="height: 30px; width: 100px" value="Erase">
</form>
</body></html>)rawliteral";

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request) {
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {

    Serial.println("Web Server Request...");
    AsyncResponseStream *response = request->beginResponseStream("text/html");

    //check if erase was pressed
    if (request->hasParam("erase")) {
      if (strstr(request->getParam("erase")->value().c_str(), "YES")) {
        Serial.println("Erasing EEPROM...");
        conf_init(config_ptr);
        conf_put(config_ptr);
        conf_print(config_ptr);
      } else {
        Serial.println("Erase button pressed without YES...");
      }
    }

    //parse each posted item
    if (request->hasParam("ssid"))
      sprintf(config_ptr->ssid, "%s", request->getParam("ssid")->value().c_str());
    if (request->hasParam("password"))
      sprintf(config_ptr->password, "%s", request->getParam("password")->value().c_str());
    if (request->hasParam("retries"))
      sscanf(request->getParam("retries")->value().c_str(), "%u", &config_ptr->home_connection_attempts);
    if (request->hasParam("timeout"))
      sscanf(request->getParam("timeout")->value().c_str(), "%u", &config_ptr->home_connection_timeout);
    if (request->hasParam("delay"))
      sscanf(request->getParam("delay")->value().c_str(), "%u", &config_ptr->home_shutdown_delay);
    if (request->hasParam("host"))
      sprintf(config_ptr->host, "%s", request->getParam("host")->value().c_str());
    if (request->hasParam("port"))
      sscanf(request->getParam("port")->value().c_str(), "%u", &config_ptr->httpsPort);
    if (request->hasParam("url"))
      sprintf(config_ptr->url, "%s", request->getParam("url")->value().c_str());

    //lazy way to see if something was posted, should check each item
    if (request->hasParam("ssid")) {
      Serial.println("Updating EEPROM...");
      config_ptr->init = INIT_VALUE;
      conf_put(config_ptr);
      conf_print(config_ptr);
    }

    //generate the reply page
    response->print(index_html_pre_form);
    response->print(form_formatter("Wi-Fi SSID", "ssid", config_ptr->ssid));
    response->print(form_formatter("Wi-Fi Password", "password", config_ptr->password));
    response->print(form_formatter("Wi-Fi Retries", "retries", String(config_ptr->home_connection_attempts)));
    response->print(form_formatter("Wi-Fi Timeout (Seconds)", "timeout", String(config_ptr->home_connection_timeout)));
    response->print(form_formatter("Shutdown Delay (Minutes)", "delay", String(config_ptr->home_shutdown_delay)));
    response->print(form_formatter("IFTTT Host", "host", config_ptr->host));
    response->print(form_formatter("IFTTT Port", "port", String(config_ptr->httpsPort)));
    response->print(form_formatter("IFTTT URL", "url", config_ptr->url));
    response->print(index_html_post_form);

    //full send
    request->send(response);
  }
};

void server_start(EpromTags *config) {
  // save pointer for the async handler
  config_ptr = config;

  // setup wifi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Dashcam Power Control");

  // setup dns for captive portal
  dnsServer.start(53, "*", apIP);

  //s etup async webserver handler
  server.addHandler(new CaptiveRequestHandler());
  server.begin();

  uint32_t blink_time = millis();
  while (1) {
    // service DNS
    dnsServer.processNextRequest();

    // slow non-blocking blink while acting as a server
    static bool blink_state = true;
    if (millis() - blink_time > 1000) {
      digitalWrite(INDICATOR_LED, blink_state);
      blink_state = !blink_state;
      blink_time = millis();
    }
  }
}