
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "WiFiMQTTManager.h"
#include "Stepper.h"
#include "secrets.h"

const int RESET_BUTTON = D7; //Button that will put device into Access Point mode to allow for re-entering WiFi and MQTT settings

const int Pin1 = D1; //define pin for ULN2003 in1
const int Pin2 = D2; //define pin for ULN2003 in2
const int Pin3 = D3; //define pin for ULN2003 in3
const int Pin4 = D4; //define pin for ULN2003 in4

const String htmlHeader = "<!DOCTYPE html><html><head><title>Cat (Giulio) feeder motor control</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>\html,body{  width:100%\;height:100%\;margin:0}*{box-sizing:border-box}.colorAll{background-color:#90ee90}.colorBtn{background-color:#add8e6}.angleButtdon,a{font-size:30px\;border:1px solid #ccc\;display:table-caption\;padding:7px 10px\;text-decoration:none\;cursor:pointer\;padding:5px 6px 7px 10px}a{display:block}.btn{margin:5px\;border:none\;display:inline-block\;vertical-align:middle\;text-align:center\;white-space:nowrap}";
const String buttonTitles[] = {"Feed"};
const String argId[] = {"feed"};

// stepper control
const int feederSteps = 512;
const int stepsPerRevolution = 2048;
const int stepperSpeed = 6;

const String mqtt_topic = "cat_giulio_feeder";
const String client_id = "esp8266-client-";

String mqtt_broker = "192.168.1.";
int mqtt_port = 1883;
String mqtt_username = "mqttuser";
String mqtt_password = "";

boolean feedingStatus = false;

Stepper stepper = Stepper(stepsPerRevolution, D1, D2, D3, D4);
WiFiMQTTManager wmm(RESET_BUTTON, AP_PASSWORD);  // AP_PASSWORD is defined in the secrets.h file
ESP8266WebServer server(80);

void handleRoot()
{

  String HTML = htmlHeader;
  HTML += "</style></head><body><h1>Cat (Giulio) feeder motor control</h1>";

  if (feedingStatus)
  {
    HTML += "<h2><span style=\"background-color: #FFFF00\">Feed ON</span></h2>";
  }
  else
  {
    HTML += "<h2><span style=\"background-color: #FFFF00\">Feed OFF</span></h2>";
  }

  if (feedingStatus)
  {
    HTML += "<div class=\"btn\"><a class=\"angleButton\" style=\"background-color:#f56464\" href=\"/feeder?";
    HTML += argId[0];
    HTML += "=off\">";
    HTML += buttonTitles[0]; //Feed button title
  }
  else
  {
    HTML += "<div class=\"btn\"><a class=\"angleButton \" style=\"background-color:#90ee90\" href=\"/feeder?";
    HTML += argId[0];
    HTML += "=on\">";
    HTML += buttonTitles[0]; //Feed button title
  }

  HTML += "</a> </div>";

  HTML += "</body></html>";

  server.send(200, "text/html", HTML);
}

void handleNotFound()
{
  String message = "File Not Found";
  message += "URI: ";
  message += server.uri();
  message += "Method: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "Arguments: ";
  message += server.args();
  message += "";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "";
  }

  server.send(404, "text/plain", message);
}

void configModeCallback (WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup(void)
{
  stepper.setSpeed(stepperSpeed);

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
  if (MDNS.begin("catgiuliofeeder"))
  {
    Serial.println("MDNS responder started");
    Serial.println("access via http://catgiuliofeeder.local");
  }

  server.on("/", handleRoot);
  server.on("/feeder", HTTP_GET, feederControl);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  birthMessage();
}

void birthMessage()
{
  char topic[200];
  snprintf(topic, sizeof(topic), "%s%s%s", "sensors/feeders/", wmm.deviceId, "/address");

  Serial.print("Birth message for sensor : " );
  Serial.print(wmm.deviceId );
  Serial.print(" ready at ");
  Serial.println(WiFi.localIP().toString());
  wmm.client->publish(topic, WiFi.localIP().toString().c_str(), true);
}

// optional function to subscribe to MQTT topics
void subscribeTo()
{
  Serial.println("subscribing to some topics...");
  // subscribe to some topic(s)
  char topic[200];
  snprintf(topic, sizeof(topic), "%s%s%s", "sensors/feeders/", wmm.deviceId, "/command/feed");
  wmm.client->subscribe(topic);
}

// optional function to process MQTT subscribed to topics coming in
void subscriptionCallback(char* topic, byte* message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  String commandMessage;

  for (int i = 0; i < length; i++)
  {
    // Serial.println((char)message[i]);
    commandMessage += (char)message[i];
  }
  Serial.print("Message arrived: ");
  Serial.println(commandMessage);

  command(commandMessage);
}

void loop(void)
{
  server.handleClient();
  MDNS.update();

  // required - allow WiFiMQTTManager to check for new MQTT messages,
  // check for reset button push, and reconnect to MQTT if necessary
  wmm.loop();

  delay(2000);
}

void feederControl()
{
  feedExecute(server.arg(argId[0]));

  handleRoot();
}

void feedingMessage(boolean feedingStatus)
{
  char topic[200];
  snprintf(topic, sizeof(topic), "%s%s%s", "sensors/feeders/", wmm.deviceId, "/status");

  if (feedingStatus)
  {
    Serial.print("Feeding message: ");
    Serial.println("feeding");
    wmm.client->publish(topic, "feeding", true);
  }
  else
  {
    Serial.print("Feeding message: ");
    Serial.println("off");
    wmm.client->publish(topic, "off", true);
  }
}

void command(String commandMessage)
{
  if ( !strcmp(commandMessage.c_str(), "on")) return feedExecute("on");
  if ( !strcmp(commandMessage.c_str(), "off")) return feedExecute("off");
}

void feedExecute(String command)
{
  if (command == "on")
  {
    feedingStatus = true;
    feedingMessage(feedingStatus);
    driveStepper(feederSteps);
    delay(300);
    driveStepper(-feederSteps);
    feedingStatus = false;
    feedingMessage(feedingStatus);
  }

  if (command == "off")
  {
    feedingStatus = false;
    feedingMessage(feedingStatus);
  }
}

void driveStepper(int steps)
{
  stepper.step(steps);
}
