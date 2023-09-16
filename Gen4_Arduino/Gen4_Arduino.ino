#include <TimeLib.h>
#include <genieArduino.h>

// This Demo communicates with a 4D Systems Display, configured with ViSi-Genie, utilising the Genie Arduino Library - https://github.com/4dsystems/ViSi-Genie-Arduino-Library.
// The display has a slider, a cool gauge, an LED Digits, a string box and a User LED. Workshop4 Demo Project is located in the /extras folder
// The program receives messages from the Slider0 object using the Reported Events. This is triggered each time the Slider changes on the display, and an event
// is genereated and sent automatically. Reported Events originate from the On-Changed event from the slider itself, set in the Workshop4 software.
// Coolgauge is written to using Write Object, and the String is updated using the Write String command, showing the version of the library.
// The User LED is updated by the Arduino, by first doing a manual read of the User LED and then toggling it based on the state received back.

// As the slider changes, it sends its value to the Arduino (Arduino also polls its value using genie.ReadObject, as above), and the Arduino then
// tells the LED Digit to update its value using genie.WriteObject. So the Slider message goes via the Arduino to the LED Digit.
// Coolgauge is updated via simple timer in the Arduino code, and updates the display with its value.
// The User LED is read using genie.ReadObject, and then updated using genie.WriteObject. It is manually read, it does not use an Event.

// This demo illustrates how to use genie.ReadObject, genie.WriteObject, Reported Messages (Events), genie.WriteStr, genie.WriteContrast, plus supporting functions.

// Application Notes on the 4D Systems Website that are useful to understand this library are found: https://docs.4dsystems.com.au/app-notes
// Good App Notes to read are: 
// ViSi-Genie Connecting a 4D Display to an Arduino Host - https://docs.4dsystems.com.au/app-note/4D-AN-00017/
// ViSi-Genie Writing to Genie Objects Using an Arduino Host - https://docs.4dsystems.com.au/app-note/4D-AN-00018/
// ViSi-Genie A Simple Digital Voltmeter Application using an Arduino Host - https://docs.4dsystems.com.au/app-note/4D-AN-00019/
// ViSi-Genie Connection to an Arduino Host with RGB LED Control - https://docs.4dsystems.com.au/app-note/4D-AN-00010/
// ViSi-Genie Displaying Temperature values from an Arduino Host - https://docs.4dsystems.com.au/app-note/4D-AN-00015/
// ViSi-Genie Arduino Danger Shield - https://docs.4dsystems.com.au/app-note/4D-AN-00025


#define Form_LockerScreen   0
#define Form_PinEntry       1
#define Form_Rec_off        2
#define Form_Rec_on         3

#define  LockbuttonLockScreen  0
#define  Pinbutton1       1
#define  Pinbutton2       2
#define  Pinbutton3       4
#define  Pinbutton6       5
#define  Recordbutton_Form_record_off    6
#define  Recordbutton_Form_record_on     8
#define  Stopbutton_form_record_on       9

#define RESETLINE 4  // Change this if you are not using an Arduino Adaptor Shield Version 2 (see code below)
#define LED_PIN 9

Genie genie;
bool g_isRecording = false;
unsigned long g_recordingStartTime = 0;
int g_securityPinNumber = 0;

struct GaugeStruct 
{
    uint16_t value;
    uint16_t incrementAmount;
    uint16_t stopValue;
    bool isIncrementing;
};

#define NUM_GAUGES 6
GaugeStruct g_gauges[NUM_GAUGES] = {
  {0,10,99,true},
  {0,10,99,true},
  {0,10,99,true},
  {0,10,99,true},
  {0,10,99,true},
  {0,10,99,true}
};

uint16_t setNewGaugeValue(int index)
{
  GaugeStruct &gauge = g_gauges[index];
  if( gauge.isIncrementing )
  {
    if( gauge.value + gauge.incrementAmount >= gauge.stopValue)
    {
      gauge.isIncrementing = false;
      gauge.incrementAmount = random(5,10);
      gauge.stopValue = random(1,gauge.value);
    }
    else
    {
      gauge.value += gauge.incrementAmount;
    }
  }
  else //decrementing
  {
    if( gauge.value < gauge.incrementAmount)
    {
      gauge.isIncrementing = true;
      gauge.incrementAmount = random(10,15);
      gauge.stopValue = random(gauge.value, 99);
    }
    else
    {
      gauge.value -= gauge.incrementAmount;
    }
  }
  return gauge.value;
}


void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, 1);

  // Use a Serial Begin and serial port of your choice in your code and use the 
  // genie.Begin function to send it to the Genie library (see this example below)
  // 200K Baud is good for most Arduinos. Galileo should use 115200.  
  // Some Arduino variants use Serial1 for the TX/RX pins, as Serial0 is for USB.
  Serial.begin(115200);  // Serial0 @ 200000 (200K) Baud
  genie.Begin(Serial);   // Use Serial0 for talking to the Genie Library, and to the 4D Systems display

  genie.AttachEventHandler(myGenieEventHandler); // Attach the user function Event Handler for processing events

  // Reset the Display (change D4 to D2 if you have original 4D Arduino Adaptor)
  // THIS IS IMPORTANT AND CAN PREVENT OUT OF SYNC ISSUES, SLOW SPEED RESPONSE ETC
  // If NOT using a 4D Arduino Adaptor, digitalWrites must be reversed as Display Reset is Active Low, and
  // the 4D Arduino Adaptors invert this signal so must be Active High.  
  pinMode(RESETLINE, OUTPUT);  // Set D4 on Arduino to Output (4D Arduino Adaptor V2 - Display Reset)
  digitalWrite(RESETLINE, 0);  // Reset the Display via D4
  delay(100);
  digitalWrite(RESETLINE, 1);  // unReset the Display via D4


  // Let the display start up after the reset (This is important)
  // Increase to 4500 or 5000 if you have sync problems as your project gets larger. Can depent on microSD init speed.
  delay (3500); 

  // Set the brightness/Contrast of the Display - (Not needed but illustrates how)
  // Most Displays use 0-15 for Brightness Control, where 0 = Display OFF, though to 15 = Max Brightness ON.
  // Some displays are more basic, 1 (or higher) = Display ON, 0 = Display OFF.  
  genie.WriteContrast(10); // About 2/3 Max Brightness

  //Write to String0 on the Display to show the version of the library used
  //genie.WriteStr(0, GENIE_VERSION);
  //OR to illustrate (comment out the above, uncomment the below)
  //genie.WriteStr(0, (String) "Hello 4D World");
    digitalWrite(LED_PIN, 0);
}

void loop()
{
  // Write to Arduino LED heartbeat on each 10 iterations
  static int ledCount = 0;
  if(ledCount < 2 )
      digitalWrite(LED_PIN, 1);
  else if(ledCount <10 )
      digitalWrite(LED_PIN, 0);
  ledCount = ++ledCount % 100;


  genie.DoEvents(); // This calls the library each loop to process the queued responses from the display
  
  if( g_isRecording )
  {
      //update recording time
      unsigned long recordingTime = (millis() - g_recordingStartTime)/1000;
      int seconds = recordingTime % 60;
      int minutes = (recordingTime / 60 ) % 60;
      int hours =  (recordingTime /60/60) % 24;
      
      genie.WriteObject(GENIE_OBJ_CUSTOM_DIGITS, 0, hours);
      genie.WriteObject(GENIE_OBJ_CUSTOM_DIGITS, 1, minutes);
      genie.WriteObject(GENIE_OBJ_CUSTOM_DIGITS, 2, seconds);
      // Simulation code, just to increment and decrement gauge value each loop, for animation
      for(uint16_t index = 0; index < NUM_GAUGES;index++)
      {
        genie.WriteObject(GENIE_OBJ_GAUGE, index, setNewGaugeValue(index));
      }
  }
  else
  {
      //set time back to 0
      genie.WriteObject(GENIE_OBJ_CUSTOM_DIGITS, 0, 0);
      genie.WriteObject(GENIE_OBJ_CUSTOM_DIGITS, 1, 0);
      genie.WriteObject(GENIE_OBJ_CUSTOM_DIGITS, 2, 0);
  }
}

/////////////////////////////////////////////////////////////////////
//
// This is the user's event handler. It is called by genieDoEvents()
// when the following conditions are true
//
//		The link is in an IDLE state, and
//		There is an event to handle
//
// The event can be either a REPORT_EVENT frame sent asynchronously
// from the display or a REPORT_OBJ frame sent by the display in
// response to a READ_OBJ (genie.ReadObject) request.
//
// LONG HAND VERSION, MAYBE MORE VISIBLE AND MORE LIKE VERSION 1 OF THE LIBRARY
void myGenieEventHandler(void)
{
  genieFrame Event;
  genie.DequeueEvent(&Event); // Remove the next queued event from the buffer, and process it below


  //If the cmd received is from a Reported Event (Events triggered from the Events tab of Workshop4 objects)
  if (Event.reportObject.cmd == GENIE_REPORT_EVENT)
  {
    if( Event.reportObject.object == GENIE_OBJ_USERBUTTON)
    {
      //start recording
      if (Event.reportObject.index == Recordbutton_Form_record_off && !g_isRecording )
      {
          g_isRecording = true;
          genie.WriteObject(GENIE_OBJ_FORM, Form_Rec_on, 0);    //Show the recording on form 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Recordbutton_Form_record_on, 1);    //Set the button state to "down" 
          g_recordingStartTime = millis();
      }
      
      //stop recording
      if (Event.reportObject.index == Stopbutton_form_record_on && g_isRecording )
      {
          g_isRecording = false;
          genie.WriteObject(GENIE_OBJ_FORM, Form_Rec_off, 0);    //Show the recording off form 
      }
      
      //Pin number entry
      if(Event.reportObject.index >= Pinbutton1 && Event.reportObject.index <= Pinbutton6)
      {
        if( Event.reportObject.data_lsb == 0)
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Event.reportObject.index, 1);    //Set the button state to "down" 
        else
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Event.reportObject.index, 0);    //Set the button state to "up" 

        int digit = 0;
        switch (Event.reportObject.index)
        {
          case Pinbutton1: digit = 1; break;
          case Pinbutton2: digit = 2; break;
          case Pinbutton3: digit = 3; break;
          case Pinbutton6: digit = 6; break;
        }
        g_securityPinNumber = g_securityPinNumber* 10 + digit;
        if( g_securityPinNumber == 1236)
        {
          //correct pin. Show the recording off form
          genie.WriteObject(GENIE_OBJ_FORM, Form_Rec_off, 0);    //Show the recording off form 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton1, 0);    //Set the button state to "down" 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton2, 0);    //Set the button state to "down" 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton3, 0);    //Set the button state to "down" 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton6, 0);    //Set the button state to "down" 
        }
        else if(g_securityPinNumber > 1236)
        {
          //reset the pin
          g_securityPinNumber = 0;
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton1, 0);    //Set the button state to "down" 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton2, 0);    //Set the button state to "down" 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton3, 0);    //Set the button state to "down" 
          genie.WriteObject(GENIE_OBJ_USERBUTTON, Pinbutton6, 0);    //Set the button state to "down" 
        }
      }
    }  
  }
}
