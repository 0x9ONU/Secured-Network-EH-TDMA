

/* Constants and assumptions*/
const String HEARABLE[] = {};                          // List of nodes that this node can hear (not including base station)
const String ID = "03";                                    // Each node knows its ID based on assumption
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of sensor nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                 // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long ERROR = 70;                            // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 101;                             // energy harvest rate

const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES + 1) * TIME_SLOT;   // total length of one cycle
const unsigned long TRANSMIT_TIME = (ID.toInt() - 1) * TIME_SLOT +(TIME_SLOT / 2); // time in the cycle to transmit TRANSMIT_TIME

/* FLAGS... and stuff*/
bool updated = false;       // tracks if we need to read a time for syncing or if we already did that
bool is_sync = false;       // keeps track of if the last read message was a sync
bool is_sent = false;       // checks if a message was sent this cycle
bool overlap_check = false; // we only check overlap if it gets a data packet

/* Timers */
long offset = 0;                    // offset from the node's cycle to the global cycle
long global_time = 0;               // the time the previous node sent the message
unsigned long time_in = 0;          // time that this node received the message
unsigned long time_in_U = 0;        // time_in but updated to global time

/* Transmition stuff */
String data_in = ",E,";             // the data coming in

/* Function headers */
void nodeFSM();
bool energyAvailible();
bool readData();
unsigned long cycleTime();
bool isHearable(String sender);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.setTimeout(30);
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
      if(energyAvailible()) {
        //--reset flags--//
        updated = false;
        is_sync = false;
        is_sent = false;
        overlap_check = false;
        //--reset timers--//
        offset = 0;
        global_time = 0;
        time_in = 0;
        time_in_U = 0;
        //--reset data--//
        data_in = ",E,";                
        // to sync
        state = SYNC;
      }
      break;
    
    case SYNC: // -- Verified working
      //--for if we are waiting for a message to get time from--//
      /*
      if(!updated && readData()){
        is_sent = false;
      }*/
      readData();
      //--for if we have the time to sync off of--//
      if(updated){
        offset = (global_time - (long)time_in); // add 50ms to account for process timing found during early testing
        if(offset < 0) {
          offset = (long)CYCLE_LENGTH + offset;
        }
        time_in_U = (time_in + offset) % CYCLE_LENGTH;
        state = WAIT;
        updated = false; // reset flag
      }
      is_sent = true;
      break;

    case WAIT: // -- Verified working
      is_sync = false;
      //--if in time slot--//
      if(cycleTime() >= TRANSMIT_TIME && !is_sent){ 
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
      //--check for overlap errors--//
      int clock_diff = time_in_U - global_time; 
      int is_overlap = 2; // base case, all good
      if(overlap_check && (time_in_U > global_time + TIME_SLOT + (TIME_SLOT / 2) - ERROR  || time_in_U < global_time - (TIME_SLOT / 2))){
          is_overlap = 1;        //There is an error
      }
      //--send the data--//
      Serial.print(ID);
      Serial.print(",D,");
      Serial.print(cycleTime());
      Serial.print(",");
      Serial.print(ID);
      Serial.print(is_overlap);
      Serial.println(data_in);
      Serial.flush();
      //--reset--//
      data_in = ",E,";
      is_sent = true;
      overlap_check = false;
      //--energy checking--//
      if(energyAvailible()){
        state = WAIT;
        break;
      }
      else {
        state = DEAD;
        break;
      }
      break;
  }
}

// energyAvailible() // -- Verified working
// RNG for if a node has energy or not, based on the chances of that node having energy
bool energyAvailible(){
  if(random(0,100) <= ENERGY_CHANCE){
    return true;
  } 
  else return false;
}

// readData() // -- Verified working
// Helper function that updates the variables that hold the data. Created to simplify code. (and improve efficency)
// If it reads data, returns true
// Test messages A,G,1234    1,D,1234,012,E,     A,S,1234,02,0102
bool readData(){
  if(Serial.available() <= 0){
    return false;
  }
  String sender = Serial.readStringUntil(',');
  if(isHearable(sender)){
    updated = true;
    time_in = millis() % CYCLE_LENGTH;          // grabs the nodal time of receiving
    String type1 = Serial.readStringUntil(','); // grabs the type of message
    global_time = Serial.parseInt();            // grabs the global time from sender
    
    if(type1 == "D") {
      if(global_time < TRANSMIT_TIME){
        data_in = Serial.readStringUntil('\r');
        if(!data_in.endsWith(",E,")){
          data_in = data_in + ",E,";
        }
        Serial.read();
        overlap_check = true;
      }
      is_sync = false;
    }
    
    else if(type1 == "S") {
      long num_syncs = Serial.parseInt();
      Serial.readStringUntil(',');
      String sync_list = Serial.readStringUntil(',');
      //--linear search through all node IDs--//   REPLACE WITH BINARY SEARCH EVENTUALLY

      for(int i = 0; i < num_syncs; i++){
        String to_check = sync_list.substring((i*2), (i*2)+2);
        if(to_check == ID){ //if this node finds it's ID on the sync list
          is_sync = true;
          break;
        }
      }
      Serial.readStringUntil('\n');
    }

    else if(type1 == "G"){
      is_sync = true;
      Serial.readStringUntil('\n');
    }
    return true; // if theres a message
  }
  Serial.readStringUntil('\n');
}

// cycleTime() -- Verified working
// helper function to keep the time in the range of one cycle and incorporate the offset
// also resets the is_sent variable so we can send a new message if we get to a new cycle
unsigned long cycleTime(){
  static unsigned long last_time;
  long time = ((long)(millis() % CYCLE_LENGTH) + offset) % (long)CYCLE_LENGTH;
  if(last_time > time){ 
    is_sent = false; 
  } // checks if the clock reset and resets is_sent
  last_time = time; // for next time we call the function
  return (unsigned long)time;
}

// inInZone()
// helper to find if the message is for this node
bool isHearable(String sender){
  if(sender == "B"){ return true; }
  for(String i:HEARABLE){
    if(i == sender){
      return true;
    }
  }
  return false;
}


