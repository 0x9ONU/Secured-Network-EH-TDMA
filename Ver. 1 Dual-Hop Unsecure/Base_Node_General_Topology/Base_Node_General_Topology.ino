

/* Constants and assumptions*/
const String HEARABLE[] = {"01", "02", "03"};
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES+1) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 70;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 101;                               // energy harvest rate
const PROGMEM int TRANSMIT_TIME = TIME_SLOT * TOTAL_NODES + (TIME_SLOT / 2);


/* FLAGS... and stuff*/
bool is_sent = false;              // checks if a message was sent this cycle

/* Timers */
long time_sent = 0;                   // the time the previous node sent the message
unsigned long time_in = 0;            // local arrival time, then converted to global arrival time, ideally the same as time_sent
unsigned long last_packet_in = 0;     // used for checking if we are not getting messages. if no messages in 3 cycles, reset the network


/* Transmition stuff */
int num_syncs = 0;             // the number of syncs to send out if sync list

/*  Data stuff  */
const char node1[] PROGMEM = "01";
const char node2[] PROGMEM = "02";
const char node3[] PROGMEM = "03"; 
const char node4[] PROGMEM = "04"; 
const char node5[] PROGMEM = "05";

const char all_nodes[TOTAL_NODES] = {node1, node2, node3};
String data[TOTAL_NODES];
byte needs_sync[TOTAL_NODES];


void baseFSM();
bool readData();
unsigned long cycleTime();
bool isHearable(const String& sender);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.setTimeout(30);

}

void loop() {
  // put your main code here, to run repeatedly:
  baseFSM();
}


// baseFSM()
// The function that implements the states and their functionality in the base
void baseFSM(){
  static enum { SYNC_ALL, ACTIVE, SYNC_LIST } state = SYNC_ALL;
  switch(state){
    case SYNC_ALL:
      Serial.print(F("B,G,"));// send a general sync message 
      Serial.println(cycleTime());
      Serial.flush();           
      state = ACTIVE;
      break;
      

    case ACTIVE:
      readData();
      if(cycleTime() == TRANSMIT_TIME && !is_sent){
        is_sent = true;
        state = SYNC_LIST;
        break;
      }
      
      /*
      if(millis() - last_packet_in > 3 * CYCLE_LENGTH){
        state = SYNC_ALL;
        break;
      }*/

      break;
      
    case SYNC_LIST:
      // adjust the nodes needing syncing
      // we sync the node that reads the error and the node that "creates" it. We update the nodes that creat the issue here
      for(int i = 1; i < TOTAL_NODES; i++){
        if(needs_sync[i] == 1){
          if(needs_sync[i - 1] == 2){
            needs_sync[i - 1] = 1;
          }
        }
      }
      
      // adds resyncs for missing nodes and counts total number of resyncs
      for(int i = 0; i < TOTAL_NODES; i++){
        if(data[i] == F("EE")){
          needs_sync[i] = 2;
        }
        if(needs_sync[i] != 2){
          num_syncs++;
        }
      }

      // prints the sync list if needed
      if(num_syncs > 0){
        Serial.print(F("B,G,"));// send a general sync message 
        Serial.println(cycleTime());
        Serial.flush();        
        /*
        Serial.print(F("B,S,"));
        Serial.print(cycleTime());
        Serial.print(F(","));
        Serial.print(num_syncs);
        Serial.print(F(","));
        for(int i = 0; i < TOTAL_NODES; i++){
          if(needs_sync[i] != 2){
            Serial.print(data[i]);
          }
        }
        
        Serial.print(F(","));
        for(int i = 0; i < TOTAL_NODES; i++){
            Serial.print(data[i]);
            Serial.print(needs_sync[i]);
            Serial.print(F("|"));
        }
        Serial.println(F(","));
        */
      }
      // prints the data otherwise
      else{ 
        Serial.print(F("D_OUT"));
        
        for(int i = 0; i < TOTAL_NODES; i++){
            Serial.print(data[i]);
            Serial.print(needs_sync[i]);
            Serial.print("|");
        }
        Serial.println(F(","));
      }
      // resets the sync list info and data in
      num_syncs = 0;
      for(byte node:needs_sync){
        node = 2;
      }
      for(int i = 0; i < TOTAL_NODES; i++ ){
        data[i] = F("EE");
      }

      // if all nodes dead, SYNCA
      /*
      if(millis() - last_packet_in > 3 * CYCLE_LENGTH){
        state = SYNC_ALL;
        //reset everything
        for(byte node:needs_sync){
          node = 2;
        }
        is_sent = false;             
        time_sent = 0;                  
        time_in = 0;            
        num_syncs = 0;  
        break;
      }
      */

      state = ACTIVE;
      break;
      
  }
}

// readData()
// Helper function that updates the variables that hold the data. Created to simplify code. (and improve efficency)
// If it reads data, returns true
bool readData(){
  if(Serial.available() > 0){ // is there anything to read?
    String sender = Serial.readStringUntil(',');
    if(isHearable(sender)){
      last_packet_in = millis();
      String type = Serial.readStringUntil(',');
      time_in = cycleTime();
      if(type == F("D")){
        time_sent = Serial.parseInt();
        Serial.readStringUntil(',');
        //--if data--//
        String data_in = "";
        
        while(true){
          String p_data = Serial.readStringUntil(',');
          if(p_data == "E"){
            Serial.readStringUntil('\n');         
            break;// D,4500,102,092,082,072,063,052,042,033,022,012,E,
          }
          int data_idx = p_data.substring(0,2).toInt();
          data[data_idx-1] = p_data.substring(0,2);
          needs_sync[data_idx-1] = (byte)String(p_data[2]).toInt();
          data_in = data_in + "," + p_data;
        }
        //--check for overlap errors in incoming message--//
        if(time_in > time_sent + (TIME_SLOT / 2) - ERROR || time_in < time_sent - (TIME_SLOT / 2)){
          needs_sync[data_in.substring(1,3).toInt()-1] = 3;
        }
      } 
      Serial.readStringUntil('\n');
      return true;
    }
    Serial.readStringUntil('\n');
  }
  return false;
}

// cycleTime() -- Verified working
// helper function to keep the time in the range of one cycle and incorporate the offset
// also resets the is_sent variable so we can send a new message if we get to a new cycle
unsigned long cycleTime(){
  static unsigned long last_time;
  unsigned long time = millis() % CYCLE_LENGTH;
  if(last_time > time){ // checks if the clock reset and resets is_sent
    is_sent = false;
  }
  last_time = time; // for next time we call the function
  return time;
}


// inInZonbool isHearable(String sender)
bool isHearable(const String& sender){
  for(String i:HEARABLE){
    if(i == sender){
      return true;
    }
  }
  return false;
}