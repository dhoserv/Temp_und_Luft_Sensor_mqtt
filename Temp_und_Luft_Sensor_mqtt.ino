/*********
free
*********/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Replace with your network credentials
const char* ssid = "wlan-ssid";
const char* password = "wlan-password";
const char* Hostname = "hostname";

// MQTT
const char* mqtt_server = "p-address-mqtt-broker";
const char* mqtt_hostname = "hostname-mqtt-broker";
const char* mqtt_username = "mqtt-user";
const char* mqtt_password = "mqtt-password";
const int mqtt_port = 1883;
const char* mqtt_topic_temperature = "Umgebungssensor1/temperatur";
const char* mqtt_topic_humidity = "Umgebungssensor1/humidity";
const char* mqtt_topic_pressure = "Umgebungssensor1/pressure";
const char* mqtt_topic_gas = "Umgebungssensor1/gas";

// OTA
const char* OTAHostname ="OTA-hostname";
const char* OTAPassword = "OTA-password";

//Static IP address configuration
IPAddress staticIP(xxx, xxx, xxx, xxx); //ESP static ip
IPAddress gateway(xxx, xxx, xxx, xxx); //IP Address of your WiFi Router (Gateway)
IPAddress subnet(xxx, xxx, xxx, xxx); //Subnet mask
IPAddress dns(xxx, xxx, xxx, xxx); //DNS

//Uncomment if using SPI
/*#define BME_SCK 14
#define BME_MISO 12
#define BME_MOSI 13
#define BME_CS 15*/

Adafruit_BME680 bme; // I2C
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
//Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
Adafruit_SSD1306 display(128, 32, &Wire, -1);

float temperature;
float humidity;
float pressure;
float gasResistance;

AsyncWebServer server(80);
AsyncEventSource events("/events");
Ticker mqttReconnectTimer;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 10000;        // Interval at which to publish sensor readings

unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;  // send readings timer
//unsigned long timerDelay = 60;

void getBME680Readings(){
  // Tell BME680 to begin measurement.
  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }
  temperature = bme.temperature;
  pressure = bme.pressure / 100.0;
  humidity = bme.humidity;
  gasResistance = bme.gas_resistance / 1000.0;
}

String processor(const String& var){
  getBME680Readings();
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
  else if(var == "PRESSURE"){
    return String(pressure);
  }
  else if(var == "GAS"){
    return String(gasResistance);
  }
  return String();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<head>
  <title>Umgebungssensor 1</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #13f6fe; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .card.temperature { color: #0e7c7b; }
    .card.humidity { color: #17bebb; }
    .card.pressure { color: #3fca6b; }
    .card.gas { color: #d62246; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>Umgebungssensor 1</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATURE</h4><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> HUMIDITY</h4><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card pressure">
        <h4><i class="fas fa-angle-double-down"></i> PRESSURE</h4><p><span class="reading"><span id="pres">%PRESSURE%</span> hPa</span></p>
      </div>
      <div class="card gas">
        <h4><i class="fas fa-wind"></i> GAS</h4><p><span class="reading"><span id="gas">%GAS%</span> K&ohm;</span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('pressure', function(e) {
  console.log("pressure", e.data);
  document.getElementById("pres").innerHTML = e.data;
 }, false);
 
 source.addEventListener('gas', function(e) {
  console.log("gas", e.data);
  document.getElementById("gas").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);
  client.setServer(mqtt_server, mqtt_port);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done
  display.display();
  delay(100);
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  
  // Set device as a Wi-Fi Station
  WiFi.config(staticIP, subnet, gateway, dns);
  WiFi.hostname(Hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  
  ArduinoOTA.setHostname(OTAHostname);
  ArduinoOTA.setPassword(OTAPassword);
  ArduinoOTA.setPort(80);
  ArduinoOTA.begin();

  // Init BME680 sensor
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    while (1);
  }
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  client.connect(mqtt_hostname, mqtt_username, mqtt_password);

  display.setCursor(0,0);
  display.clearDisplay();
  display.dim(false);
  display.setTextColor(WHITE);
  
  if ((millis() - lastTime) > timerDelay) {
    getBME680Readings();
    Serial.printf("Temperature = %.2f ºC \n", temperature);
    display.print("Temperature: "); display.print(temperature); display.println(" *C");
    client.publish(mqtt_topic_temperature, String(temperature).c_str());
    Serial.printf("Humidity = %.2f % \n", humidity);
    display.print("Humidity: "); display.print(humidity); display.println(" %");
    client.publish(mqtt_topic_humidity, String(humidity).c_str());
    Serial.printf("Pressure = %.2f hPa \n", pressure);
    display.print("Pressure: "); display.print(pressure); display.println(" hPa");
    client.publish(mqtt_topic_pressure, String(pressure).c_str());
    Serial.printf("Gas Resistance = %.2f KOhm \n", gasResistance);
    client.publish(mqtt_topic_gas, String(gasResistance).c_str());
    Serial.println();

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(temperature).c_str(),"temperature",millis());
    events.send(String(humidity).c_str(),"humidity",millis());
    events.send(String(pressure).c_str(),"pressure",millis());
    events.send(String(gasResistance).c_str(),"gas",millis());

    display.display();
    
    lastTime = millis();
  }
}
