int ThermistorPin = A0;
int Vo;
float R1 = 10000;
float logR2, R2, T, Tc, Tf;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

String inputstringpH = "";                              //a string to hold incoming data from the PC
String sensorstringpH = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_completepH = false;                //have we received all the data from the PC
boolean sensor_string_completepH = false;               //have we received all the data from the Atlas Scientific product
float pH;                                               //used to hold a floating point number that is the pH
volatile int NbTopsFan;                                 //measuring the rising edges of the signal
int Calc;
int hallsensor = 23;                                    //The pin location of the flow sensor

String inputstringDO = "";                              //a string to hold incoming data from the PC
String sensorstringDO = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_completeDO = false;                //have we received all the data from the PC
boolean sensor_string_completeDO = false;               //have we received all the data from the Atlas Scientific product
float DO;                                               //used to hold a floating point number that is the DO


void rpm ()                                           //This is the function that the interupt calls
{
  NbTopsFan++;                                          //This function measures the rising and falling edge of the hall effect sensors signal
}


void setup() {                                        //set up the hardware
  Serial.begin(9600);                                   //set baud rate for the hardware serial port_0 to 9600
  Serial2.begin(9600);                                  //set baud rate for software serial port_2 to 9600
  inputstringpH.reserve(10);                            //set aside some bytes for receiving data from the PC
  sensorstringpH.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product
  Serial3.begin(9600);                                  //set baud rate for software serial port_3 to 9600
  inputstringDO.reserve(10);                            //set aside some bytes for receiving data from the PC
  sensorstringDO.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product
  pinMode(hallsensor, INPUT);                           //initializes digital pin 23 as an input
  Serial.begin(9600);                                   //This is the setup function where the serial port is initialised,
  attachInterrupt(0, rpm, RISING);                      //and the interrupt is attached
}


void serialEvent() {                                  //if the hardware serial port_0 receives a char
  inputstringpH = Serial.readStringUntil(13);           //read the string until we see a <CR>
  input_string_completepH = true;                       //set the flag used to tell if we have received a completed string from the PC
  input_string_completeDO = true;                       //set the flag used to tell if we have received a completed string from the PC
}


void serialEvent3() {                                 //if the hardware serial port_3 receives a char
  sensorstringpH = Serial2.readStringUntil(13);         //read the string until we see a <CR>
  sensor_string_completepH = true;                      //set the flag used to tell if we have received a completed string from the PC
  sensorstringDO = Serial3.readStringUntil(13);         //read the string until we see a <CR>
  sensor_string_completeDO = true;                      //set the flag used to tell if we have received a completed string from the PC
}


void loop() {                                         //here we go...
 Vo = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  Tc = T - 273.15;
  Tf = (Tc * 9.0)/ 5.0 + 32.0; 

  Serial.print("Temperature: "); 
  Serial.print(Tf);
  Serial.print(" F; ");
  Serial.print(Tc);
  Serial.println(" C");   

  delay(500);

  if (input_string_completepH == true) {                //if a string from the PC has been received in its entirety
    Serial2.print(inputstringpH);                       //send that string to the Atlas Scientific product
    Serial2.print('\r');                                //add a <CR> to the end of the string
    inputstringpH = "";                                 //clear the string
    input_string_completepH = false;                    //reset the flag used to tell if we have received a completed string from the PC
  }


  if (sensor_string_completepH == true) {               //if a string from the Atlas Scientific product has been received in its entirety
    Serial.println("pH:");
    Serial.println(sensorstringpH);                     //send that string to the PC's serial monitor
   /*                                                   //uncomment this section to see how to convert the pH reading from a string to a float 
    if (isdigit(sensorstringpH[0])) {                   //if the first character in the string is a digit
      pH = sensorstringpH.toFloat();                    //convert the string to a floating point number so it can be evaluated by the Arduino
      if (pH >= 7.0) {                                  //if the pH is greater than or equal to 7.0
        Serial.println("high");                         //print "high" this is demonstrating that the Arduino is evaluating the pH as a number and not as a string
      }
      if (pH <= 6.99) {                                 //if the pH is less than or equal to 6.99
        Serial.println("low");                          //print "low" this is demonstrating that the Arduino is evaluating the pH as a number and not as a string
      }
    }
  */
  }
  sensorstringpH = "";                                  //clear the string:
  sensor_string_completepH = false;                     //reset the flag used to tell if we have received a completed string from the Atlas Scientific product


  NbTopsFan = 0;                                        //Set NbTops to 0 ready for calculations
  sei();                                                //Enables interrupts
  delay (1000);                                         //Wait 1 second
  cli();                                                //Disable interrupts
  Calc = (NbTopsFan * 60 / 7.5);                        //(Pulse frequency x 60) / 7.5Q, = flow rate in L/hour

  Serial.print ("flowrate:");
  Serial.print (Calc, DEC);                             //Prints the number calculated above
  Serial.print (" L/hour\r\n");                         //Prints "L/hour" and returns a  new line
  if (input_string_completeDO == true) {                //if a string from the PC has been received in its entirety
    Serial3.print(inputstringDO);                       //send that string to the Atlas Scientific product
    Serial3.print('\r');                                //add a <CR> to the end of the string
    inputstringDO = "";                                 //clear the string
    input_string_completeDO = false;                    //reset the flag used to tell if we have received a completed string from the PC
  }


  if (sensor_string_completeDO == true) {               //if a string from the Atlas Scientific product has been received in its entirety
    Serial.println("DO2:");
    Serial.println(sensorstringDO);                     //send that string to the PC's serial monitor
   /*                                                   //uncomment this section to see how to convert the D.O. reading from a string to a float 
    if (isdigit(sensorstringDO[0])) {                   //if the first character in the string is a digit
      DO = sensorstringDO.toFloat();                    //convert the string to a floating point number so it can be evaluated by the Arduino
      if (DO >= 6.0) {                                  //if the DO is greater than or equal to 6.0
        Serial.println("high");                         //print "high" this is demonstrating that the Arduino is evaluating the DO as a number and not as a string
      }
      if (DO <= 5.99) {                                 //if the DO is less than or equal to 5.99
        Serial.println("low");                          //print "low" this is demonstrating that the Arduino is evaluating the DO as a number and not as a string
      }
    }
  */
  }
  sensorstringDO = "";                                  //clear the string:
  sensor_string_completeDO = false;                     //reset the flag used to tell if we have received a completed string from the Atlas Scientific product
}
