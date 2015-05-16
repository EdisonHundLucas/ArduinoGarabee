#include <Wire.h> //I2C needed for sensors
#include <SoftwareSerial.h>
#include <string.h>
#include <DHT11.h>//Humidity sensor
#include <SFE_BMP180.h>//Pressure sensor
#include <avr/sleep.h>  
#include <LowPower.h>

SFE_BMP180 BMP180pressure;//Create an instance of the pressure sensor

//Hardware pin definitions
//#############################################################################
///////////// digital I/O pins ///////////////

const byte RAIN = 2;
const byte WSPEED = 3;
DHT11 dht11(4);
SoftwareSerial serial(19,18);
const byte RESET_PIN = 12;

///////////// analog I/O pins ////////////////

const byte WDIR = A0;
const byte LDR = A10;
const byte BATT = A2;
const byte REFERENCE_3V3 = A3;
//BPM180 SDA 20
//BPM180 SCL 21
//#############################################################################

//Global Variables
//#############################################################################

const int DELAYtime = 20000; 
const float sealevelPressure = 1013.25;

double BMP180temp = 0.0, BMP180pressao = 0.0, BMP180altitude = 0.0;
char status;

//long lastSecond; //The millis counter to see when a second rolls by
byte seconds = 0, minutes = 0; //When it hits 60, increase the current minute

long lastWindCheck = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;

volatile float rainHour[60]={0}; //60 floating numbers to keep track of 60 minutes of rain

//These are all the weather values that wunderground expects:
int winddir = 0; // [0-360 instantaneous wind direction]
float windspeedmph = 0; // [mph instantaneous wind speed]

float DHT11temp = 0.0, DHT11humi = 0.0, pontOrvalho = 0.0;
int LDRvalue = 0;

float rainin = 0; // [rain inches over the past hour)] -- the accumulated rainfall in the past 60 min
volatile float dailyrainin = 0; // [rain inches so far today in local time]

float batt_lvl = 11.8; //[analog value from 0 to 1023]

// volatiles are subject to modification by IRQs
volatile unsigned long raintime, rainlast, raininterval, rain;

//#############################################################################

void setup(){
  digitalWrite(RESET_PIN, HIGH);
  delay(200);
  
  Serial.begin(19200);
  serial.begin(19200);


  pinMode(RESET_PIN, OUTPUT);
 
  pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor
  
  pinMode(REFERENCE_3V3, INPUT);


  BMP180pressure.begin();

  //lastSecond = millis();

  // attach external interrupt pins to IRQ functions
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);

  // turn on interrupts
  interrupts();
  
  rainHour[0]=0;
}

void loop(){
    sleepNow();
    
    int i;
    //Serial.println(seconds);
    if(seconds > 59){
     
      seconds = 0;

      for(i = 58; i >= 0; i--){
         rainHour[i+1] = rainHour[i];
      }
      rainHour[0] = 0;
     }
    printWeather();
    seconds = seconds + DELAYtime/1000;
  
  //delay(2000);
}


//Interrupt routines (these are called by the hardware interrupts, not by the main code)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
void rainIRQ(){
  raintime = millis(); // grab current time
  raininterval = raintime - rainlast; // calculate interval between this and last event

    // ignore switch-bounce glitches less than 10mS after initial edge
    if (raininterval > 10){
      dailyrainin += 0.011; //Each dump is 0.011" of water
      rainHour[minutes] += 0.011; //Increase this minute's amount of rain
      rainlast = raintime; // set up for next event
    }
}

// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
void wspeedIRQ(){
  // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
  if (millis() - lastWindIRQ > 10){
    lastWindIRQ = millis(); //Grab the current time
    windClicks++; //There is 1.492MPH for each click per second.
  }
}

void sleepNow(){  
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  
} 

//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float get_battery_level(){
  // read the analog in value:
  float operatingVoltage = averageAnalogRead(REFERENCE_3V3);
   
  // Calculate the battery voltage value
  float rawVoltage = averageAnalogRead(BATT);
  
  operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V
  
  rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin
  
  rawVoltage *= 4.90; //(3.9k+1k)/1k - multiple BATT voltage by the voltage divider to get actual system voltage
  
  return(rawVoltage);
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(int pinToRead){
  byte numberOfReadings = 8;
  unsigned int runningValue = 0; 

  for(int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return(runningValue);  
}

//Returns the instataneous wind speed
float get_wind_speed(){
  float deltaTime = millis() - lastWindCheck; //750ms

  deltaTime /= 1000.0; //Covert to seconds

  float windSpeed = (float)windClicks / deltaTime; //3 / 0.750s = 4

  windClicks = 0; //Reset and start watching for new wind
  lastWindCheck = millis();

  windSpeed *= 1.492; //4 * 1.492 = 5.968MPH

  return(windSpeed);
}

//Read the wind direction sensor, return heading in degrees
int get_wind_direction(){
  unsigned int adc;

  adc = analogRead(WDIR); // get the current reading from the sensor

  // The following table is ADC readings for the wind direction sensor output, sorted from low to high.
  // Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
  // Note that these are not in compass degree order! See Weather Meters datasheet for more information.

  if (adc < 380) return (113);
  if (adc < 393) return (68);
  if (adc < 414) return (90);
  if (adc < 456) return (158);
  if (adc < 508) return (135);
  if (adc < 551) return (203);
  if (adc < 615) return (180);
  if (adc < 680) return (23);
  if (adc < 746) return (45);
  if (adc < 801) return (248);
  if (adc < 833) return (225);
  if (adc < 878) return (338);
  if (adc < 913) return (0);
  if (adc < 940) return (293);
  if (adc < 967) return (315);
  if (adc < 990) return (270);
  return (-1); // error, disconnected?
}


String getStringVal(double val){
  String stringVal = "";
  long decimalTest;
  decimalTest = getDecimal(val);
  if(decimalTest >= 10)
    stringVal+=String(long (val))+ "."+String(decimalTest);
  else
    stringVal+=String(long (val))+ "."+"0"+String(decimalTest);
  return stringVal;
}

//function to extract decimal part of float
long getDecimal(double val){
  int intPart = int(val);
  long decPart = 100*(val-intPart); //I am multiplying by 100 assuming that the foat values will have a maximum of 3 decimal places
                                   //Change to match the number of decimal places you need
  if(decPart>0)
    return(decPart);           //return the decimal part of float number if it is available    
  else if(decPart=0)
    return(00);           //return 0 if decimal part of float number is not available
}

// dewPoint function NOAA
// reference (1) : http://wahiduddin.net/calc/density_algorithms.htm
// reference (2) : http://www.colorado.edu/geography/weather_station/Geog_site/about.htm
//
double dewPoint(double celsius, double humidity){
// (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);

// factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity;

// (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP/0.61078);   // temp var
  return (241.88 * T) / (17.558 - T);
}


//Calculates each of the variables that wunderground is expecting
void calcWeather(){
  //Calc winddir
  winddir = get_wind_direction();

  windspeedmph = get_wind_speed();  
  
  dht11.read(DHT11humi, DHT11temp);

  LDRvalue = analogRead(LDR);

  //Total rainfall for the day is calculated within the interrupt
  //Calculate amount of rainfall for the last 60 minutes
  rainin = 0;  
  for(int i = 0 ; i < 60 ; i++){
    rainin += rainHour[i];    
  }

  //Calc battery level 4.2 fully charged, 3.2 very dead
  batt_lvl = get_battery_level();
//131.36 4.42
//Serial.println(batt_lvl);
  batt_lvl = 100-(((4.49 - batt_lvl)*100)/1.19);
  
  
  status = BMP180pressure.startTemperature();
  if (status != 0){
    delay(status);
    BMP180pressure.getTemperature(BMP180temp);
    // Oversampling: 0 to 3, higher numbers are slower, higher-res outputs.
    status = BMP180pressure.startPressure(3);
    if (status != 0){
      delay(status);
      BMP180pressure.getPressure(BMP180pressao,BMP180temp);
    } 
  }

 BMP180temp = BMP180temp - 2;
 DHT11temp = DHT11temp - 3;
 

 //http://www.sbmet.org.br/ecomac/pages/trabalhos/movimentos%20na%20atmosfera.txt

  BMP180altitude = abs(BMP180pressure.altitude(BMP180pressao,sealevelPressure));
 
  pontOrvalho =  dewPoint(BMP180temp,DHT11humi);  
  
}


//Prints the various variables directly to the port
//I don't like the way this function is written but Arduino doesn't support floats under sprintf
void printWeather(){
  calcWeather(); //Go calc all the various sensors

//  if(BMP180temp < 0){
//    digitalWrite(RESET_PIN, LOW);
//    Serial.println("reset");
//  }
   
  String strVal = "";
  strVal = getStringVal(BMP180temp) + " ";
  //strVal += getStringVal(DHT11temp) + " ";
  strVal += getStringVal(DHT11humi) + " ";
  strVal += getStringVal(BMP180pressao) + " ";
  strVal += getStringVal(LDRvalue) + " ";
  
  strVal += getStringVal(winddir) + " ";
  strVal += getStringVal(windspeedmph) + " ";
  strVal += getStringVal(rainin) + " ";
  strVal += getStringVal(BMP180altitude) + " ";
 
  strVal += getStringVal(pontOrvalho) + " ";
  strVal += getStringVal(batt_lvl) + " ";

  serial.println(strVal);

}


