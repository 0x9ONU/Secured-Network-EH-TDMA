#include <Arduino.h>

//Constants and Assumptions
const String NODES[] = {"01", "02", "03"};                      //Nodes in the network
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES+1) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 70;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 101;                               // energy harvest rate
const PROGMEM int TRANSMIT_TIME = TIME_SLOT * TOTAL_NODES + (TIME_SLOT / 2); 

//Setup structures to hold flags and othe rdata
struct flags {
  String ID = "BB";
  String data[TOTAL_NODES];
  bool is_sent = 0;                     //Was a packet sent during the cycle?
  bool all_nodes_dead = 0;              // true when all the nodes are dead (is_sent was never set to true during the cycle length)
  long time_sent = 0;                   // the time the previous node sent the message
  unsigned long time_in = 0;            // local arrival time, then converted to global arrival time, ideally the same as time_sent
  unsigned long last_packet_in = 0;     // used for checking if we are not getting messages. if no messages in 3 cycles, reset the network
  bool is_ack = 0;                      // Checks if acknowledgements have been received
  bool sent_sync = 0;                   // Checks if a sync was sent recently
};
//Setup Ennumerations to Closely Match the FSM and Pseudocode
enum states {SYNC, ACTIVE}; 

//Function Definitions
void baseFSM();
void send(String output);
bool receive();
unsigned long cycleTime();
bool isHearable(const String& sender);
int idToNode(const String& ID);


//Setup data structures and global variables
states state;
flags myFlags;
/** loop()
 * @attention Arduino needs this!
 * Put your setup code here, to run one time. Like a main function that is ran a single time BEFORE loop().
 * */ 
void setup() {
  //Setup the serial to begin running over the air and makes sure that the timeout is set properly
  Serial.begin(9600);
  Serial.setTimeout(30);

  state = SYNC;
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
    case SYNC: {
      // Test
      myFlags.all_nodes_dead = false;
      // Broadcast a SYNC packet
      send(myFlags.ID + ":" + (String)cycleTime() +":1" + ":0");
      myFlags.sent_sync = true;
      //Set state to active after broadcasting a sync packet
      // Continuously receive packets until an acknowledgement is found
        while(!myFlags.is_ack) {
          receive();
          cycleTime();
          if(myFlags.all_nodes_dead) {
            break;
          }
        }
        myFlags.is_ack = false;
      state = ACTIVE;
      break;
    }
    case ACTIVE: {
      //Grab any packets on the network and parse their values
      receive();
      //Out-of-energy errors
      cycleTime(); // Call cycle time to check for out-of-energy errors

      //ERRORS
      if((myFlags.time_in > myFlags.time_sent + (TIME_SLOT/2) - ERROR || myFlags.time_in < myFlags.time_sent - (TIME_SLOT/2) || myFlags.all_nodes_dead) && myFlags.sent_sync == false) {
        state = SYNC;
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
      if(isHearable(sender)) {
      //Debug
      //Serial.println("I GOT THE DATA");
      myFlags.all_nodes_dead = false;                                // All nodes are not dead if you get here lol
      myFlags.is_sent = true;                                        // Sets the is_sent to true as a packet was received
      myFlags.last_packet_in = millis();                             // Get current time
      myFlags.time_in = cycleTime();                                 // Caculate the cycle time and sets the is_sent flag to false if necessary
      //Serial.readStringUntil(':');                                   // Grab the time the node sent the data
      myFlags.time_sent = Serial.parseInt();                         // Turn the data from String to integer
      Serial.readStringUntil(':');                                   // Sync Flag
      Serial.readStringUntil(':');                                   // ACK Flag
      myFlags.is_ack = Serial.parseInt();                            // Reads in String as a Integer and saves if an acknowledgement has been received
      myFlags.data[idToNode(sender)] = Serial.readStringUntil('\n').substring(1); // Data
      //Debug
      //Serial.println(myFlags.data[idToNode(sender)]);
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
  unsigned long time = millis() % CYCLE_LENGTH;
  if(last_time > time){ // checks if the clock reset and resets is_sent and sets all nodes dead
    if (!myFlags.is_sent) {
      myFlags.all_nodes_dead = true;
    }
    myFlags.is_sent = false;
    myFlags.sent_sync = false;
  }
  last_time = time; // for next time we call the function
  return time;
}

/** isHearable
 * Determines if the base node is in charage of the nodes sending the data
 */
bool isHearable(const String& sender) {
  for(String i:NODES){
    if(i == sender){
      return true;
    }
  }
  return false;
}

/**
 * Helper function:
 * Maps a node's ID to which node 
 */
int idToNode(const String& ID) {
  int j = 0;
  for(String i:NODES){
    if(i == ID){
      return j;
    }
    j++;
  }
  return false;
}