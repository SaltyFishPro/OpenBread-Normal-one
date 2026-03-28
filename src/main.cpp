#include <Arduino.h>

#include "app/App.h"

App app;

void setup() { app.begin(); }

void loop() { app.tick(); }
