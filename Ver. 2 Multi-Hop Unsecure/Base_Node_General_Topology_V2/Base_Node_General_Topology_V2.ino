

/* Constants and assumptions*/
const String HEARABLE[] = {"03"};
const PROGMEM int TOTAL_NODES = 3;                                 // Total number of nodes in the network
const PROGMEM int TIME_SLOT = 1000;                                  // amount of time per slot in milliseconds (ms) 10^-3
const PROGMEM unsigned long CYCLE_LENGTH = (TOTAL_NODES) * TIME_SLOT; // total length of one cycle
const PROGMEM int ERROR = 60;                                       // Transmission time error threshold
const PROGMEM int ENERGY_CHANCE = 80;                               // energy harvest rate
const PROGMEM int TRANSMIT_TIME = TIME_SLOT * TOTAL_NODES + (TIME_SLOT / 2);


/* FLAGS... and stuff*/
bool is_sent = false;              // checks if a message was sent this cycle
bool sync_flag = false;

/* Timers */
long time_sent = 0;                   // the time the previous node sent the message
unsigned long time_in = 0;            // local arrival time, then converted to global arrival time, ideally the same as time_sent
unsigned long last_packet_in = 0;     // used for checking if we are not getting messages. if no messages in 3 cycles, reset the network
unsigned long last_sent = 0;

/* Transmition stuff */
int num_syncs = 0;             // the number of syncs to send out if sync list

/*  Data stuff  */
const char node1[] PROGMEM = "01";
const char node2[] PROGMEM = "02";
const char node3[] PROGMEM = "03"; 
const char node4[] PROGMEM = "04"; 
const char node5[] PROGMEM = "05";

const char all_nodes[TOTAL_NODES] = {node1, node2, node3};
unsigned long last_node_time[TOTAL_NODES];
String data[TOTAL_NODES];
byte needs_sync[TOTAL_NODES];


void baseFSM();
bool readData();
bool isError();
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
      //extra timer to prevent spam
      if(millis() - last_sent > 2 * CYCLE_LENGTH){
        Serial.println(F("G,BB,E,"));// send a general sync message 
        Serial.flush(); 
        last_sent = millis();
      }
      state = ACTIVE;
      sync_flag = false;
      //resets any recorded overlap errors
      for(byte node:needs_sync){
        node = 0;
      }
      break;
      

    case ACTIVE:
      if(readData()){
        if(isError(last_packet_in)){
          state = SYNC_ALL;
          break;
        }
      }

      if(millis() - last_packet_in > 5 * CYCLE_LENGTH){
        state = SYNC_ALL;
        break;
      }

      /*
      unsigned long time = millis();
      for(unsigned long node_time : last_node_time){
        if(time - node_time > 4 * CYCLE_LENGTH){
          sync_flag = true;
          node_time = time;
        }
      }*/

      if(sync_flag == true){
        state = SYNC_ALL;
        break;
      }

      break;
      
    case SYNC_LIST:
      // adjust the nodes needing syncing
      // we sync the node that reads the error and the node that "creates" it. We update the nodes that creat the issue here
      for(int i = 1; i < TOTAL_NODES; i++){
        if(needs_sync[i] == 1){
          if(needs_sync[i - 1] == 0){
            needs_sync[i - 1] = 1;
          }
        }
      }
      
      // adds resyncs for missing nodes and counts total number of resyncs
      for(int i = 0; i < TOTAL_NODES; i++){
        if(data[i] == F("EE")){
          needs_sync[i] = 0;
        }
        if(needs_sync[i] != 0){
          num_syncs++;
        }
      }

      // prints the sync list if needed
      if(num_syncs > 0){
        Serial.println(F("G,BB,E,"));// send a general sync message 
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
        node = 0;
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
  if(Serial.available() < 5){ return false; } // is there anything to read?

  String type = Serial.readStringUntil(',');      // grabs message type
  String data_in = Serial.readStringUntil(',');   // grabs the first data
  String sender = data_in.substring(0,2);         // extracts sender from data
  if(!isHearable(sender)){ return false; }  // if unhearable message

  last_packet_in = millis();                      // grabs time in
  data[sender.toInt()-1] = sender;
  last_node_time[sender.toInt()-1] = millis();    // time message in for timeout stuff
  needs_sync[sender.toInt()-1] = (byte)String(data_in[2]).toInt();

  

  while(true){
    String p_data = Serial.readStringUntil(',');
    if(millis() - last_packet_in > CYCLE_LENGTH){ break; } // time out
    if(p_data == "E"){ break; } // 
    int data_idx = p_data.substring(0,2).toInt();
    data[data_idx-1] = p_data.substring(0,2);
    last_node_time[data_idx-1] = millis();
    needs_sync[data_idx-1] = (byte)String(p_data[2]).toInt();
    data_in = data_in + "," + p_data;
  }
  // time_sent = (sender.toInt() - 1) * TIME_SLOT + (TIME_SLOT / 2); // relic of the past
  Serial.readStringUntil('\n');
  return true;
}

// isError() -- Verified working
// returns the check for overlap errors
bool isError(unsigned long time){
  static unsigned long last_time;
  if(time - last_time < ERROR){
    last_time = 0;
    return true;
  }

  for(byte node : needs_sync){
    if(node != 0){
      last_time = 0;
      return true;
    }
  }

  last_time = time; // for next time we call the function
  return false;
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