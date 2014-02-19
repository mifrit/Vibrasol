/*
 VibraSol code for Arduino  - 
 Purpose: Hallux Valgus treatment/prevention by using your own muscles without orthotics.
 Disclaimer: Use at your own risk. Consult a podiatrist to calibrate variable "SENSOR_PRESSURE_LEVEL"
             To claibrate you can use the com port to monitor sensor output. 
 The circuit:
 -----------------------------------
 * 1.  3.3V Output - Resistive pressure sensor (placed in x position) - A0 input
 * 2.             inptut - 75K Ohm (purple red orange ) resistor - GND
 * 3.  Output 13 - Vibrator motor (flat coin type, in contact with skin, 5V) - GND
 
     _ _
   /     \                     (b) 
  |       |                    |
  |       |                    |
  |       |                  | | |
  |      |                   |(a)|
  (x)   |                    | | |
  |     |                   /  | |
  |     |             _  -     | | 
   \_ _/            (________(x)__)  
                      
 LEFT FOOTPRINT      LEG SIDE-VIEW

 (a) Strap Arduino (and vibrator) here
 (b) 4-pin usb cable to Nokia battery pack which is in a side pocket
 (x) Optimal placement of the pressure resisitve sensor to detect over-pronation. 
     Place on the opposite side (outer) if using right foot.
     One sensor on one leg is enough to give biofeedback if you mirror both legs.

 -----------------------------------
 Created by Jose Berengueres, Feb. 2014
 
 Pat. Pend. This code is in the public domain.
 Last revision v.4 of Feb 12th 2014
 Logs data to eeprom & debug keys by Michael Fritschi, Tiago Docilio
 +[mf, 19.2.14]: add settings for cheapduino (use Arduino NG/ATmega8 board settings)
 */


#include <EEPROM.h>

// Device specific definitions

#if defined(__AVR_ATmega8__)        // Settings for CheapDuino (Arduino NG/ATmega8A)
  #define EPROM_SIZE  511           // only 512bytes for cheapduino
  #define SENSOR_PIN  0             // Sensor imput pin
  #define MOTOR_PIN   9             // ...where the vibration motor is connected to
#else
  #define EPROM_SIZE  1023          // 1024 BYTES in ARDUINO UNO        
  #define SENSOR_PIN  0             // Sensor imput pin
  #define MOTOR_PIN   13            // ...where the vibration motor is connected to
#endif

// General definition/parameters

#define LIMIT                  30   // warn user if overpronated for longer that sampling_t x limit
#define OVERHEAD               4
#define SENSOR_PRESSURE_LEVEL  105  // controls min pressure to activate motor. It is sensor/shoe dependent
#define FOOT_ON_AIR            20   // ???
#define SAMPLE_TIME            300  // Sampling rate [ms]
#define LOG_RATE               15   // time delay for logging bad-count [minutes]


int p = 0;                          // variable to store the value coming from the sensor
int warncount = 0;                  // number warnings to user
int badcount = 0;                   // temporary overpronation counter
int cyclebadcount = 0;              // total number of samples where p < th for more thanbadcount
int elapsed = 0;                    // number samples elapsed
int log_addr = 0;                   // address index of last log
int p_zero = 0;

char whatsup;

boolean debug_flag = false;
boolean badposture = false;  // temp variable



//  EPROM MAP by BYTE
//     0     1         2      3     4    5  6  7  8  9 10 11 .... EPROM_SIZE_1023 
//   ( address )     (user id )    (stats log : 1 Byte per every 15 minutes   )

void writeW(int where, int x) {
  int b1 = x % 255;
  int b0 = x /255;
  EEPROM.write(where, b0);
  EEPROM.write(where + 1, b1);
}

int readW( int from) {
  return  255 * EEPROM.read(from) + EEPROM.read(from+1); 
}

// -- SETUP
void setup() {
  pinMode(MOTOR_PIN, OUTPUT); 
  Serial.begin(115200);

  log_addr = readW(0);
  Serial.print(   "number of logs in EEPROM = " );
  Serial.println(  log_addr - OVERHEAD    );
  
  Serial.print(   "user_id = " );
  Serial.println(  readW(2)    );
  hello();
  warn();
}

void hello(){
  vibrate(1,1000,1000);
}

void warn(){
  vibrate(2,255,255);
}

void malfunction(){
  vibrate(3,2000,300);
}

void mem_full_warn(){
  vibrate(2,2000,300);
}


void vibrate(int n,int b, int c) {
  for(int i=0;i<n;i++){
    digitalWrite(MOTOR_PIN, HIGH);
    delay(b);
    digitalWrite(MOTOR_PIN, LOW);
    delay(c);
    warncount ++;
    
  }
}

void logit(){
 
    if ( log_addr < EPROM_SIZE  ) {
       if (cyclebadcount > 255) {
          cyclebadcount = 255;
       }
       log_addr ++;
       writeW(0,log_addr);
       delay(10);
       EEPROM.write(log_addr, cyclebadcount);
       delay(10);
       if (debug_flag) {
         Serial.print("logging : ");
         Serial.print(cyclebadcount);
         Serial.print(" at ");
         Serial.println(log_addr);
       }
       cyclebadcount = 0;

    }else{
      if (debug_flag)
         Serial.println("MEM FULL(!) CANNOT LOG DATA");
      mem_full_warn();
    }

}

void sample() {
  delay(SAMPLE_TIME);
  listen();
  elapsed ++;
  p = analogRead(SENSOR_PIN);
  if (debug_flag) {
    Serial.print( "log_adr : "); 
    Serial.print( log_addr ); 
    Serial.print( " elapsed : "); 
    Serial.print( elapsed ); 
    //Serial.print( " p_zero : "); 
    //Serial.print( p_zero ); 
    Serial.print( " pressure : ");  
    Serial.println( p );
   
  }

  if ( elapsed*(SAMPLE_TIME/100)  > (LOG_RATE*60*10) ){ // compared in 10ths of seconds. int range is 32k check saturation!
    
    logit();
    elapsed = 0;
  }
  // --- END EEPROM -------------

}

void send_data_usart(){
  
  int i,j;
  
  Serial.println();

  Serial.print("log_addr :");
  Serial.print("\t");
  Serial.println(readW(0));

  Serial.print("User ID:");
  Serial.print("\t");
  Serial.println(readW(2));

  for (int i=OVERHEAD; i<=log_addr; i++){
    Serial.print(i-3,DEC);
    Serial.print("\t");
    Serial.println(EEPROM.read(i),DEC);
  }
  Serial.println();
}

void listen(){

  whatsup = Serial.read();
  
  switch(whatsup) {
    
    case 'D': //Debug on
      debug_flag = true;
      break;
    case 'd': //Debug off
      debug_flag = false;
      break;
    case 'h': //Dump stored values to USART
      send_data_usart();
      break;
    case '0': //Reset EEPROM
      log_addr = OVERHEAD;
      writeW( 0, log_addr );
      Serial.println("reset done");
      
      break;
  }
  

}

// -- MAIN LOOP

void loop() {
  
  sample();
 
  if (p < SENSOR_PRESSURE_LEVEL && !(p < FOOT_ON_AIR)) {
     badposture = true;
     badcount ++;

  }else{
     badposture = false;
     badcount = 0;

  }
  
  if (badcount > LIMIT) {
    cyclebadcount ++;
    badcount = 0;
    if (debug_flag){
      Serial.println( "Warning user ");
    }
    warn();
  }

  //check presure sensor makes contact
  /*
  if (p<1) {
    p_zero ++;
  }else{
    p_zero=0;
  }
  
  if (p_zero > 4* LIMIT) {
    p_zero = 0;
    if (debug_flag) {
       Serial.write("Is sensor conected?");
    }
    malfunction(); 
  }
  */

}

