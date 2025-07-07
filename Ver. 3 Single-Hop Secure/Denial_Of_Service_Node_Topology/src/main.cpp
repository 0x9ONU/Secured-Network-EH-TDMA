#include <Arduino.h>

//Setup Ennumerations to Closely Match the FSM and Pseudocode
enum states {ACTIVE, DEAD}; 

//Constants and Assumptions
const PROGMEM int ENERGY_CHANCE = 101;                               // energy harvest te

//Setup data structures and global variables
states state;
//Function Definitions
void baseFSM();
void send(String input);
bool energyAvailible();
long randomData(int min, int max);
long randomData(int min, int max, int seed);


/** loop()
 * @attention Arduino needs this!
 * Put your setup code here, to run one time. Like a main function that is ran a single time BEFORE loop().
 * */ 
void setup() {
  //Setup the serial to begin running over the air and makes sure that the timeout is set properly
  Serial.begin(9600);
  Serial.setTimeout(30);

  randomSeed(analogRead(0));

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
        state = ACTIVE;
      }
    }
    case ACTIVE: {
      //If there is energy, replay all the packets gained all at once

      bool isEnergyAvailable = energyAvailible();

      while(isEnergyAvailable) {
        send(String(randomData(INT16_MIN, INT16_MAX)));
        isEnergyAvailable = energyAvailible();
      }
        
      state = DEAD;
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


/** bool energyAvailible()
 * is the RNG for if a node has energy or not, based on the chances of that node having energy
 * 
 * @returns true if there is energy available in the node, and false otherwise
*/
bool energyAvailible(){
  //Make it extra unlikely to have energy
  if(random(0,1000) <= ENERGY_CHANCE){
    return true;
  } 
  else return false;
}

/**
 * Generates a random number given a minimum and a maximum. The seed is set to the defualt seed
 * 
 * @param min  The minimum amount of time to wait
 * @param max  The maximum amount of time to wait
 * 
 * @returns a random number
 */
long randomData(int min, int max) {
  return random(min, max);
}

/**
 * Generates a random value given a seed, a minimum, and a maximum number
 * 
 * @param min  The minimum amount of time to wait
 * @param max  The maximum amount of time to wait
 * @param seed The seed for the PRNG
 * 
 * @returns a random number
 */
long randomData(int min, int max, int seed) {
  randomSeed(seed);
  return random(min, max);
}
