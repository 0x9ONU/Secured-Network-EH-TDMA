#include <Arduino.h>

struct flags {
  String ID = "03";
  long offset = 0;                        // Offset from the node's cycle to the global cycle
  long global_time = 0;                   // the time the previous node sent the message
  unsigned long time_in = 0;            // local arrival time, then converted to global arrival time, ideally the same as time_sent
  unsigned long time_in_U= 0;     // time in, but relative to the global time
  bool is_sync = 0;                      // Checks if a sync packet has been sent
  bool is_sent = 0;                      // Flag if a packet has been sent during it's transmission time
  String data = "Node 03: Network 2"; // Random data sent by the node
};

//Setup Ennumerations to Closely Match the FSM and Pseudocode
enum states {SYNC, ACTIVE, DEAD}; 

//Setup data structures and global variables
states state;
flags myFlags;

//Constants and Assumptions
const String NODES[] = {"BB"};                                      //Hearable Nodes in the network. Single hop, so assume it can only hear from the base station
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES+1) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 70;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 40;                               // energy harvest rate
const unsigned long TRANSMIT_TIME = (myFlags.ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2); // time in the cycle to transmit TRANSMIT_TIME

//Function Definitions
void baseFSM();
void send(String output);
bool receive();
unsigned long cycleTime();
bool isHearable(const String& sender);
int idToNode(const String& ID);
bool energyAvailible();

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
        // Send ACK
        send(myFlags.ID + ":" + (String)cycleTime() + ":0" + ":1");
        myFlags.offset = (myFlags.global_time - (long) myFlags.time_in); // Add 50ms to account of processing timing
        if(myFlags.offset < 0) {
          myFlags.offset = (long) CYCLE_LENGTH + myFlags.offset;
        }
        myFlags.time_in_U = (myFlags.time_in + myFlags.offset) % CYCLE_LENGTH;
        state = ACTIVE;
      }
      break;
    }
    case ACTIVE: {
      // See if energy is available
      bool isenergyAvailible = energyAvailible();
      //Send their packet with it's values
      // (ID:TIME:SYNC:ACK:Data)
      while(isenergyAvailible && state == ACTIVE) {
         long currentCycle = cycleTime();
        //Serial.println("IM ACTIVE");
        if((currentCycle >= TRANSMIT_TIME && TRANSMIT_TIME + TIME_SLOT - ERROR >= currentCycle) && !myFlags.is_sent){ 
          myFlags.is_sent = true;
          send(myFlags.ID + ":" + (String)cycleTime() + ":0" + ":0" + ":" + myFlags.data);
          isenergyAvailible = energyAvailible();
        }
        else if(receive()) {
          myFlags.time_in_U = cycleTime();
          if (myFlags.is_sync) {
            state = SYNC;
          }
        }
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
    String sender = Serial.readStringUntil(':');                     // Grab ID
    // TEST REPLACE LATER
      if(isHearable(sender)) {
      myFlags.time_in = millis() % CYCLE_LENGTH;                    // Grab the correct values for the current time in terms of the Cycle Length
      myFlags.global_time = Serial.parseInt();                       // Turn the data from String to integer
      Serial.readStringUntil(':');
      myFlags.is_sync = Serial.parseInt();                           // Sync Flag
      Serial.readStringUntil(':');                                   // ACK Flag
      Serial.readStringUntil('\n');                                 // Data
      return true;
    }
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

bool isHearable(const String& sender) {
  for(String i:NODES){
    if(i == sender){
      return true;
    }
  }
  return false;
}