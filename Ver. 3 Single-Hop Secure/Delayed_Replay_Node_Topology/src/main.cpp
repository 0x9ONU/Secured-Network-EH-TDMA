#include <Arduino.h>

struct flags {
  String ID = "XX";
  long offset = 0;                        // Offset from the node's cycle to the global cycle
  long global_time = 0;                   // the time the previous node sent the message
  unsigned long time_in = 0;            // local arrival time, then converted to global arrival time, ideally the same as time_sent
  unsigned long time_in_U= 0;     // time in, but relative to the global time
  bool is_sync = 0;                      // Checks if a sync packet is in the buffer
  bool is_ack = 0;                      // Checks if a ack packet is in the buffer
  bool is_data = 0;                     // Checks if there is data in the buffer
};

//Setup Ennumerations to Closely Match the FSM and Pseudocode
enum states {CAPTURE, ACTIVE, DEAD}; 

//Constants and Assumptions
//const String NODES[] = {"BB"};                              
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES+1) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 70;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 101;                               // energy harvest rate

//Setup data structures and global variables
states state;
flags myFlags;
String sync_packet = "";
String temp_data;

//Set transmission time
const unsigned long TRANSMIT_TIME = (myFlags.ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2); // time in the cycle to transmit TRANSMIT_TIME


//Function Definitions
void baseFSM();
void send(String output);
bool receive();
unsigned long cycleTime();
bool energyAvailible();
int randomTime(int min, int max);
int randomTime(int min, int max, int seed);

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
        myFlags.is_ack = false;
        myFlags.is_data = false;
        // Reset Timers
        myFlags.offset = 0;
        myFlags.global_time = 0;
        myFlags.time_in = 0;
        myFlags.time_in_U = 0;
        state = CAPTURE;
      }
    }
    case CAPTURE: {
      receive();
      if(myFlags.is_sync) {
        sync_packet = temp_data;
        //Delays a random amount of time and uses the 0 pin to make it closer to a TRNG
        delay(randomTime(10000,100000, analogRead(0))); 
        state = ACTIVE;
      }
      // Reset Flags
      myFlags.is_sync = 0; myFlags.is_ack = 0; myFlags.is_data;
      break;
    }

    case ACTIVE: {
      // See if energy is available
      bool isenergyAvailible = energyAvailible();

      //If there is energy, replay all the packets gained all at once
      if(isenergyAvailible) {
        send(sync_packet);
        sync_packet = ""; //Clear the buffered sync packet
        state = CAPTURE; //Start capturing more packets
      }
      else {
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
 * @param String output:  The String that will be sent over the wireless module into the air
 */
void send(String output) {
  Serial.println(output);
  Serial.flush();
}

/** receive
 * Grabs a string out of the same wireless channel and reads it in if it exists
 * Saves the packet to a temp String to be saved into a queue later
 * 
 * @return  True if a packet was found
 */
bool receive() {
  //Makes sure to only grab a packet if it exists
  if(Serial.available() > 0) {
    String sender = Serial.readStringUntil(':');                     // Grab ID
    String timing = Serial.readStringUntil(':');                   //Ignore timing flag
    myFlags.is_sync = Serial.readStringUntil(':').toInt();                           // Sync Flag
    myFlags.is_ack = Serial.readStringUntil(':').toInt();                           // ACK Flag
    String data = Serial.readStringUntil('\n');                                 // Data
    if (data.length() != 0) {
      myFlags.is_data = true;
      temp_data = (sender + ":" + timing + ":" + (String) myFlags.is_sync + ":" + (String) myFlags.is_ack + ":" + data);
    }
    else temp_data = (sender + ":" + timing + ":" +  (String) myFlags.is_sync + ":" + (String) myFlags.is_ack);
    return true;
  }
  return false;
}

/** cycleTime()
 * Helper function to keep the time in the range of one cycle and incorporate the offset
 * Also resets the is_sent variable so we can send a new message if we get to a new cycle
 * 
 * @returns an unsigned long value that is the calcualted global time
 */
unsigned long cycleTime() {
  static unsigned long last_time;
  unsigned long time = ((long)(millis() % CYCLE_LENGTH) + myFlags.offset) % (long)CYCLE_LENGTH;
  /*
  if(last_time > time){ // checks if the clock reset and resets is_sent and sets all nodes dead
    myFlags.is_sent = false;
  }
    */
  last_time = time; // for next time we call the function
  return time;
}


/** bool energyAvailible()
 * is the RNG for if a node has energy or not, based on the chances of that node having energy
 * 
 * @returns true if there is energy available in the node, and false otherwise
*/
bool energyAvailible(){
  if(random(0,100) <= ENERGY_CHANCE){
    return true;
  } 
  else return false;
}

/**
 * Generates a random time to wait given a minimum and a maximum. The seed is set to the defualt seed
 * 
 * @param min  The minimum amount of time to wait
 * @param max  The maximum amount of time to wait
 * 
 * @returns a random number that represents a time in miliseconds (ms)
 */
int randomTime(int min, int max) {
  return random(min, max);
}

/**
 * Generates a random time to wait given a seed, a minimum, and a maximum
 * 
 * @param min  The minimum amount of time to wait
 * @param max  The maximum amount of time to wait
 * @param seed The seed for the PRNG
 * 
 * @returns a random number that represents a time in miliseconds (ms)
 */
int randomTime(int min, int max, int seed) {
  randomSeed(seed);
  return random(min, max);
}