/*
 * Spring_2022_Main_code
 *
 * GitHub: https://github.com/JackW291/EpicsUF
 * Contributer:
 * Jittipat Shobbakbun - winsatid62@gmail.com (If email me please say it is about EPICS-UF in the subject)
 * Jack
 * Alex
 *
 * Description:
 * This Arduino program is based on the Whitebox T2 Mini MkII (https://www.whiteboxes.ch/docs/tentacle/t2-mkII/#/).
 * The code is originally designed to read data from EZO sensors that is connected to the Whitebox T2 shield.
 * We have added more code for other sensors that we will use (see the list of sensors below). The code is structured
 * so the setup contain the setup for all sensors and the network system. The main loop contain (mostly) non blocking
 * functions that read the data of each sensor and the function that upload those data to ThingSpeak. The code put
 * all the sensor reading in global variables (see the list of reading global variables below). We use ThingSpeak as
 * the cloud that the Arduino upload the data to. The Arduino connect to the internet via ethernet cable connected
 * to the sheild.
 *
 * List of sensors:
 *  1. pH sensor                  - digital - use Ezo_i2c.h library
 *  2. DO Sensor                  - digital - use Ezo_i2c.h library
 *  3. Water temperature sensor   - analog  - It is a thermistor. Measure voltage to calculate resistance and temperature
 *  4. Humidity / Air temp sensor - digital - use DHT.h library
 *
 * List of functions:
 *  1.  void setup()       - default Arduino setup function
 *  2.  void loop()        - default Arduino loop function
 *  3.  void blink_led()   - blink LED every BLINK_FREQUENCY milliseconds
 *  4.  void read_ezo()    - read from EZO sensors
 *  5.  float receive_reading(Ezo_board &Sensor) - Take EZO sensor object, read the data and output it as float
 *  6.  void read_analog_temp(int temp_pin)      - Take analog pin number and read temperature probe connected to that pin
 *  7.  float old_temperature_code(float Vo)     - Take in voltage reading from temperature sensor and calculate temperature
 *  8.  void hum_read()         - Read humidity and air temperature from humidity sensor
 *  9.  void update_display()   - Output readings to serial port
 *  10. void upload_cloud()     - Upload to ThingSpeak every UPLOAD_FREQUENCY

 *
 * List of sensor readings (global variables):
 *  1. float pH_reading  - reading from pH sensor (desired range: 6.5 and 7.0)
 *  2. float _DO_reading - reading from dissolved oxygen sensor (desired range: 5.0 mg/L)
 *  3. float water_Tc    - Water temperature in Celsius of the temperature probe
 *  4. float water_Tf    - Water temperature in Fahrenheit of the temperature probe (desired range: 65 and 75 degrees)
 *  5. float humidity    - Humidity in the air in percent (Can read from 0-100)
 *  6. float air_Tc      - Air temperature in Celsius from humidity sensor
 *  7. float air_Tf      - Air temperature in Fahrenheit from humidity sensor
 *
 * --------------------------------------------------------------------------- *
 *
 *  GOALS:
 *  - Sensor alerts when the value is out of suitable ranges (found in sensors list above)
 *  - Detect faulty connection or sensors
 *      - Maybe EZO have something about checking if ther is a conencted sensor
 *      - Maybe check when values are impossibly out of ranges (Absolute zero, for example)
 *  - Power saving mode. Read sensors further aparts and only read every 15 seconds (fastest you can upload) when it detect some error. Maybe implement a sleep to arduino and only wake up every half an hour or some amount of time
 *  - Implement flow rate sensor - would need to know what flow rate sensor wil use.
 *
 *  STRETCH GOALS:
 *  - Automatically change pH by pumping safe acidic or basic solutions
 *  - Activate air pump when having insufficient dissolved oxygen
 *  - Control HVAC system automatically using temperature and
 *  - Maybe water heater too?
 */
#include <Ezo_i2c.h>                        // include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib
#include <Wire.h>                           // enable I2C.
#include <Ethernet.h>                       // for internet shield
#include "ThingSpeak.h"                     // always include thingspeak header file after other header files and custom macros
#include "DHT.h"                            // For humidity sensor


//--Status LED--
#define BLINK_FREQUENCY 250                 // the frequency of the led blinking, in milliseconds
unsigned long next_blink_time;              // holds the next time the led should change state
boolean led_state = LOW;                    // keeps track of the current led state


//--Network--
byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x40, 0x4F};

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(192, 168, 0, 1);

EthernetClient client;


//--ThingSpeak--
unsigned long myChannelNumber = 1646406;
const char * myWriteAPIKey = "PI0UW7PG786YHESL";
// timer
#define UPLOAD_FREQUENCY 20000              // frequency of uploading to ThingSpeak in milliseconds (ThingSpeak only allow upload every 15 seconds (15000 millis))
unsigned long next_upload = 0;              // time to upload the next set of data
// Code for calculating time interval between each updates
unsigned long last_upload = 0;              // Time of the last update


//--Serial display--
// setups
#define DISPLAY_INDIVIDUAL false            // true to display individual readings as soon as they come in, false to display only aggregated one
#define DISPLAY_DEBUG true                  // true to display debug massages, false to not display them
// timer
#define DISPLAY_FREQUENCY 1000              // frequency to print data of all sensors to Serial
unsigned long next_display = 0;             // variable keeping track of when to display the data next


//--EZO Sensors (pH and Disolved oxygen)--
// setups
Ezo_board ph = Ezo_board(99, "PH");         //create a PH circuit object who's address is 99 and name is "PH"
Ezo_board _do = Ezo_board(97, "DO");        //create a DO circuit object who's address is 97 and name is "DO"
// globals
float pH_reading;                           // global to hold pH
float _DO_reading;                          // global to hold DO
// timer
#define EZO_FREQUENCY 1000                  // frequency of checking EZO sensor (EZO sensors need around 1000 milliseconds to process the data, so this should be more than that)
unsigned long next_ezo_time = 0;            // holds the time when the next EZO reading is due
// misc.
boolean request_pending = false;            // wether we've requested a reading from the EZO devices


//--Water temperature--
// setups
#define TEMP_PIN 0                                  // the ANALOG pin of the analog port of the temperature probe
const long RESISTOR_RESISTANCE = 9.78 * 1000;       // the resistance of the resistor connected serially with the temperature sensor
const float ARDUINO_VOLTAGE = 4.74;                 // the measured voltage of the arduino five volt
// globals
float water_Tc;                             // global to hold water temperature in Celsius
float water_Tf;                             // global to hold water temperature in Fahrenheit
float probe_voltage;                        // voltage reading from the analog pin
float probe_resistance;                     // resistance reading from the analog pin
// timer
#define TEMP_CHECK_FREQUENCY 500            // the amount of time between each temperature read, in milliseconds (No minimum)
unsigned long next_temp_check_time;         // holds the next time the program read the temperature
// misc.
const float C1 = 1.009249522e-03, C2 = 2.378405444e-04, C3 = 2.019202697e-07;       // constants for temperature conversion


//--Humidity and air temperature--
// setups
#define DHTPIN 2                            // what pin we're connected to (Digital)
#define DHTTYPE DHT22                       // DHT 22  (AM2302)
DHT hum_sen(DHTPIN, DHTTYPE);               // humidity sensor DHT object
// globals
float humidity;                             // global to hold humidity
float air_Tc;                               // global to hold air temperature in Celsius
float air_Tf;                               // global to hold air temperature in Fahrenheit
// timer
#define HUMIDITY_FREQUENCY 1000             // frequency to check the humidity (It need around 250 milliseconds to process between each reading)
unsigned long next_hum_time = 0;            // next time to check humidity


void setup() {
  Serial.begin(9600);                       // set the hardware serial port.
  Serial.println("Starting setup...");
  Ethernet.init(10);                        // most Arduino Ethernet hardware
  pinMode(13, OUTPUT);                      // set the led output pin
  Wire.begin();                             // enable I2C port.
  hum_sen.begin();                          // set the dht for humidity sensor

  // Set up the ethernet connection
  //Serial.println("Initialize Ethernet with DHCP:");
  Serial.println("Connecting to the Internet...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :( [Please reset]");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    else {
      // try to congifure using IP address instead of DHCP:
      Serial.println("Cofigure using IP address...");
      Ethernet.begin(mac, ip, myDns);
    }
  }
  else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  Serial.println("Initializing...");
  delay(1000);

  // ThingSpeak
  Serial.println("Connecting to ThingSpeak...");
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  // Set the start read times. The code will check after waiting for sensor to start up and measure the data
  next_ezo_time = millis() + EZO_FREQUENCY;
  next_temp_check_time = millis() + TEMP_CHECK_FREQUENCY;
  next_hum_time = millis() + HUMIDITY_FREQUENCY;
  last_upload = millis();
  next_upload = millis() + UPLOAD_FREQUENCY;
  next_display = millis();                                      // display immediately
}


void loop() {
  // none of these functions block or delay the execution
  // read sensors
  read_ezo();
  read_analog_temp(TEMP_PIN);
  hum_read();
  // blink status update
  blink_led();
  // Output to serial
  update_display();
  // Upload to cloud
  upload_cloud();
}


// blinks a led on pin 13. this function returns immediately, if it is not yet time to blink
void blink_led() {
  if (millis() >= next_blink_time) {              // is it time for the blink already?
    led_state = !led_state;                       // toggle led state on/off
    digitalWrite(13, led_state);                  // write the led state to the led pin
    next_blink_time = millis() + BLINK_FREQUENCY; // calculate the next time a blink is due
  }
}


// take sensor readings every EZO_FREQUENCY. this function returns immediately, if it is not time to interact with the EZO devices.
void read_ezo() {
  if (request_pending) {                    // is a request pending?
    if (millis() >= next_ezo_time) {        // is it time for the reading to be taken?
      // Get the outputs and print the output in serial
      pH_reading = receive_reading(ph);                   // get the reading from the PH circuit
      if (DISPLAY_INDIVIDUAL) {
        Serial.print("  ");
      }
      _DO_reading = receive_reading(_do);                 // get the reading from the DO circuit
      if (DISPLAY_INDIVIDUAL) {
        Serial.println();
      }

      request_pending = false;
      next_ezo_time = millis() + EZO_FREQUENCY;
    }
  } else {                                  // it's time to request a new reading
    ph.send_read_cmd();
    _do.send_read_cmd();
    request_pending = true;
  }
}


// function to decode the reading after the read command was issued
float receive_reading(Ezo_board &Sensor) {

  if (DISPLAY_INDIVIDUAL) {                               // Check if we want to display individual data
    Serial.print(Sensor.get_name()); Serial.print(": ");  // print the name of the circuit getting the reading
  }
  Sensor.receive_read_cmd();                              // get the response data

  if (DISPLAY_INDIVIDUAL) {
    switch (Sensor.get_error()) {                         // switch case based on what the response code is.
      case Ezo_board::SUCCESS:
        Serial.print(Sensor.get_last_received_reading()); //the command was successful, print the reading
        break;

      case Ezo_board::FAIL:
        Serial.print("Failed ");                          //means the command has failed.
        break;

      case Ezo_board::NOT_READY:
        Serial.print("Pending ");                         //the command has not yet been finished calculating.
        break;

      case Ezo_board::NO_DATA:
        Serial.print("No Data ");                         //the sensor has no data to send.
        break;
    }
  }

  return Sensor.get_last_received_reading();              //return the sensor reading
}


// function to read the temperature from temperature probe. Return if it is not the time to do it yet.
void read_analog_temp(int temp_pin) {

  if (millis() >= next_temp_check_time) {                         // is it the time to check temperature
    float temp_voltage;
    temp_voltage = analogRead(temp_pin);                          // read voltage from analog pin
    probe_voltage = (temp_voltage/1023.0)*ARDUINO_VOLTAGE;        // set global variable for the probe sensor

    if (DISPLAY_INDIVIDUAL) {
      Serial.print("Temperature sensor: ");                       // print label for temperature reading to serial port
    }
    float Tc, Tf;                                                 // variables declaration for the temperature calculation

    Tc = old_temperature_code(temp_voltage);                      // run old code
    Tf = (Tc * 9.0)/ 5.0 + 32.0;
    if (DISPLAY_DEBUG && DISPLAY_INDIVIDUAL) {
      Serial.print("Temp sensor voltage: ");
      Serial.println(probe_voltage);                              // print actual voltage of the sensor
    }

    water_Tc = Tc;                                                // set global for water temperature in Celsius
    water_Tf = Tf;                                                // set global for water temperature in Fahrenheit
    next_temp_check_time = millis() + TEMP_CHECK_FREQUENCY;       // set the next time to read the temperature probe
  }
}


// function to calculate temperature, Taken from an old UF team Arduino code (Fall 2020)
float old_temperature_code(float Vo) {
  float logR2, R2, T, Tc, Tf;                               // variables declaration for the temperature calculation
  // Calculate the temperature from the voltage
  R2 = RESISTOR_RESISTANCE * ((1023.0 / (float)Vo) - 1.0);
  logR2 = log(R2);
  T = (1.0 / (C1 + C2*logR2 + C3*logR2*logR2*logR2));       // Used linear regression to figure this out from testing
  // Convert them to Celsius and Fahrenheit
  Tc = T - 273.15;
  Tf = (Tc * 9.0)/ 5.0 + 32.0;

  // Display the data
  if (DISPLAY_INDIVIDUAL) {
    Serial.print("Temperature: ");
    Serial.print(Tf);
    Serial.print(" F; ");
    Serial.print(Tc);
    Serial.println(" C");
  }

  return Tc;
}


// Function for reading humidity sensor.
void hum_read() {
  // adapted from the code from
  if (millis() >= next_hum_time) {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = hum_sen.readHumidity();
    // Read temperature as Celsius
    float Tc = hum_sen.readTemperature();
    float Tf = (Tc * 9.0)/ 5.0 + 32.0;

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(Tc)) {
      if (DISPLAY_INDIVIDUAL) {
        Serial.println("Failed to read from DHT sensor!");
      }
    }
    else {
      air_Tc = Tc;                        // set global air temperature in Celsius
      air_Tf = Tf;                        // set global air temperature in Fahrenheit
      humidity = h;                       // set global humidity
      // Print the output to serial monitor
      if (DISPLAY_INDIVIDUAL) {
        Serial.print("Humidity: ");
        Serial.print(h);
        Serial.print(" %\t");
        Serial.print("Temperature: ");
        Serial.print(Tc);
        Serial.println(" *C ");
      }
    }

    next_hum_time = millis() + HUMIDITY_FREQUENCY;        // set the next read time
  }
}


// update the Serial output
void update_display() {
  if (millis() >= next_display) {                       // check if it is time to display data
    next_display = millis() + DISPLAY_FREQUENCY;        // if it is, reset the clock to the next time
    Serial.println();
    Serial.println("Readings:");                        // Print title of the display readings
    // pH
    Serial.print("pH: ");
    Serial.println(pH_reading);
    // DO
    Serial.print("DO: ");
    Serial.println(_DO_reading);
    // Water temperature
    Serial.print("Water temperature: ");
    Serial.print(water_Tf);
    Serial.println(" F");
    if (DISPLAY_DEBUG) {                                // check if we want to display debug information
      // Temperature probe voltage
      Serial.print("Probe voltage: ");
      Serial.print(probe_voltage);
      Serial.println(" V");
    }
    // Air temperature
    Serial.print("Air temperature: ");
    Serial.print(air_Tf);
    Serial.println(" F");
    // Humidity
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  }
}


// upload data to cloud
void upload_cloud() {
  if (millis() >= next_upload) {
    next_upload = millis() + UPLOAD_FREQUENCY;
    if (DISPLAY_DEBUG) {
      Serial.println("Uploading data...");
    }
    // Set data into ThingSpeak fields
    ThingSpeak.setField(1, pH_reading);
    ThingSpeak.setField(2, _DO_reading);
    ThingSpeak.setField(3, water_Tf);
    ThingSpeak.setField(5, humidity);
    ThingSpeak.setField(6, air_Tf);

    unsigned long time_interval = 0;                                    // variable for keeping track of how long the last successful update was
    // write fields to the ThingSpeak channel
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (DISPLAY_DEBUG) {
      if(x == 200) {
        Serial.println("Channel update successful.");
        // calculate the time since last upload
        time_interval = millis() - last_upload;
        last_upload = millis();
        // display the time since last upload
        if (DISPLAY_DEBUG) {
          Serial.print("Last successful upload was ");
          Serial.print(time_interval);
          Serial.println("milliseconds ago.");
        }
      }
      else {
        Serial.println("Problem updating channel. HTTP error code " + String(x));
      }
    }
  }
}
