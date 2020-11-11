//QualiaSG's Atmega FDC for 3.5" FDDs with 34-pin connector. 11th Nov 2020. GPLv3.
//https://github.com/QualiaSG

//SAMSUNG's SFD-321B Datasheet: https://cdn.datasheetspdf.com/pdf-down/S/F/D/SFD-321B-Samsung.pdf
//Mitsubishi's MF355B Datasheet: http://www.bitsavers.org/pdf/mitsubishi/floppy/MF355/UGD-0489A_MF355B_Specifications_Sep86.pdf
//NEC's FD1035 Datasheet: http://www.bitsavers.org/components/nec/FD1035_Product_Description_Jul84.pdf
//Microchip's Atmel Atmega328P Datasheet: https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf

//OUTPUTs
#define   DRIVE_SELECT1       3    //PD3, FDD pin 12
#define   MOTOR_ON            4    //PD4, FDD pin 16
#define   DIRECTION_SELECT    5    //PD5, FDD pin 18
#define   STEP                6    //PD6, FDD pin 20
#define   WRITE_DATA          7    //PD7, FDD pin 22
#define   WRITE_GATE          9    //PB1, FDD pin 24
#define   SIDE_SELECT         10   //PB2, FDD pin 32

//INPUTs
#define   INDEX               2    //PD2, FDD pin 8
#define   READ_DATA           8    //PB0, FDD pin 30
#define   TRACK_00            A0   //PC0, FDD pin 26
#define   WRITE_PROTECT       A1   //PC1, FDD pin 28
#define   READY_DISK_CHANGE   A2   //PC2, FDD pin 34
#define   BUTTON              A3   //PC3, External button for debugging

//Global variables
unsigned char current_track = 0;   //1 byte to store current track

void setup() {
  //Set pins direction
  DDRD = 0xFF ^ B100;   //Set 0-7 as outputs except 2 (PD2)
  DDRB = 0xFF ^ B1;     //Set 8-13 as outputs except 8 (PB0)
  DDRC = 0x00;          //Set A0-A5 as inputs
  PORTD = 0xFF;   //Enable internal pullups
  PORTB = 0xFF;   //Enable internal pullups
  PORTC = 0xFF;   //Enable internal pullups
  /*Note: External pullups are still recommended, internall pull-ups are not
    active during reset or code upload, and the FDD might WRITE to your floppy disk.*/
  /*Outputs from FDDs are open-collector, they can only SINK current.*/
  //Set pin status, initial status
  digitalWrite(DRIVE_SELECT1, HIGH);    //OFF, drive is not selected
  digitalWrite(MOTOR_ON, HIGH);         //OFF, motor not spinning
  digitalWrite(DIRECTION_SELECT, HIGH); //TO OUTER TRACKS, head direction
  digitalWrite(STEP, HIGH);             //OFF, no steps, head not moving
  digitalWrite(WRITE_DATA, HIGH);       //OFF (Set as READ)
  digitalWrite(WRITE_GATE, HIGH);       //OFF (Set as READ OR SEEK)
  digitalWrite(SIDE_SELECT, HIGH);      //LOWER SIDE (SIDE 0) selected
  //Open serial port
  delay(1); //Otherwise noise appears on terminal
  Serial.begin(115200);
  Serial.println(F("--QualiaSG's Atmega FDC--"));
  digitalWrite(DRIVE_SELECT1, LOW); //Enable the FDD
  //We set our global variable 'current_track' to 0, so let's move the head to track 0
  Find_Track_0();
}

void loop() {
  static boolean FDD_empty = true;
  if (!digitalRead(READY_DISK_CHANGE)) {
    Serial.println(F("FDD is empty."));
    FDD_empty = true;
    while (!digitalRead(READY_DISK_CHANGE)) {
      digitalWrite(STEP, LOW);
      delayMicroseconds(1);
      digitalWrite(STEP, HIGH);
      delay(500);
    }
  } else {
    Serial.println(F("Diskette inserted."));
    FDD_empty = false;
    while (digitalRead(READY_DISK_CHANGE)) {
      while (digitalRead(BUTTON)) {
        digitalWrite(MOTOR_ON, LOW);
        //delay(500);
        //Steps();
        GotoTrack(1);
        GotoTrack(15);
        GotoTrack(1);
        GotoTrack(45);
        //Serial.print(digitalRead(TRACK_00),HEX);
        //delay(10);
      }
      readTracktest();
      digitalWrite(MOTOR_ON, HIGH);
    }
  }


  digitalWrite(STEP, HIGH);
  delay(1);
  digitalWrite(STEP, HIGH);
}


//Go to track 0
void Find_Track_0 () {
  unsigned int i = 0;
  digitalWrite(DIRECTION_SELECT, HIGH);
  while (i < 90 || digitalRead(TRACK_00)) {
    digitalWrite(STEP, LOW);
    //delay(1);
    delayMicroseconds(1);
    digitalWrite(STEP, HIGH);
    i++;
    delay(3);
  }
  if (digitalRead(TRACK_00) == LOW) {
    Serial.println("FDD set at Track 0.");
  } else {
    Serial.println("Couldn't find Track 0.");
  }
}


//Call OneStep(1) to move one step to inner tracks (increase track),
//Call OneStep(0) to move one step to outer tracks (decrease track).
void OneStep(boolean increase_track) {
  if (increase_track && current_track < 82) {         //Only increase if track is less than 82
    if (digitalRead(DIRECTION_SELECT))                  //If we are changing direction..
      delay(15);                                        //..comply '18ms turn-around time'.
    digitalWrite(DIRECTION_SELECT, LOW);              //Set direction to: inner tracks
    current_track++;
  } else if (!increase_track && current_track > 0) {  //Only decrease if track is more than 0
    if (!digitalRead(DIRECTION_SELECT))                  //If we are changing direction..
      delay(15);                                        //..comply '18ms turn-around time'.
    digitalWrite(DIRECTION_SELECT, HIGH);             //Set direction to: outer tracks
    current_track--;
  } else if (current_track == 0) {                    //If current_track is 0
    if (!digitalRead(TRACK_00) == LOW) {              //Check we are in Track 0 just in case
      Find_Track_0();                                 //Go to track 0 if we weren't on it
      Serial.println("Head was misaligned!");         //Report head was probably misaligned
    }
  } else {                                            //Do nothing
    return;                                           //Exit function, don't do any step
  }
  digitalWrite(STEP, LOW);
  delayMicroseconds(1);                               //Drives require a minimum
  digitalWrite(STEP, HIGH);                           //Step is issued now, on rising edge
  delay(3);                                           //3ms are required between steps
  //Remember that 18ms are required before reading or writing so the head has time to settle.
}


//Self-descriptive, use from 0 to 81, '81' is track #82, '0' is track #1.
void GotoTrack(unsigned char goto_track) {
  Serial.print("Go from Track ");
  Serial.print(current_track);
  Serial.print(" to track ");
  Serial.println(goto_track);
  if (goto_track > 82) return;
  if ((goto_track - current_track) >= 0) {
    unsigned char steps = (goto_track - current_track);
    for (unsigned char i = 0; i < steps; i++) {
      OneStep(1); //Increase track
    }
  } else {
    unsigned char steps = (current_track - goto_track);
    for (unsigned char i = 0; i < steps; i++) {
      OneStep(0); //Decrease track
    }
  }
  Serial.print("Current track:");
  Serial.print(current_track);
  Serial.print(" (Track #");
  Serial.print(current_track + 1);
  Serial.println(")");
}


//Turn on or off disk spinning
void SetSpindle(boolean spin) {
  if (spin) {
    digitalWrite(MOTOR_ON, LOW);
    Serial.println("Motor turned on.");
    delay(500);
  } else {
    digitalWrite(MOTOR_ON, HIGH);
    Serial.println("Motor turned off.");
  }
}


//Unfinished code from this point
void readTracktest () {
  __attribute__((optimize("-O2")));;
  Serial.println(F("Read Track Test."));

  /* Configure Input Capture on Atmega328P
    ICP1 is the input capture pin, which is PB0, or pin 8 on Arduino.
    ICP1 acts as an input capture pin for Timer/Counter1.
    We need to configure Timer/Counter1, for that we use TCCR1A and TCCR1B registers.
    TCCR1A and TCCR1B are Timer/Counter1 Control Registers, 8-bit each.
    These registers set Timer/Counter1 mode of operation. 15.11 in datasheet.
  */
  //Trigger capture event on falling edge in ICP1 (pin READ_DATA, PB0)
  TCCR1B &= ~_BV(ICES1); //TCCRB1B = TCCRB1B & ~(1 << ICES1) -> We set ICES1 bit to 0
  //Disable input capture noise canceller
  TCCR1B &= ~_BV(ICNC1); //ICNC1 bit to 0
  //Set Timer/Counter1 to 'no prescaling'
  TCCR1B &= ~_BV(CS12); //CS12 bit to 0
  TCCR1B &= ~_BV(CS11); //CS11 bit to 0
  TCCR1B |=  _BV(CS10); //CS10 bit to 1
  //Set Timer/Counter1 to 'Normal' operation
  TCCR1A &= ~_BV(WGM10); //WGM10 bit to 0
  TCCR1A &= ~_BV(WGM11); //WGM11 bit to 0
  TCCR1B &= ~_BV(WGM12); //WGM12 bit to 0
  TCCR1B &= ~_BV(WGM13); //WGM13 bit to 0
  //Make sure Analog Comparator is disconnected
  ACSR &= ~_BV(ACIC); //ACIC bit to 0

  /* Input Capture
    When en event is captured on ICP1 (PB0), the value of TCNT1 is copied to ICR1.
    The flag bit ICF1 is also set for every ICP1 event.
    TCNT1 is Timer/Counter1. With no prescaling, it increases 1 unit every clock. 16-bit.
    ICR1 is Input Capture Register, also 16-bit.
    TIFR1 is Timer/counter1 Interrupt Flag register. Stores flags.
  */
  TIFR1  =  _BV(ICF1) | _BV(OCF1A) | _BV(TOV1); //Clear all timer flags
  
  uint16_t capture_data_mfm[600]; //The array for storing ICR1 values.
  /* ^Note that we are using an array of 16-bit values, because ICR1 is 16-bit. */
  unsigned char sreg;
  register uint16_t count = 0;
  register unsigned char byte_from_mfm = 0;
  register unsigned int sync_count = 0;
  unsigned int sync_pattern[6] = {48, 48, 48, 32, 32, 48};
  //We turn on the motor, go to track 0, and wait for index pulse.
  SetSpindle(1);
  Find_Track_0();
  OneStep(1);OneStep(1);
  Serial.println(F("Wait for INDEX pulse."));

  sreg = SREG;  //Save global interrupt flag
  cli();        //Disable interrupts IRQs

  //while (PINB != B00001110);  //Wait for index pulse

  while (!digitalRead(INDEX));      //If INDEX is 0 we wait
  while (digitalRead(INDEX));       //Wait until INDEX becomes 0
  __builtin_avr_delay_cycles(30000);//For testing
  
  //The capture of MFM pulses happens here
  do {
    TIFR1 = _BV(ICF1);              //Clear input capture flag
    while (!(TIFR1 & _BV(ICF1)));   //Wait until flag is set (event happened)
    TCNT1 = 0;                      //Set the Timer/Counter1 to 0
    capture_data_mfm[count] = ICR1; //Copy Input Capture Register to array
    count++;                        //Increase array counter
  } while (count < 599);            //Repeat until array if full

  //Unfinished code
  if (ICR1 < 32) {

  } else if (ICR1 < 48) {
    sync_count++;
  } else {
    sync_count = 0;
  }

  
  SREG = sreg; //Restore global interrupt flag
  //sei();

  SetSpindle(0);

  //Display captured data
  Serial.println(F("Capture data:"));
  for (unsigned int i = 1; i < 599; i++) {
    Serial.print(capture_data_mfm[i], DEC);
    if (capture_data_mfm[i] < 10) Serial.print("  -> ");
    if (capture_data_mfm[i] >= 10) Serial.print(" -> ");
    Serial.print((int)(62.5 * capture_data_mfm[i]));
    Serial.print(" ns - ");
    if ((int)(62.5 * capture_data_mfm[i]) < 2000) Serial.println("A");
    if ((int)(62.5 * capture_data_mfm[i]) >= 2000 && (int)(62.5 * capture_data_mfm[i]) < 3000)
      Serial.println(" B");
    if ((int)(62.5 * capture_data_mfm[i]) >= 3000) Serial.println("  C");
    delay(0);
    if (i == 3) delay(950); if (i == 8) delay(950); if (i == 10) delay(1250);
  }
  
  //  //Display captured data in MFM pulses ('10','100', or '1000')
  //  Serial.println(F("Capture data in MFM as 0s or 1s:"));
  //  for (unsigned int i = 0; i < 599; i++) {
  //    if ((int)(62.5*capture_data_mfm[i]) < 2000) Serial.println("10");
  //    if ((int)(62.5*capture_data_mfm[i]) >= 2000 && (int)(62.5*capture_data_mfm[i]) < 3000)
  //      Serial.println("100");
  //    if ((int)(62.5*capture_data_mfm[i]) >= 3000) Serial.println("1000");
  //    delay(50);
  //  }

  //MFM bit sync (Pattern BBBAAB or 100 100 100 10 10 100 or 0x4E)
  unsigned int sync = 0;
  unsigned int sync_i = 1;
  for (sync_i; sync_i < 599; sync_i++) {
    if (sync == 3 || sync == 4) {
      if ((int)(62.5 * capture_data_mfm[sync_i]) < 2000) sync++;
    } else {
      if ((int)(62.5 * capture_data_mfm[sync_i]) < 2000) sync = 0;
      if ((int)(62.5 * capture_data_mfm[sync_i]) >= 2000 && (int)(62.5 * capture_data_mfm[sync_i]) < 3000)
        sync++;
      if ((int)(62.5 * capture_data_mfm[sync_i]) >= 3000) sync = 0;
    }
    if (sync == 6) break;
  }
  Serial.print(F("Sync at "));
  Serial.println(sync_i - sync) + 1;

  //Display captured data in bits
  Serial.println(F("Capture data in bits"));
  boolean even = true;
  for (unsigned int i = (sync_i - sync + 1); i < 599; i++) {
    if (even) {
      if ((int)(62.5 * capture_data_mfm[i]) < 2000) Serial.println("0"); //10
      if ((int)(62.5 * capture_data_mfm[i]) >= 2000 && (int)(62.5 * capture_data_mfm[i]) < 3000) {
        Serial.println("0"); //10, extra 0
        even = false;
      }
      if ((int)(62.5 * capture_data_mfm[i]) >= 3000) Serial.println("00"); //10 00
    } else {
      if ((int)(62.5 * capture_data_mfm[i]) < 2000) Serial.println("1"); //01 0
      if ((int)(62.5 * capture_data_mfm[i]) >= 2000 && (int)(62.5 * capture_data_mfm[i]) < 3000) {
        Serial.println("10"); //01 00
        even = true;
      }
      if ((int)(62.5 * capture_data_mfm[i]) >= 3000) Serial.println("10"); //01 00 0
    }
    delay(5);
  }

  //Display captured data in bytes
  even = false;
  Serial.println(F("Capture data in bytes"));
  Serial.println(62.5 * capture_data_mfm[(sync_i - sync + 1)]);
  Serial.println(62.5 * capture_data_mfm[(sync_i - sync + 2)]);
  Serial.println(62.5 * capture_data_mfm[(sync_i - sync + 3)]);
  Serial.println(62.5 * capture_data_mfm[(sync_i - sync + 4)]);
  Serial.println(62.5 * capture_data_mfm[(sync_i - sync + 5)]);
  Serial.println(62.5 * capture_data_mfm[(sync_i - sync + 6)]);
  delay(500);
  unsigned char le_byte = 0;
  boolean extra = false;
  unsigned int byte_count = 0;
  //Really bad code, but it works
  for (unsigned int i = (sync_i - sync + 1); i < 599; i++) {
    for (unsigned int b = 7; b >= 0; b--) { //Byte
      if ((int)(62.5 * capture_data_mfm[i]) < 2000) { //10
        if (!extra) { //no extra, extra is false
          le_byte = le_byte &= ~(1 << b); //0 to bit position
          if (b == 0) break;
        } else {
          le_byte |=  (1 << b); //1 to bit position
          if (b == 0) break;
        }
      }
      if ((int)(62.5 * capture_data_mfm[i]) >= 2000 && (int)(62.5 * capture_data_mfm[i]) < 3000) { //100
        if (!extra) { //no extra, extra is false
          le_byte &= ~(1 << b); //0 to bit position
          extra = true;
          if (b == 0) break;
        } else {
          le_byte |=  (1 << b); //1 to bit position
          if (b == 0) break;
          b--;
          le_byte &= ~(1 << b); //0 to bit position
          extra = false;
          if (b == 0) break;
        }
      }
      if ((int)(62.5 * capture_data_mfm[i]) >= 3000) { //10 00
        if (!extra) { //no extra, extra is false
          le_byte = le_byte &= ~(1 << b); //0 to bit position
          if (b == 0) break;
          b--;
          le_byte = le_byte &= ~(1 << b); //0 to bit position
          if (b == 0) break;
        } else {
          le_byte |=  (1 << b); //1 to bit position
          if (b == 0) break;
          b--;
          le_byte = le_byte &= ~(1 << b); //0 to bit position
          if (b == 0) break;
        }
      }
      i++;
    }
    byte_count++;
    Serial.print("BYTE ");
    Serial.print(byte_count);
    Serial.print(": ");
    Serial.println(le_byte, HEX);
    //Serial.println(le_byte, BIN);
    delay(500);
  }

  Serial.println(F("delay"));
  delay(1000);

}



