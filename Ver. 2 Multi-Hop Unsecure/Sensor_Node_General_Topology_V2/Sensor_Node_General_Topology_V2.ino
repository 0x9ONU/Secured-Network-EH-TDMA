

/* Constants and assumptions*/
const String HEARABLE[] = {"01","02", "03"};                          // List of nodes that this node can hear (not including base station)
const String ID = "01";                                    // Each node knows its ID based on assumption
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of sensor nodes in the network
const PROGMEM int TIME_SLOT = 1500;                                 // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long ERROR = 60;                            // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 40;                             // energy harvest rate

const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES) * TIME_SLOT;   // total length of one cycle
const unsigned long TRANSMIT_TIME = (ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2); // time in the cycle to transmit TRANSMIT_TIME

/* FLAGS... and stuff*/
bool has_time = false;      // tracks if we need to read a time for syncing or if we already did that
bool is_sync = false;       // keeps track of if the last read message was a sync
bool is_sent = false;       // checks if a message was sent this cycle
bool overlap_check = false; // we only check overlap if it gets a data packet

/* Timers */
long offset = 0;                    // offset from the node's cycle to the global cycle
long global_time = 0;               // the time the previous node sent the message
unsigned long time_in = 0;          // time that this node received the message
unsigned long time_in_U = 0;        // time_in but updated to global time

/* Transmition stuff */
String data_in = "E,";             // the data coming in
int is_overlap;

/* Function headers */
void nodeFSM();
bool energyAvailible();
bool readData();
unsigned long cycleTime();
bool isHearable(String sender);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.setTimeout(15);
  randomSeed(TRANSMIT_TIME);
}

void loop() {
  // put your main code here, to run repeatedly:
  nodeFSM();
}

// nodeFSM()
// The function that implements the states and their functionality in the nodes
void nodeFSM(){
  static enum { DEAD, SYNC, WAIT, ACTIVE } state = DEAD;
  switch (state) {
    case DEAD: // -- Verified working
      
        //--reset flags--//
        has_time = false;
        overlap_check = false;
        data_in = "E,";  
           
        // to sync
        state = SYNC;
      
      break;
    
    case SYNC:
      if(has_time){ //if we have a time to sync off of
        offset = (global_time - (long)time_in); // add 50ms to account for process timing found during early testing
        if(offset < 0) {
          offset = (long)CYCLE_LENGTH + offset;
        }
        time_in_U = (time_in + offset) % CYCLE_LENGTH; //update the time the message came in
        state = WAIT;
        has_time = false; // reset flag
        cycleTime();
        is_overlap = 0; //stays 0 until a resync is needed
      }
      else{ // read until we have a time
        readData();
      }
      break;

    case WAIT: // -- Verified working
      is_sync = false;
      //--if in time slot--//
      if(cycleTime() >= TRANSMIT_TIME && !is_sent && cycleTime() < TRANSMIT_TIME + TIME_SLOT - ERROR){ 
        state = ACTIVE; 

      }
      
      //--if not in time slot--//
      else if(readData()){
        time_in_U = cycleTime();
        if (is_sync){
          state = SYNC;
          break;
        }
      }
      break;

    case ACTIVE: // -- Verified working
      if(energyAvailible()){
        if(overlap_check && TRANSMIT_TIME - time_in_U < ERROR){ //if the last message came in too close to us sending
          is_overlap = 1;        //There is an error
        }
        //--send the data--//
        Serial.print("D,");
        Serial.print(ID);
        Serial.print(is_overlap);
        Serial.print(',');
        Serial.println(data_in);
        Serial.flush();
        //--reset--//
        overlap_check = false;
        
        state = WAIT;
      }
      else{
        state = DEAD;
      }
      is_sent = true;
      data_in = "E,";
      break;
  }
}

// energyAvailible() // -- Verified working
// RNG for if a node has energy or not, based on the chances of that node having energy
bool energyAvailible(){
  if(random(0,100) < ENERGY_CHANCE){
    return true;
  } 
  else return false;
}

// readData() // -- Verified working
// Helper function that updates the variables that hold the data. Created to simplify code. (and improve efficency)
// If it reads data, returns true
// Test messages A,G,1234    1,D,1234,012,E,     A,S,1234,02,0102

bool readData(){
  if(Serial.available() <= 0){ // if no message 
    return false;
  }
  
  has_time = true;
  time_in = millis() % CYCLE_LENGTH;             // grabs the nodal time of receiving
  String type = Serial.readStringUntil(',');     // grabs the type of message
  data_in = Serial.readStringUntil('\r'); // grabs the data
  String sender = data_in.substring(0,2);        // extracts the sender from data
  Serial.readStringUntil('\n');
  if(!isHearable(sender)){ return false; } // if unhearable message
    
  if(sender == "BB"){  // if base station
    if(type == "S"){  // sync list
      //sync list for later
    }
    else if(type == "G"){  // general sync
      global_time = 0;
      is_sync = true;
      has_time = true;
    }
    return true;
  }

  if(type != "D"){ return false; } // if weirdness between sender and type
  is_sync = false;
  overlap_check = true;
  if(!data_in.endsWith(",E,")){
    data_in = data_in + ",E,";
  }
  global_time = (sender.toInt() - 1) * TIME_SLOT + (TIME_SLOT / 2); // calculates the global time from sender ID
  if(global_time > TRANSMIT_TIME && !is_sent){ // for if node is supposed to send after us but we havent sent yet
    is_overlap = 3;
    overlap_check = false;
  }
  return true;
}

// cycleTime() -- Verified working
// helper function to keep the time in the range of one cycle and incorporate the offset
// also resets the is_sent variable so we can send a new message if we get to a new cycle
unsigned long cycleTime(){
  static unsigned long last_time;
  long time = ((long)(millis() % CYCLE_LENGTH) + offset) % (long)CYCLE_LENGTH;
  if(last_time > time){ 
    is_sent = false; 
    data_in = "E,";
  } // checks if the clock reset and resets is_sent
  last_time = time; // for next time we call the function
  return (unsigned long)time;
}

// inInZone()
// helper to find if the message is for this node
bool isHearable(String sender){
  if(sender == "BB"){ return true; }
  for(String i:HEARABLE){
    if(i == sender){
      return true;
    }
  }
  return false;
}


