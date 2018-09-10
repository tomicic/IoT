// BASED ON THE DS18B20 SENSOR CODE PROVIDED BY: Rob Tillaart

#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <TinyGPS++.h>

#define ONE_WIRE_BUS_1 15

OneWire oneWire_in(ONE_WIRE_BUS_1);
DallasTemperature sensor_inhouse(&oneWire_in);

const int buttonPin = 14;
const int ledPin =  27;
int buttonState = 0;
int location_code = 0;

float oldTemp = 0;
float newTemp = 100;
int WiFiCode = 0;

// set up the 'temperature' and 'humidity' feeds
AdafruitIO_Feed *temperature = io.feed("temperature");
AdafruitIO_Feed *humidity = io.feed("humidity");

// set up the 'longitude', 'latitude', and other feeds
AdafruitIO_Feed *latitude = io.feed("latitude");
AdafruitIO_Feed *longitude = io.feed("longitude");
AdafruitIO_Feed *latlong = io.feed("latlong");
AdafruitIO_Feed *timeio = io.feed("timeio");
AdafruitIO_Feed *humidityloc = io.feed("humidityloc");
AdafruitIO_Feed *temperatureloc = io.feed("temperatureloc");


// GPS module data (EM-506 model)
static const int RXPin = 16, TXPin = 17;
static const uint32_t GPSBaud = 4800; // this is fairly important

// TinyGPS++ object
TinyGPSPlus gps;
HardwareSerial Serial1(2);


const char* ssid      = "-------";
const char* pass      = "-------";
const char* resource  = "-------";
const char* server    = "-------";


void setup(void) {
  
    pinMode(buttonPin, INPUT);
    pinMode(ledPin, OUTPUT);

    Serial.begin(9600);

    // GPS serial
    Serial1.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
    
    Serial.println("***** test DS18B20 *****");

    sensor_inhouse.begin();

    digitalWrite(ledPin, LOW);
    WiFi.begin(ssid, pass);
    delay(2000);

    // connecting to io.adafruit.com
    Serial.print("Connecting to Adafruit IO");
    io.connect();
  
    // wait for a connection
    while(io.status() < AIO_CONNECTED) {
      Serial.print(".");
      delay(500);
    }

    // connected to io.adafruit.com
    Serial.println();
    Serial.println(io.statusText());

}

void loop(void) {

  checkWiFi();
  
  // keep the client connected to io.adafruit.com:
  io.run();

  while (Serial1.available() > 0)
    {
      if (gps.encode(Serial1.read()))
      {
          GPSInfo();
          break;
      }
    }

    if (millis() > 5000 && gps.charsProcessed() < 10)
    {
        Serial.println(F("No GPS detected: check wiring."));
    }
    

    sensor_inhouse.requestTemperatures();
    Serial.println(sensor_inhouse.getTempCByIndex(0));
    
    
    // save temp to Adafruit IO:
    temperature->save(sensor_inhouse.getTempCByIndex(0));
    delay(2000);


//  oldTemp = newTemp;
//  newTemp = sensor_inhouse.getTempCByIndex(0);

    //calculate temperature difference between the last two readings
    //Serial.println("TEMPERATURE DIFFERENCE:");
    //Serial.println(abs(oldTemp-newTemp));
    //delay(2000);    
    
    buttonState = digitalRead(buttonPin);

    // "gps.location.age() < 5000" is here because the isValid() will return true even if the gps fix is lost (old value)
    
    if (WiFi.status() == 3 && gps.location.isValid() && gps.location.age() < 5000) {
      
      digitalWrite(ledPin, HIGH);
      Serial.println("... READY FOR UPLOAD ...");
      
      if (buttonState == HIGH) {
        Serial.println("SWITCH PRESSED!");
        ledBlinking();
        delay(1000);
        //readTemp();
        GPSInfo();
        makeIFTTTRequest();
      }
    }
    else {
      digitalWrite(ledPin, LOW);
      Serial.println("WiFi and/or GPS not working");
      Serial.println(WiFi.status());
      Serial.println(gps.location.isValid());
      Serial.println(gps.location.age());
    }

}

void readTemp(void) {
  
    Serial.print("Requesting temperatures...");
    sensor_inhouse.requestTemperatures();
    Serial.println(" done");

    Serial.print("Temperature: ");
    Serial.println(sensor_inhouse.getTempCByIndex(0));

    digitalWrite(ledPin, HIGH);
    delay(2000);
    
}

// Make an HTTP request to the IFTTT web service:

void makeIFTTTRequest() {
  Serial.print("Connecting to "); 
  Serial.print(server);
  
  WiFiClient client;
  int retries = 5;
  while(!!!client.connect(server, 80) && (retries-- > 0)) {
    Serial.print(".");
  }
  Serial.println();
  if(!!!client.connected()) {
    Serial.println("Failed to connect...");
  }
  
  Serial.print("Request resource: "); 
  Serial.println(resource);

  // Temperature in Celsius
  sensor_inhouse.requestTemperatures();

  char latlong[100];
  sprintf(latlong,"%lf, %lf", gps.location.lat(),gps.location.lng());
  
  String jsonObject = String("{\"value1\":\"") + sensor_inhouse.getTempCByIndex(0) + "\",\"value2\":\"" + location_code
                      + "\",\"value3\":\"" + latlong + "\"}";
                      
  client.println(String("POST ") + resource + " HTTP/1.1");
  client.println(String("Host: ") + server); 
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);
        
  int timeout = 5 * 10; // 5 seconds             
  while(!!!client.available() && (timeout-- > 0)){
    delay(100);
  }
  if(!!!client.available()) {
    Serial.println("No response...");
  }
  while(client.available()){
    Serial.write(client.read());
  }
  
  Serial.println("\nclosing connection");
  client.stop(); 

  location_code++;
}


void checkWiFi() {
  
  if (WiFi.status() == 3) {
      Serial.println("ESP32 connected to:");
      Serial.println(ssid);
  }

  if (WiFi.status() == 6 || WiFi.status() == 1 || WiFi.status() == 4 || WiFi.status() == 5){
      Serial.println("WIRELESS CONNECTION LOST!");
      //Serial.println("restarting device...!");
      //ESP.restart();
      Serial.println("trying to reconnect...!");
      WiFi.begin(ssid, pass);
      delay(6000); // delay shouldn't be too small because WiFi doesn't have the time to reconnect!
      checkWiFi();
  }

}



// GPS
void GPSInfo()
{
    Serial.print(F("Location: "));
    if (gps.location.isValid())
    {
        Serial.print(gps.location.lat(), 6);
        Serial.print(F(","));
        Serial.print(gps.location.lng(), 6);

        //latitude->save(gps.location.lat());
        //delay(1000);
        //longitude->save(gps.location.lng());
        //delay(1000);
        
    }
    else
    {
        Serial.print(F("LOCATION INVALID"));
    }

    Serial.print(F("  Date/Time: "));
    if (gps.date.isValid())
    {
        Serial.print(gps.date.day());
        Serial.print(F("."));
        Serial.print(gps.date.month());
        Serial.print(F("."));
        Serial.print(gps.date.year());
        Serial.print(F("."));

        //timeio->save(gps.time.value());
        //delay(1000);
        //temperatureloc->save(sensor_inhouse.getTempCByIndex(0), gps.location.lat(), gps.location.lng(), gps.altitude.meters());
    }
    else
    {
        Serial.print(F("GPS DATE INVALID"));
    }

    Serial.print(F(" "));
    if (gps.time.isValid())

    {
        if (gps.time.hour() < 10)
            Serial.print(F("0"));
        
    Serial.print(gps.time.hour());
        Serial.print(F(":"));
        
    if (gps.time.minute() < 10)
            Serial.print(F("0"));
        
    Serial.print(gps.time.minute());
        Serial.print(F(":"));

        if (gps.time.second() < 10)
            Serial.print(F("0"));

        Serial.print(gps.time.second());
        Serial.print(F("."));

        if (gps.time.centisecond() < 10)
            Serial.print(F("0"));

        Serial.print(gps.time.centisecond());
    }

    else
    {
        Serial.print(F("GPS TIME INVALID"));
    }
    Serial.println();
}

void ledBlinking(){

  for (int a = 0; a < 10; a++){
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
  }

}
