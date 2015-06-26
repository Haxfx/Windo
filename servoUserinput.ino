#include <Servo.h> 

Servo myservo;  // create servo object to control a servo 
                // a maximum of eight servo objects can be created 

int pos = 0;    // variable to store the servo position 
int incomingByte = 0;
void setup()
{ 
  Serial.begin(9600);
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object 
} 


void loop() 
{ 
   if (Serial.available()) {
   incomingByte = Serial.parseInt();   
//   int movement = incomingByte - val;
  myservo.attach(9);
  
  if(pos < incomingByte){
    for(pos; pos < incomingByte; pos += 1)  // goes from 0 degrees to 180 degrees 
    {                                  // in steps of 1 degree 
      myservo.write(pos);              // tell servo to go to position in variable 'pos' 
      Serial.println(pos);
      delay(15);                       // waits 15ms for the servo to reach the position 
      
    }
  } 
  if(pos > incomingByte){
    for(pos; pos>incomingByte; pos-=1)     // goes from 180 degrees to 0 degrees 
    {                                
      myservo.write(pos);              // tell servo to go to position in variable 'pos' 
      delay(15);                       // waits 15ms for the servo to reach the position 
    }
  }
    if(pos == incomingByte)
    myservo.detach();
  
  }else{
    myservo.detach();
  } 
} 
