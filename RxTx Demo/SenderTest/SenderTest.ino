void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Serial.setTimeout(56);
  Serial.println("INITIALIZE");
}

void loop() {
  // put your main code here, to run repeatedly:
  
  unsigned long currentTime = millis();
  if(currentTime%1000 == 0){
    Serial.println(currentTime);
    delay(10);
    }

}
