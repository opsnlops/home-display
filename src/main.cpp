

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <ESP32Servo.h>

#include <creatures.h>
#include "secrets.h"
#include "connection.h"

#define CREATURE_NAME "Sockey"

// WiFi network name and password:
const char *network_name = WIFI_NETWORK;
const char *network_password = WIFI_PASSWORD;

WiFiClient espClient;
PubSubClient client(espClient);

const char *broker_role = "magic";
const char *broker_service = "mqtt";
const char *broker_protocol = "tcp";

File myFile;

#define FILE_NAME "/two.aaw"

Servo servos[2];
const int servo0Pin = 2;
const int servo1Pin = 6;

// RAWR!
uint8_t MAGIC_NUMBER_ARRAY[5] = {0x52, 0x41, 0x57, 0x52, 0x21};

// Keep a counter of the frames we've played
uint16_t currentFrame = 0;

void config_fail()
{
  while (true)
  {
    digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
    delay(100);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
    delay(100);                      // wait for a second
  }
}

bool check_file(File *file)
{

  // Our magic number is 5 bytes long
  const uint8_t magic_number_size = 5;

  bool return_code = false;

  // Make sure the first five bytes are our magic number
  char *buffer = (char *)malloc(sizeof(uint8_t) * magic_number_size);

  file->readBytes(buffer, magic_number_size);

  for (int i = 0; i < magic_number_size; i++)
  {
    if (buffer[i] != MAGIC_NUMBER_ARRAY[i])
    {
      Serial.print("Magic Number fail at position ");
      Serial.println(i);
      return_code = false;
    }
  }

  // If we made it this far, we're good
  return_code = true;

  free(buffer);

  return return_code;
}

// Returns the header from the file
struct Header read_header(File *file)
{
  struct Header header;

  file->readBytes((char *)&header, sizeof(Header));

  Serial.print("number of servos: ");
  Serial.print(header.number_of_servos);
  Serial.print(", number of frames: ");
  Serial.print(header.number_of_frames);
  Serial.print(", ms per frame: ");
  Serial.println(header.time_per_frame);

  return header;
}

void play_frame(File *file, size_t number_of_servos)
{
  uint8_t servo[number_of_servos];
  file->readBytes((char *)&servo, number_of_servos);
  for (int i = 0; i < number_of_servos; i++)
  {
    Serial.println(servo[i]);
    servos[i].write(servo[i]);
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("attaching to servo 0");
  servos[0].setPeriodHertz(50);
  servos[0].attach(servo0Pin);
  Serial.println("done");

  Serial.println("attaching to servo 1");
  servos[1].setPeriodHertz(50);
  servos[1].attach(servo1Pin);
  Serial.println("done");

  // Connect to the WiFi network (see function below loop)
  connectToWiFi(network_name, network_password);
  Serial.println("connected to Wifi");

  if (!MDNS.begin(CREATURE_NAME))
  {
    Serial.println("Error setting up mDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("MDNS set up");

  // We aren't _really_ running something on tcp/666, but this lets me
  // find the IP of the creature from an mDNS browser
  MDNS.addService("creature", "tcp", 666);
  Serial.println("added our fake mDNS service");

  IPAddress broker_ip = find_broker(broker_service, broker_protocol);
  Serial.println("The IP of the broker is " + broker_ip.toString());
  client.setServer(broker_ip.toString().c_str(), 1883);

  // Set up the SD ard

  Serial.print("Initializing SD card...");
  if (!SD.begin(5))
  {
    Serial.println("initialization failed!");
    while (1)
      ;
  }
  Serial.println("initialization done.");
  // open the file for reading:
  myFile = SD.open(FILE_NAME);
  if (myFile)
  {
    Serial.println(FILE_NAME);

    if (!check_file(&myFile))
    {
      config_fail();
    }

    struct Header header = read_header(&myFile);

    // read from the file until there's nothing else in it:
    while (myFile.available())
    {

      uint8_t command = myFile.read();
      if (command == (uint8_t)MOVEMENT_FRAME_TYPE)
      {
        play_frame(&myFile, header.number_of_servos);
        delay(header.time_per_frame);
      }

      if (currentFrame % 10 == 0)
      {
        Serial.print("frame ");
        Serial.println(currentFrame);
      }

      currentFrame++;
    }
    // close the file:
    myFile.close();
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(FILE_NAME);
    config_fail();
  }
}

void loop()
{
  // nothing happens after setup
}
