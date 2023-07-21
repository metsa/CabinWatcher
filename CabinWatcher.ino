// Using BME280 and DS18B20 with LoRaWAN
// (c) 2023 Jonne Viljanen (jonne.viljanen@helsinki.fi)
// (c) 2023 Pekko Metsä    (pekko.metsa@helsinki.fi)

#include <MKRWAN.h>          // Library for the LoRaWAN communication
#include "Adafruit_BME280.h" // Library for the BME280 sensor
#include <RTCZero.h>         // Library for RTC on SAMD21-based boards (including MKR series)
#include <OneWire.h>            // Libraries for Dallas
#include <DallasTemperature.h>  // DS18B20 OneWire sensor

// ID of the LoRaWAN radio
const char appEui[] = "AAAAAAAAAAAAAAAA"; 
const char appKey[] = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";

// Create instances of the LoRa modem and BME280 sensor
LoRaModem modem;
Adafruit_BME280 bme; // I2C
bool BME280_usable;

// Instantiate the RTC object
RTCZero rtc;

// Data wire for DS18B20 sensors is plugged into port 7 on the Arduino
#define ONE_WIRE_BUS 7
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
int numberOfDevices;             // Number of temperature devices found
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

// Define the sleep time in minutes.
//#define SLEEP_TIME 3600 //1h
#define SLEEP_TIME 900 //15min

// General error value for defective or unreadable sensor
#define SENSOR_ERROR (-1e6F)

// Define a structure to store hours, minutes, and seconds
struct Time {
  int hours;
  int minutes;
  int seconds;
};

Time wakeUpTime;

void setup() {
  // Initialize the serial interface with a baud rate of 9600
  // This will be used for outputting error messages
  Serial.begin(115200);

  // Wait for the serial interface to connect
  // This is necessary for boards that have a native USB port
  unsigned long startMillis = millis(); // save the start time

  while(millis() - startMillis < 20000) { // loop for 20 seconds
    if(Serial) { // if serial connection is established
      break;
    }
    else {
      // optionally, go into a lower power mode here
      // to save energy while waiting for a connection
      delay(1000); // delay for a second to reduce checking frequency
    }  
  }
  
  Serial.print("Starting at: ");
  Serial.print(rtc.getHours());
  Serial.print(":");
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  Serial.println(rtc.getSeconds());
  Serial.print("UNIX time: ");
  Serial.println(rtc.getEpoch());

  // Initialize the BME280 sensor
  // The argument to the begin function is the I2C address of the sensor
  // The BME280 sensor can have one of two I2C addresses: 0x76 or 0x77. 
  // This depends on how the SDO pin is connected (GND for 0x76 and VCC for 0x77). 
  // If you are not sure, you can try changing the address in your code to the other value.
  if(bme.begin(0x77)) {
    BME280_usable = true;
  } else {
    // If the sensor could not be initialized, print an error message and mark the sensor unusable.
    Serial.println("Failed to initialize BME280 sensor");
    BME280_usable = false;
  }

  // Initialize the LoRa modem
  // The argument to the begin function is the LoRaWAN frequency plan
  // In this case, we're using the European frequency plan (EU868)
  if (!modem.begin(EU868)) {
    // If the modem could not be initialized, print an error message and stop the program
    Serial.println("Failed to initialize LoRa modem");
    return;
  }

  // Join the LoRaWAN network using Over The Air Activation (OTAA)
  bool connected = false;
  int counter = 0;
  while(not connected){
    counter++;
    Serial.print("trying to connect with OTAA, trial number ");
    Serial.println(counter);    
    connected = modem.joinOTAA(appEui, appKey);
    if(counter == 20){
      // If the network could not be joined, print an error message and stop the program
      Serial.println("Failed to join LoRa network");
      return;
    }
    delay(60000); // wait for 60 seconds before the next join attempt
    // it's important not to make join requests too frequently, as
    // this can lead to network congestion
  }

  // Trying to set the data mode for the LoRa Modem.  Not sure if effective, though.
  modem.setADR(true);
  modem.dataRate(6);

  setup_dallas();
  
  // If setup completed successfully, print a success message
  Serial.println("Setup completed successfully");

  // Start the RTC clock
  rtc.begin();
}

void setup_dallas() {
  // Start up and setup the Dallas DS18B20 library
  sensors.begin();
  // Set bit width to 12 bit (ie. 0.0625°C resolution)
  sensors.setResolution(12);

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");
  
   // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++) {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
		} else {
		  Serial.print("Found ghost device at ");
		  Serial.print(i, DEC);
		  Serial.print(" but could not detect address. Check power and cabling");
		}
  }
} 


void loop() {
  // The time stamp
  union {
    uint32_t uint;
    byte     bytes[4];
  } timestamp;
  
  // The sensor data will be sent as 32bit floats (little endian) via
  // the radio link.  Reading the sensor values to unions which can be
  // handled either as floats or as byte strings.

  // Union for DS18B20 sensor temperature
  union {
    float flotari;
    byte bytes[4];
  } ds18b20_temp;

  // The temperature, humidity, and pressure from the BME280 sensora
  union {
    float flotari;
    byte  bytes[4];
  } temperature;
  union {
    float flotari;
    byte  bytes[4];
  } humidity;
  union {
    float flotari;
    byte  bytes[4];
  } pressure;

  timestamp.uint = rtc.getEpoch();
  if (BME280_usable) {
    temperature.flotari = bme.readTemperature();
    humidity.flotari = bme.readHumidity();
    pressure.flotari = bme.readPressure() / 100.0F;
  } else {
    // If BME280 is not detected, send error values
    temperature.flotari = SENSOR_ERROR;
    humidity.flotari    = SENSOR_ERROR;
    pressure.flotari    = SENSOR_ERROR;
  }
  // Read the temperatures from the Dallas DS18B20 sensors
  sensors.requestTemperatures(); // Send the command to get temperatures

  // Print the data to the Serial interface
  char data[64];
  snprintf(data, sizeof(data),"T:%.2f H:%.2f P:%.2f", temperature.flotari, humidity.flotari,
		 pressure.flotari); 
  Serial.println("BME280 sensor data:");
  Serial.println(data);

  // Start a new LoRaWAN packet
  modem.beginPacket();

  // Write the sensor data to the packet
  modem.write(timestamp.bytes,   sizeof(timestamp));
  modem.write(temperature.bytes, sizeof(temperature));
  modem.write(humidity.bytes,    sizeof(humidity));
  modem.write(pressure.bytes,    sizeof(pressure));

  // Loop through each DS18B20 device, write out temperature data
  for(int i=0; i<numberOfDevices; i++) {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
		
		// Output the device ID
		Serial.print("Temperature for device: ");
		Serial.print(i,DEC);

    // Print the data
    ds18b20_temp.flotari = sensors.getTempC(tempDeviceAddress);
    Serial.print(" Temp C: ");
    Serial.println(ds18b20_temp.flotari);
    modem.write(ds18b20_temp.bytes, sizeof(ds18b20_temp));
    } 	
  }  
  
  
  // End the packet and send it
  // The argument to the endPacket function indicates whether we want a confirmation for the packet
  // In this case, we don't bother with the confirmation, so we pass false
  int packetSent = modem.endPacket(false);

  // Enter standby (deep sleep) mode
  // The microcontroller will wake up and restart the program when the alarm goes off
  Serial.print("Entering deep sleep at: ");
  Serial.print(rtc.getHours());
  Serial.print(":");
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  Serial.println(rtc.getSeconds());
  Serial.print("UNIX time: ");
  Serial.println(rtc.getEpoch());

  //Calculate the wakeup time (it takes one second to wake up and
  //process+send the measurements
  Time wakeUpTime = calculateWakeUpTime(SLEEP_TIME - 1);
  
  // Set the alarm to wake up
  rtc.setAlarmTime(wakeUpTime.hours, wakeUpTime.minutes, wakeUpTime.seconds);
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
  
  // Attach interrupt to RTC alarm to wake up
  rtc.attachInterrupt(alarmMatch);

  // Put the microcontroller into a deep sleep mode
  // In this mode, the microcontroller will consume very little power until it is woken up
  // In our case, it will wake up when the RTC alarm triggers
  rtc.standbyMode(); // Sleep until next alarm match  
  
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}

// Function to convert total seconds into hours, minutes, and seconds
Time convertSecondsToTime(int totalSeconds) {
  Time time;
  time.hours = totalSeconds / 3600;
  time.minutes = (totalSeconds % 3600) / 60;
  time.seconds = totalSeconds % 60;
  return time;
}

// Function to get the current time
Time getCurrentTime() {
  Time currentTime;
  currentTime.hours = rtc.getHours();
  currentTime.minutes = rtc.getMinutes();
  currentTime.seconds = rtc.getSeconds();
  return currentTime;
}

// Function to calculate the wake up time
Time calculateWakeUpTime(int sleepTimeInSeconds) {
  Time sleepTime = convertSecondsToTime(sleepTimeInSeconds);
  Time currentTime = getCurrentTime();
  
  int totalSeconds = (currentTime.hours * 3600 + currentTime.minutes * 60 + currentTime.seconds) 
                     + (sleepTime.hours * 3600 + sleepTime.minutes * 60 + sleepTime.seconds);
  
  return convertSecondsToTime(totalSeconds % (24 * 3600)); // Modulo by seconds in a day to roll over
}

void alarmMatch() {
  // This function will be called when the RTC alarm triggers
  // We don't need to do anything here because we just want to wake up from sleep
}
