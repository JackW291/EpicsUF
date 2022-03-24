#include <Ezo_i2c.h>                        //include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib
#include <Wire.h>                           // enable I2C.
#include <Ethernet.h>
#include "ThingSpeak.h"                     // always include thingspeak header file after other header files and custom macros

Ezo_board ph = Ezo_board(99, "PH");         //create a PH circuit object who's address is 99 and name is "PH"
Ezo_board _do = Ezo_board(97, "DO");        //create a DO circuit object who's address is 97 and name is "DO"

unsigned long next_ezo_time = 0;            // holds the time when the next EZO reading is due
boolean request_pending = false;            // wether we've requested a reading from the EZO devices

const int TEMP_PIN = A15;                   // the pin of the analog port of the temperature probe
const unsigned int BLINK_FREQUENCY = 250;           // the frequency of the led blinking, in milliseconds
const unsigned long TEMP_CHECK_FREQUENCY = 500;     // the amount of time between each temperature read, in milliseconds

const long RESISTOR_RESISTANCE = 9.78 * 1000;       // the resistance of the resistor connected serially with the temperature sensor
const float ARDUINO_VOLTAGE = 4.74;                 // the measured voltage of the arduino five volt

const float C1 = 1.009249522e-03, C2 = 2.378405444e-04, C3 = 2.019202697e-07;       // constants for temperature conversion

unsigned long next_blink_time;              // holds the next time the led should change state
unsigned long next_temp_check_time;         // holds the next time the program read the temperature
boolean led_state = LOW;                    // keeps track of the current led state

byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x40, 0x4F};

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(192, 168, 0, 1)

EthernetClient client;

unsigned long myChannelNumber = 000000;
const char * myWriteAPIKey = "XYZ";

void setup() {
  Ethernet.init(10);                        // Most Arduino Ethernet hardware
  pinMode(13, OUTPUT);                      // set the led output pin
  Serial.begin(9600);                       // Set the hardware serial port.
  Wire.begin();                             // enable I2C port.

  next_ezo_time = millis() + 1000;

  //Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns);
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

  ThingSpeak.begin(client);  // Initialize ThingSpeak
}

void loop() {
  // none of these functions block or delay the execution
  read_ezo();
  read_analog_temp(TEMP_PIN);
  blink_led();
  update_display();
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

// take sensor readings every second. this function returns immediately, if it is not time to interact with the EZO devices.
void read_ezo() {
int ph_reading = 0;
int _do_reading = 0;
  if (request_pending) {                    // is a request pending?
    if (millis() >= next_ezo_time) {        // is it time for the reading to be taken?
      ph_reading = receive_reading(ph);                  // get the reading from the PH circuit

//***Not sure if the language works this way, watch to make sure this works as expected

      Serial.print("  ");
      ThingSpeak.setField(1, ph_reading);
      _do_reading = receive_reading(_do);                 // get the reading from the DO circuit
      Serial.println();
      ThingSpeak.setField(2, _do_reading);

      request_pending = false;
      next_ezo_time = millis() + 1000;
    }
  } else {                                  // it's time to request a new reading

    ph.send_read_cmd();
    _do.send_read_cmd();
    request_pending = true;
  }
}

// update the display
void update_display() {

  // add your display code here

}

// upload data to cloud
void upload_cloud() {

// write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

// function to decode the reading after the read command was issued
void receive_reading(Ezo_board &Sensor) {

  Serial.print(Sensor.get_name()); Serial.print(": ");  // print the name of the circuit getting the reading
  Sensor.receive_read_cmd();                            // get the response data

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

// function to read the temperature
void read_analog_temp(int temp_pin) {

  if (millis() >= next_temp_check_time) {                         // is it the time to check temperature
    float temp_voltage;
    temp_voltage = analogRead(temp_pin);                          // read voltage from analog pin
    Serial.print("Temperature sensor: ");                         // print label for temperature reading to serial port
    float Tc, Tf;                                                 // variables declaration for the temperature calculation

    Tc = old_temperature_code(temp_voltage);                      // run old code
    Tf = (Tc * 9.0)/ 5.0 + 32.0;
    ThingSpeak.setField(3, Tf);
    Serial.print("Temp sensor voltage: ");
    Serial.println((temp_voltage/1023.0)*ARDUINO_VOLTAGE);        // print actual voltage of the sensor

    next_temp_check_time = millis() + TEMP_CHECK_FREQUENCY;       // set the next time to read the temperature probe
  }
}

// function to calculate temperature from the old code (Fall 2020)
float old_temperature_code(float Vo) {

  // Section from the old code
  float logR2, R2, T, Tc, Tf;                               // variables declaration for the temperature calculation
  R2 = RESISTOR_RESISTANCE * ((1023.0 / (float)Vo) - 1.0);
  logR2 = log(R2);
  T = (1.0 / (C1 + C2*logR2 + C3*logR2*logR2*logR2));
  Tc = T - 273.15;
  Tf = (Tc * 9.0)/ 5.0 + 32.0;

  Serial.print("Temperature: ");
  Serial.print(Tf);
  Serial.print(" F; ");
  Serial.print(Tc);
  Serial.println(" C");

  return Tc;
}
