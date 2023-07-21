# CabinWatcher
Arduino Sketch for LoRaWan cabin watcher device

The aim of the sketch is to demonstrate sending data via LoRaWAN
network.  The demonstration device can be used for e.g. monitoring the
temperatures and humidity of a holiday cottage, but there are many
commercial devices which do it better.  So, if you need to transfer
any data from your own measurement sensors, here is an example code.
At minimum one should edit the AppEui and AppKey to match those used
in the device activation process of the LoRaWan service provider.
Please consult your service provider, if unsure.

The Sketch is written for Arduino MRK WAN 1310 with one Crowtail
BME280 sensor and one or more DS18B20 sensors installed.  It may work
with other BME280 sensors also, but it was tested only with Crowtail.
The DS18B20 are initialized to use the most accurate 12bit mode.

Current version is rather primitive.  It sets the Arduino board to
StandBy mode of the RTCZero library, and wakes four times per hour to
read the sensor values and to send them to the LoRaWan gateway.  It
doesn't care whether the transmit goes through or not.  If BME280
device is not detected in startup, the error value -1e6 will be sent
instead of measured values.  All traffic is one directional: the
device does not receive any data after the initial LoRaWAN OTAA join.

The transmitted data is in binary format.  The first four bytes
contain the RTC time stamp of the measurements in Unix time stamp
format, as little endian unsigned int32.  The next 12 bytes are three
little endian float32 values coming from the BME280 (temperature in
Celsius, humidity in percents, and pressure in mbar).  The rest are
little endian float32 values from the DS18B20 sensors.


