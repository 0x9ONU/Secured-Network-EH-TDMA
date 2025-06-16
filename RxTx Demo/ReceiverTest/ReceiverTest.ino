void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Serial.setTimeout(10);
}

void loop() {
  // put your main code here, to run repeatedly:

  String input;
  if(Serial.available() > 0){
    input = Serial.readStringUntil('\r'); 
    Serial.print(input);
  }
}
