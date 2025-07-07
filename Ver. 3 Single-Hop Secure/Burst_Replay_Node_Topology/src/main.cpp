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
enum states {WAIT, CAPTURE, ACTIVE, DEAD}; 

//Constants and Assumptions
//const String NODES[] = {"BB"};
const PROGMEM int QUEUE_SIZE = 11;                                  // Max size of the queue                                  
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES+1) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 70;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 101;                               // energy harvest rate
const PROGMEM unsigned long DELAY = 10000;                                    // The delay the node waits to burst send the packets (in ms)
const PROGMEM int INTERVAL = 10;                                    // The delay for how fast the program sends the packets for burstSend
//Setup data structures and global variables
states state;
flags myFlags;
String sync_packet_queue[QUEUE_SIZE] = {};
String ack_packet_queue[QUEUE_SIZE] = {};
String data_packet_queue[QUEUE_SIZE] = {};
int sync_packet_queue_position = 0;
int ack_packet_queue_position = 0;
int data_packet_queue_position = 0;
String temp_data;

//Set transmission time
const unsigned long TRANSMIT_TIME = (myFlags.ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2); // time in the cycle to transmit TRANSMIT_TIME


//Function Definitions
void baseFSM();
void send(String output);
bool receive();
unsigned long cycleTime();
bool energyAvailible();
void pushQueue(String queue[], int& i);
void burstSend(String queue[], int& i);

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
        pushQueue(sync_packet_queue, sync_packet_queue_position);
        //Serial.println("I captured this sync: " + myFlags.temp_data);
      }
      else if(myFlags.is_ack) {
        pushQueue(ack_packet_queue, ack_packet_queue_position);
        //Serial.println("I captured this ack: " + myFlags.temp_data);
      }
      else if(myFlags.is_data) {
        pushQueue(data_packet_queue, data_packet_queue_position);
        //Serial.print("I captured this data: ");
        //Serial.println(myFlags.temp_data);
      }

      myFlags.is_sync = 0; myFlags.is_ack = 0; myFlags.is_data = 0; //Reset all the flags

      if(sync_packet_queue_position > QUEUE_SIZE - 1 || 
        ack_packet_queue_position > QUEUE_SIZE - 1 ||
        data_packet_queue_position > QUEUE_SIZE - 1) {
          state = WAIT; //If at least one queue has more than PACKETS_HELD packets
        }
      break;
    }
    case(WAIT): {
            delay(DELAY);
            state = ACTIVE;
            break;
        }
    case ACTIVE: {
      // See if energy is available
      bool isenergyAvailible = energyAvailible();

      //If there is energy, replay all the packets gained all at once
      if(isenergyAvailible) {
         burstSend(sync_packet_queue, sync_packet_queue_position);
         burstSend(ack_packet_queue, ack_packet_queue_position);
         burstSend(data_packet_queue, data_packet_queue_position);
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

/** pushQueue()
 * Adds a new value to an array acting as a queue
 * 
 * @param String queue[]: The array acting as a queue that gets pushed to
 * @param int& i:         The incrementing variable's address that is passed by reference
 */
void pushQueue(String queue[], int& i) {
  if(i > QUEUE_SIZE - 1) {
    i = 0 ; //Reset to top of queue so it loops
  }
  queue[i] = temp_data; //puts the packet into the sync queue
  i++;                          //Decrement variable
}
/** burstSend(int& queue, int& i)
 * Empties the entirity of an array and sends it over wireless in LIFO style
 * 
 * @param String queue[]: The array acting as a queue that will get emptied
 * @param int& i:         The incrementing variable's address that is passed by reference that will be incremented back to QUEUE_SIZE
 */
void burstSend(String queue[], int& i) {
  for(int j = 0; j < i; j++) {
    send(queue[j]);
    queue[j] = "";
    delay(INTERVAL);
  }
  i = 0;
}
