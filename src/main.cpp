#include <Arduino.h>

#include "app/App.h"

App app;

void setup() {
  Serial.begin(115200);
  delay(300);
  app.begin();
}

void loop() { app.tick(); }
