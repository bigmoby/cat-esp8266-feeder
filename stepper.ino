const int Pin1 = D1;//define pin for ULN2003 in1
const int Pin2 = D2;//define pin for ULN2003 in2
const int Pin3 = D3;//define pin for ULN2003 in3
const int Pin4 = D4;//define pin for ULN2003 in4

const int pole1[] = {0, 0, 0, 0, 0, 1, 1, 1, 0}; //pole1, 8 step values
const int pole2[] = {0, 0, 0, 1, 1, 1, 0, 0, 0}; //pole2, 8 step values
const int pole3[] = {0, 1, 1, 1, 0, 0, 0, 0, 0}; //pole3, 8 step values
const int pole4[] = {1, 1, 0, 0, 0, 0, 0, 1, 0}; //pole4, 8 step values

int poleStep = 0;
int dirStatus = 3; // stores direction status 3 ==> STOP the stepper

const String buttonTitles[] = {"CCW", "CW", "MQTT Config"};
const String argId[] = {"ccw", "cw"};

String mqtt_broker = "192.168.1.";
int mqtt_port = 1883;
String mqtt_username = "mqttuser";
String mqtt_password = "";
String mqtt_topic = "cat_giulio_feeder";

String client_id = "esp8266-client-";

const String htmlHeader = "<!DOCTYPE html><html><head><title>Cat (Giulio) feeder motor control</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>\html,body{  width:100%\;height:100%\;margin:0}*{box-sizing:border-box}.colorAll{background-color:#90ee90}.colorBtn{background-color:#add8e6}.angleButtdon,a{font-size:30px\;border:1px solid #ccc\;display:table-caption\;padding:7px 10px\;text-decoration:none\;cursor:pointer\;padding:5px 6px 7px 10px}a{display:block}.btn{margin:5px\;border:none\;display:inline-block\;vertical-align:middle\;text-align:center\;white-space:nowrap}";

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "secrets.h"
#include <WiFiMQTTManager.h>
#include "Stepper_28BYJ_48.h"

Stepper_28BYJ_48 stepper(D1, D2, D3, D4);

// Button that will put device into Access Point mode to allow for re-entering WiFi and MQTT settings
#define RESET_BUTTON D8

WiFiMQTTManager wmm(RESET_BUTTON, AP_PASSWORD);  // AP_PASSWORD is defined in the secrets.h file

ESP8266WebServer server(80);

void handleRoot() {

  String HTML = htmlHeader;

  HTML += "</style></head><body><h1>Cat (Giulio) feeder motor control</h1>";

  if (dirStatus == 2) {
    HTML += "<h2><span style=\"background-color: #FFFF00\">Motor Running in CW</span></h2>";
  } else if (dirStatus == 1) {
    HTML += "<h2><span style=\"background-color: #FFFF00\">Motor Running in CCW</span></h2>";
  } else {
    HTML += "<h2><span style=\"background-color: #FFFF00\">Motor OFF</span></h2>";
  }

  if (dirStatus == 1) {
    HTML += "<div class=\"btn\"><a class=\"angleButton\" style=\"background-color:#f56464\" href=\"/motor?";
    HTML += argId[0];
    HTML += "=off\">";
    HTML += buttonTitles[0]; //motor ON title
  } else {
    HTML += "<div class=\"btn\"><a class=\"angleButton \" style=\"background-color:#90ee90\" href=\"/motor?";
    HTML += argId[0];
    HTML += "=on\">";
    HTML += buttonTitles[0]; //motor OFF title
  }

  HTML += "</a> </div>";

  if (dirStatus == 2) {
    HTML += "<div class=\"btn\"><a class=\"angleButton\" style=\"background-color:#f56464\" href=\"/motor?";
    HTML += argId[1];
    HTML += "=off\">";
    HTML += buttonTitles[1]; //motor ON title
  } else {
    HTML += "<div class=\"btn\"><a class=\"angleButton \" style=\"background-color:#90ee90\" href=\"/motor?";
    HTML += argId[1];
    HTML += "=on\">";
    HTML += buttonTitles[1]; //motor OFF title
  }
  HTML += "</a></div>";

  HTML += "</body></html>";
  server.send(200, "text/html", HTML);
}

void handleNotFound() {
  String message = "File Not Found";
  message += "URI: ";
  message += server.uri();
  message += "Method: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "Arguments: ";
  message += server.args();
  message += "";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "";
  }

  server.send(404, "text/plain", message);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup(void) {

  pinMode(Pin1, OUTPUT);
  pinMode(Pin2, OUTPUT);
  pinMode(Pin3, OUTPUT);
  pinMode(Pin4, OUTPUT);

  Serial.begin(115200);
  Serial.println("Cat (Giulio) feeder motor control");

  // set debug to true to get verbose logging
  // wm.wm.setDebugOutput(true);
  // most likely need to format FS but only on first use
  // wmm.formatFS = true;
  // optional - define the function that will subscribe to topics if needed
  wmm.subscribeTo = subscribeTo;
  // required - allow WiFiMQTTManager to do it's setup
  wmm.setup(__SKETCH_NAME__);
  // optional - define a callback to handle incoming messages from MQTT
  wmm.client->setCallback(subscriptionCallback);

  Serial.print("IP address: http://");
  Serial.println(WiFi.localIP());

  //multicast DNS to catgiuliofeeder.local
  if (MDNS.begin("catgiuliofeeder")) {
    Serial.println("MDNS responder started");
    Serial.println("access via http://catgiuliofeeder.local");
  }

  server.on("/", handleRoot);
  server.on("/motor", HTTP_GET, motorControl);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  birthMessage();

}

void birthMessage() {
  char topic[100];
  snprintf(topic, sizeof(topic), "%s%s%s", "sensors/feeders/", wmm.deviceId, "/address");

  Serial.print("Birth message for sensor : " );
  Serial.print(wmm.deviceId );
  Serial.print(" ready at ");
  Serial.print(WiFi.localIP().toString());
  wmm.client->publish(topic, WiFi.localIP().toString().c_str(), true);
}

// optional function to subscribe to MQTT topics
void subscribeTo() {
  Serial.println("subscribing to some topics...");
  // subscribe to some topic(s)
  char topic[100];
  snprintf(topic, sizeof(topic), "%s%s%s", "sensors/feeders/", wmm.deviceId, "/command");
  wmm.client->subscribe(topic);
}

// optional function to process MQTT subscribed to topics coming in
void subscriptionCallback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void loop(void) {
  server.handleClient();
  MDNS.update();

  // required - allow WiFiMQTTManager to check for new MQTT messages,
  // check for reset button push, and reconnect to MQTT if necessary
  wmm.loop();

  if (dirStatus == 1) {
    poleStep++;
    driveStepper(poleStep);
  } else if (dirStatus == 2) {
    poleStep--;
    driveStepper(poleStep);
  } else {
    driveStepper(8);
  }

  if (poleStep > 7) {
    poleStep = 0;
  }

  if (poleStep < 0) {
    poleStep = 7;
  }

  delay(500);
}

void motorControl() {
  if (server.arg(argId[0]) == "on")
  {
    dirStatus = 1;// CCW
  } else if (server.arg(argId[0]) == "off") {
    dirStatus = 3;  // motor OFF
  } else if (server.arg(argId[1]) == "on") {
    dirStatus = 2;  // CW
  } else if (server.arg(argId[1]) == "off") {
    dirStatus = 3;  // motor OF
  }

  handleRoot();
}

void driveStepper(int c)
{
  stepper.step(c);
}
