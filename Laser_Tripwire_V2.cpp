#define rxPin 0
#define txPin 1
//LIBRARIES--------------------------------------------------------------


// GLOBAL VARIABLES------------------------------------------------------
int LEDPIN = 11; // Connect Red LED to Digital Pin 11
int LIGHTINPUT = A0; // Pin for LDR input
int sample = 0;
long avg = 0;
char buffer[200] = {0};
short modechanger = 0;
short HC06PWR = 12;  //HC06 power pin is at Digital 12 (outputs 5V to a voltage divider, which will yield 3.3 V for the actual VCC-in for the Bluetooth Module)
unsigned short counter = 0;                            //^ Voltage Divider doesnt work, but 5V direct input seems to be ok for HC06...
unsigned short AutoTimeUpdate = 0;

// PARSING_CODE GLOBALS---------------------------------------------------
short mode = 0;
short i = 0;
short receivedInteger = 0;
short receivedMinute = 0;
short night2day = 0;  //If starttime to endtime needs transition from late PM to early AM, time conditions need to be editted.
short startTime = 2000;  //Default Laser Arming Time
short endTime = 500;  //Default Laser Disarming Time
short currentTime = 0;
short ready2compare = 0;
char commandWord[20] = {0};    //May want to increase the size of the array...
char receivedValue[4] = {0};
char receivedMinuteValue[2] = {0};
char receivedSecondsValue[2] = {0};
char *validCommands[] = {"set-armtime-start","set-armtime-end","time-now","reset","commands","modeChange",'\0'};   //"set-armtime-start" = validCommand[0], "set-armtime-end" = validCommand[1], etc.
char inputChar = '0';  //Byte Reading Buffer


// FUNCTION DECLARATIONS--------------------------------------------------
int armedFunc(int varA, int varB);
short changeStarttime(short timeVal);
short changeEndtime(short timeVal);
short militaryTime(short hour, short minute);
short modeChange(short modeNum);


// INITIAL SETUP-------------------------------------------
void setup()
  {
      //LIGHT SENSOR CALIBRATION
      Serial.begin(9600);  // Set serial communcation speed
      pinMode(LEDPIN, OUTPUT);
      pinMode(HC06PWR, OUTPUT);
      digitalWrite(HC06PWR, HIGH);
      
      long initial_time = millis();
      long projectedtime = initial_time + 2000;
//      Serial.println("Calibrating, Please Wait...");  //Will remove when final version is done
      while(initial_time < projectedtime)
          {            
              initial_time = millis();
              digitalWrite(LEDPIN, LOW);
              avg = (avg+analogRead(LIGHTINPUT))/2;
              delay(100);
              digitalWrite(LEDPIN, HIGH);
          }
//      Serial.print("Average = ");
//      Serial.println(avg, DEC);
//      Serial.println("Valid Commands:");
//      Serial.println(validCommands[0]);
//      Serial.println(validCommands[1]);
//      Serial.println(validCommands[2]);
//      Serial.println(validCommands[3]);
//      Serial.println(validCommands[4]);
//      Serial.print(validCommands[5]);
//      Serial.println(" [1 = Demo; 2 = Armed; 3 = Disarmed; 4 = Auto]");
//      Serial.println("-----------------------");  delay(5000);    //For reference only, will comment out in final version.
//      Serial.println("Enter commands ending with a comma...\n");
//      Serial.println("Code will begin in:");
//      Serial.print("5");
//      delay(1000);
//      Serial.print("4");
//      delay(1000);
//      Serial.print("3");
//      delay(1000);
//      Serial.print("2");
//      delay(1000);
//      Serial.print("1");
//      delay(1000);
//      Serial.println("0");
//      delay(1000);
//      Serial.println("Now Starting...");
      Serial.println("returnTime");  //DO NOT COMMENT THIS LINE!!!
  }
  

// STATE SWITCHING SETUP----------------------------------------------------------
typedef enum {demo, armed, disarmed, automatic} Statetype;
Statetype currentState = automatic;/*A new variable type named "Statetype"(like
                                   integers/longs/floats/etc., with four diff
                                   states*/
                              

// CORE PROGRAM LOOP---------------------------------------------------------------
void loop()
{

   int trigger = armedFunc(sample, (int) avg); //Used in armed and demo functions as boolean for security breach
   sample = analogRead(LIGHTINPUT);

//   Serial.print(" Arm:");
//   Serial.print(startTime);
//   Serial.print(" Disarm:");
//   Serial.print(endTime);
//   Serial.print(" TimeNow:");
//   Serial.print(currentTime);
   
   counter++;
   
   if(endTime < startTime)
   {
       night2day = 1;
   }
   if(endTime > startTime)
   {
       night2day = 0;
   }
   if(counter >= 1500)              //Every 2~ish minutes, returnTime command is sent
   {
       Serial.println("returnTime"); //***The '/n' is here so I can see things better on console. Remove in the final version???***
       counter = 0;
   }
   if(startTime == 0)          //Used to fix a logical error (i.e. if time range is 0 to 5am, auto will stay on forever, 
   {                           //because 0==12AM. if time is 2359 (11:59pm), val is still > 0, therefore it'll stay on.
       startTime = 2400;
   }


//PARSER CODE (SERIAL)-----------------------------------------------------------------------------------------
    if(Serial.available() > 0)
    {
      inputChar = (char)Serial.read(); //Reads one byte, interperets it as an ASCII Char, and puts in inputChar
           if(mode == 0)
           {
               if(inputChar == ':')              //Checks for the ':', which indicates the end of the commandWord
               {
                   commandWord[i] = '\0';
                   i = 0;
                   mode = 1;
                   inputChar = '0';             
               }
               else if(inputChar == ',')
               {
                   commandWord[i] = '\0';
                   i = 0;
                   ready2compare = 1;  //will initialize the comparison of commands to validCommands dictionary
                   inputChar = '0';
               }
               else
               {
                   commandWord[i] = inputChar;  //If the inputChar isn't the ':', the read byte (aka ASCII character) is added to the string at position 'i'
                   inputChar = '0';          //inputChar is cleared to make way for next reading
                   i++;
               }
           }
                          
                                  //***After reading the commandWord and the ':', what is expected next is the value with which to change the times (saved as a string):
           else if(mode == 1)
           {
                   if(inputChar == ',')  // The comma signifies end of received value. If none is present, code will assume the minute val is next.
                   {
                       receivedValue[i] = '\0';  //buffer string where value is saved
                       receivedInteger = atoi(receivedValue);  //Since receivedValue is a char, we convert it to an int using atoi fucntion (C++).
                       inputChar = '0';
                       i = 0;
                       mode = 0;
                       ready2compare = 1;
                   }
                   else if(inputChar == ':')  // The colon signifies that the minute value is next.
                   {
                       receivedValue[i] = '\0';  /*This section of code is specifically for when we are receiving the current time requested by the returnTime command.
                                                   The received current time is expected to be in HH:MM:SS format. When 'HH:' is received, the two digits for hours will
                                                   be stored in receivedValue, then once the ':' is encountered, the loop will  break, and go to the next loop, which is
                                                   intended to parse the values for minutes and seconds.*/
                       receivedInteger = atoi(receivedValue);  //Since receivedValue is a char, we convert it to an int using atoi fucntion (C++).
                       inputChar = '0';
                       i = 0;
                       mode = 2;
                   }
                   else
                   {
                       receivedValue[i] = inputChar;
                       inputChar = '0';
                       i++;
                   }
           }
           else if(mode == 2)                //This loop will convert the minutes and seconds to the format used in this code (MM:SS ---> militaryTime)...Don't really need seconds...?
           {               
                   if(inputChar == ',')  // Comma signals the end of command
                   {
                       receivedMinuteValue[i] = '\0';
                       receivedMinute = atoi(receivedMinuteValue);    //Converts Minute(received as chars) into Minute(as short int)
                       i = 0;
                       mode = 0;
                       ready2compare = 1;
                   }
                   else if(inputChar == ':')
                   {
                       receivedMinuteValue[i] = '\0';
                       receivedMinute = atoi(receivedMinuteValue);
                       i = 0;
                       mode = 3;
                   }
                   else
                   {
                       receivedMinuteValue[i] = inputChar;
                       inputChar = '0';
                       i++;
                   }
           }
           else if(mode == 3)    //If a seconds value is sent, i will not use it, but to avoid code getting confused, i will store it somewhere to delete.
           {
                   if(inputChar == ',')
                   {
                       receivedSecondsValue[0] = '\0';      //Clears seconds value...
                       i = 0;
                       mode = 0;
                       ready2compare = 1;
                   }
                   else
                   {
                       receivedSecondsValue[i] = inputChar;
                       inputChar = '0';
                       i++;
                   }
           }
    }
       
//COMMAND COMPARATOR------------------------------------------------------
    if(ready2compare == 1)
    {
         if(*commandWord == *validCommands[5])    //If the received command word == modeChange...
         {
             modechanger = modeChange(receivedInteger);
             if(modechanger == 0)
             {
                 currentState = demo;
             }
             else if(modechanger == 1)
             {
                 currentState = armed;
             }
             else if(modechanger == 2)
             {
                 currentState = disarmed;
             }
             else
             {
                 currentState = automatic;
             }
         }
         else if(*commandWord == *validCommands[4])    //If the received command word == print-commands...
         {
             Serial.println("-");
             Serial.println("-----------------------");
             Serial.println("Valid Commands:");
             Serial.println(validCommands[0]);
             Serial.println(validCommands[1]);
             Serial.println(validCommands[2]);
             Serial.println(validCommands[3]);
             Serial.println(validCommands[4]);
             Serial.println(validCommands[5]);
             Serial.println("1 = Demo; 2 = Armed; 3 = Disarmed; 4 = Auto");
             Serial.println("**End each command with a comma.**");
             Serial.println("----------------------- ");
             Serial.print("Resuming in 5");
             delay(1000);
             Serial.print("4");
             delay(1000);
             Serial.print("3");
             delay(1000);
             Serial.print("2");
             delay(1000);
             Serial.print("1");
             delay(1000);
             Serial.print("0");
             delay(1000);             
         }
         else if(*commandWord == *validCommands[3])  //If the received command word == reset... 
         {
           receivedValue[0] = '\0';
           receivedInteger = 0;
           receivedMinuteValue[0] = '\0';
           receivedMinute = 0;      
           commandWord[0] = '\0';
//           Serial.println("\n -------Command Line reset-------");
         }
         else if(*commandWord == *validCommands[2])  //If the recieved command word == time-now... 
         {
             currentTime = militaryTime(receivedInteger, receivedMinute);     //<---  function to convert HH:MM:SS format to Military time format
             if(currentTime > 2400 || currentTime < 0)
             {
//               Serial.println("Error, Invalid Time Value!");    //Remove when finalizing project...
               Serial.println("returnTime");            //DO NOT REMOVE THIS LINE, THOUGH!!!!
             }
             else if(currentTime == 2400)
             {
               currentTime = 0;
             }
             else
             {
               commandWord[0] = '\0';
               receivedInteger = 0;
               receivedMinuteValue[0] = '\0';
               receivedMinute = 0;
               receivedValue[0] = '\0';
             }
         }
         else if(commandWord[12] == 'e' && commandWord[4] == 'a')   //If the received command word == set-armtime-end... 
         {
             endTime = changeEndtime(receivedInteger);       //<--- run a function to change the end of autoarming time
             commandWord[0] = '\0';                           //Clears commandWord buffer to make way for the next incoming command...
             receivedValue[0] = '\0';
             receivedInteger = 0;
             receivedMinuteValue[0] = '\0';
             receivedMinute = 0;      
         }
         else if(commandWord[12] == 's' && commandWord[4] == 'a')  //If the received command word == set-armtime-start... compares 's' only (really puzzled)
         {
             startTime = changeStarttime(receivedInteger);   //<--- run a function to change the start of autoarming time
             commandWord[0] = '\0';                           //Clears commandWord buffer to make way for the next incoming command...
             receivedValue[0] = '\0';
             receivedInteger = 0;
             receivedMinuteValue[0] = '\0';
             receivedMinute = 0;
         }
         else
         {
             Serial.println("Error, Invalid Command!");
         }
         ready2compare = 0;
    }


//STATE-SWTICHING CODE ------------------------------------------------------------      
       switch(currentState)
       {
               case demo:
                     if(trigger == 1)
                     {
                         Serial.println("emailMsg:'Hello, Testing (Demo)!'");
                         delay(3000);
                         currentState = disarmed;
                     }
                     else
                     {
//                         Serial.print(" [Demo]");
                     }
               break;
                 
               case armed:
                     if(trigger == 1)
                     {
                         Serial.println("emailMsg:'ArmedMode Breach!'");
                         delay(3000);
                         trigger = 0;
                     }
                     else
                     {
//                         Serial.print(" [Armed]");
                     }
               break;
                 
               case disarmed:
                     if(night2day == 0)
                     {
                         if(currentTime >= startTime && currentTime < endTime)
                         {
                             currentState = automatic;
                         }
                         else
                         {
//                             Serial.print(" [Disarmed]");
                         }
                     }
                     else if(night2day == 1)
                     {
                         if(currentTime >= startTime || currentTime < endTime)
                         {
                             currentState = automatic;
                         }
                         else
                         {
//                             Serial.print(" [Disarmed]");
                         }
                     }
               break;
                 
               case automatic:
                     AutoTimeUpdate++;
                     if(night2day == 0)
                     {
                         if(currentTime >= startTime && currentTime < endTime) //currentTime is now within the arm period
                         {
                             if(trigger == 1)
                             {
                                 Serial.println("emailMsg:'AutoMode Breach!'");
                                 delay(3000);
                                 trigger = 0;
                             }
                         }
                         else
                         {
                             currentState = disarmed;
                         }
                     }
                     else if(night2day == 1)
                     {
                         if(currentTime >= startTime || currentTime < endTime)
                         {
                             if(trigger == 1)
                             {
                                 Serial.println("emailMsg:'AutoMode Breach!'");
                                 delay(3000);
                                 trigger = 0;
                             }
                         }
                         else
                         {
                             currentState = disarmed;
                         }
                     }
//                     Serial.print(" [Auto]");
                     if(AutoTimeUpdate >= )
                     {
                         Serial.println("returnAutoarm_Time");
                         AutoTimeUpdate = 0;
                     }
               break;
               
               default:
                     Serial.print("State Machine Error\n");
      }
//      Serial.print(" Prev:");    //Erase this on the Final version!
//      Serial.print(commandWord);
//      Serial.print(" Cnt:");
//      Serial.print(counter);
//      Serial.print(" Sensors: ");
//      Serial.print(sample);
//      Serial.print("_");
//      Serial.print((int)avg);
//      Serial.print("_");
//      Serial.println(night2day);
}

  
// FUNCTION DEFINITIONS--------------------------------------------------------
int armedFunc(int varA, int varB)
    {
        if(varA < varB-40) 
        {
            digitalWrite(LEDPIN, HIGH);                                       
            return 1;
        } 
        else 
        {
            digitalWrite(LEDPIN, LOW);
            return 0;
        }            
    }
short changeStarttime(short timeVal)
    {
        short newTime = timeVal;
        return newTime;
    }
short changeEndtime(short timeVal)
    {
        short newTime = timeVal;
        return newTime;
    }
short militaryTime(short hour, short minute)
    {
        short hourVal = hour;
        short minVal = minute;
        short newCurrentTime = (100*hourVal) + minVal;
        return newCurrentTime;
    }
short modeChange(short modeNum)
    {
        short mode = modeNum - 1;
        return mode;
    }
