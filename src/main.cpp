

#include <SPI.h>
#include <SD.h>
#include <PWMServo.h>

#include <creatures.h>

File myFile;

#define FILE_NAME "two.aaw"

PWMServo servos[2];
const int servo0Pin = 9;
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
  // Make sure the first five bytes are our magic number
  uint8_t buffer[5];
  file->readBytes(buffer, 5);

  for (int i = 0; i < 5; i++)
  {
    if (buffer[i] != MAGIC_NUMBER_ARRAY[i])
    {
      Serial.print("Magic Number fail at position ");
      Serial.println(i);
      return false;
    }
  }

  return true;
}

// Returns the header from the file
struct Header read_header(File *file)
{
  struct Header header;

  file->read(&header, sizeof(struct Header));

  Serial.print("number of servos: ");
  Serial.print(header.number_of_servos);
  Serial.print(", number of frames: ");
  Serial.print(header.number_of_frames);
  Serial.print(", ms per frame: ");
  Serial.println(header.time_per_frame);

  return header;
}

void play_frame(File *file, uint8_t number_of_servos)
{
  uint8_t servo[number_of_servos];
  file->read(&servo, number_of_servos);
  for (int i = 0; i < number_of_servos; i++)
  {
    //Serial.println(servo[i]);
    servos[i].write(servo[i]);
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("attaching to servo 0");
  servos[0].attach(servo0Pin);
  Serial.println("done");

  Serial.println("attaching to servo 1");
  servos[1].attach(servo1Pin);
  Serial.println("done");

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
