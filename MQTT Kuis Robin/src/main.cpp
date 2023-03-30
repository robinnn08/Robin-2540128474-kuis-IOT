#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <BH1750.h>

#define LED_RED 15 
#define LED_YELLOW 2  
#define LED_GREEN 14 

#define DHTPIN 16
#define DHTTYPE DHT11

#define SDA_PIN 21
#define SCL_PIN 22

#define WIFI_SSID "hotspot1"
#define WIFI_PASSWORD "12345678"

#define MQTT_BROKER "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH "esp32_robb/data"
#define MQTT_TOPIC_SUBSCRIBE "esp32_robb/cmd"

DHT dht(DHTPIN, DHTTYPE);

Ticker timerPublish;
Ticker timerMqtt;
char g_szDeviceId[30];

float temperature = 0;
float humidity = 0;
float lux = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);

BH1750 lightMeter(0x23);

void WifiConnect()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void WarningLed()
{
  dht.begin();
  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE, 0x23, &Wire);

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  lux = lightMeter.readLightLevel();

  Serial.printf("Temperature: %2f\n", temperature);
  Serial.printf("Humidity: %2f\n", humidity);
  Serial.printf("Lux: %2f\n", lux);

  // Warning Lux
  if (lux > 400)
  {
    Serial.println("Pintu DIBUKA");
  }
  else
  {
    Serial.println("Pintu DITUTUP");
  }

  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Warning Temperature and Humidity
  if (temperature > 28 && humidity > 80)
  {
    digitalWrite(LED_RED, HIGH);
    Serial.println("Warning High Temp and Humidity!");
  }
  else
  {
    digitalWrite(LED_RED, LOW);
  }
  if (temperature > 28 && humidity > 60 && humidity < 80)
  {
    digitalWrite(LED_YELLOW, HIGH);
    Serial.println("Warning High Temp and Medium Humidity!");
  }
  else
  {
    digitalWrite(LED_YELLOW, LOW);
  }
  if (temperature < 28 && humidity < 60)
  {
    digitalWrite(LED_GREEN, HIGH);
    Serial.println("Temp and Humidity is normal");
  }
  else
  {
    digitalWrite(LED_GREEN, LOW);
  }
  delay(1000);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  Serial.println();
}

void reconnect()
{
  while (!mqtt.connected())
  {
    Serial.print("Connecting to MQTT");
    if (mqtt.connect(g_szDeviceId))
    {
      Serial.println("Connected to MQTT broker");
      mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      delay(2000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE, 0x23, &Wire))
  {
    Serial.println("Error initialising BH1750");
  }

  WifiConnect();
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(callback);

  while (!mqtt.connected())
  {
    String clientId = "esp32_robb";
    if (mqtt.connect(clientId.c_str()))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.print(mqtt.state());
      delay(2000);
    }
  }

  mqtt.publish(MQTT_TOPIC_PUBLISH, "Hello from esp32_robb");
  mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);

  dht.begin();
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
}

void loop()
{
  WarningLed();

  if (!mqtt.connected())
  {
    reconnect();
  }

  unsigned long currentMillis = millis();

  static unsigned long previousLuxMillis = 0;
  if (currentMillis - previousLuxMillis >= 4000)
  {
    mqtt.publish(MQTT_TOPIC_PUBLISH, String(lux).c_str());
    previousLuxMillis = currentMillis;
  }

  static unsigned long previousTemperatureMillis = 0;
  if (currentMillis - previousTemperatureMillis >= 5000)
  {
    mqtt.publish(MQTT_TOPIC_PUBLISH, String(temperature).c_str());
    previousTemperatureMillis = currentMillis;
  }

  static unsigned long previousHumidityMillis = 0;
  if (currentMillis - previousHumidityMillis >= 6000)
  {
    mqtt.publish(MQTT_TOPIC_PUBLISH, String(humidity).c_str());
    previousHumidityMillis = currentMillis;
  }

  mqtt.loop();
}