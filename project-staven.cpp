// This #include statement was automatically added by the Particle IDE.
#include <HC_SR04.h>

// This #include statement was automatically added by the Particle IDE.
#include <PietteTech_DHT.h>

// This library is used to allow access to commong Particle Functions
#include "Particle.h"

// Needed libraries for general functions
#include <math.h>
#include <deque>

// Set up definitions for DHT22 Sensor.
#define DHTTYPE  DHT22       // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   D4           // Digital pin for communications

// Instantiate Lib for DHT22
PietteTech_DHT DHT(DHTPIN, DHTTYPE);

// Set up consts for HC_SR04 Ultra Sonic
const int triggerPin = D1;
const int echoPin = D0;
HC_SR04 rangefinder = HC_SR04(triggerPin, echoPin);

// Set up consts for PIR (SR505)
const int outPin = D2;

// Set up consts for buzzer (localAlert)
const int buzzerPin = D6;

// Set up consts for resetButton
const int buttonPin = D8;

// Global Variables
const double ConnectedAlertTimePeriod = 0.25;   // Timer till ConnectedAlert triggered - Set in minutes
const double localAlertTimePeriod = 0.25;      // Timer till localAlert triggered - Set in minutes

bool localAlert = false;
long int noMotionAt = 0;

bool movement = false;
bool lastMovement = false;

int tempC = 0;
int tempAtAlert = 0;

double cm = 0;
double lastCM = 0;


long int lastVTime = 0;

bool resetNeeded = false;
bool eventPublished = false;

std::deque<int> tempRecs;



void setup() {
    // Start up DHT sensor
    DHT.begin();
    
    // Set pinMode on devices
    pinMode(buzzerPin, OUTPUT);     // Buzzer
    pinMode(buttonPin, INPUT);      // Reset button
    pinMode(outPin, INPUT);         // PIR
    
    // Get initial values for variables
    // Todo
    
}

void loop() {
    
    // Reset sequence
    if(digitalRead(buttonPin) == HIGH && resetNeeded == true) {
        resetNeeded = false;
        eventPublished = false;
        localAlert = false;
        noMotionAt = 0;
        tempAtAlert = 0;
        digitalWrite(buzzerPin, LOW);
    }
    
    // Get current readings from devices.
    // DHT
    DHT.acquireAndWait(1000);
    tempC = DHT.getCelsius();
    // Particle.publish("DHT", String(tempC));
    
    // HC_SR04
    cm = rangefinder.getDistanceCM();
    // Particle.publish("Distance", String(cm));
    
    // PIR Motion
    movement = readPIR();
    // Particle.publish("Movement", String(movement));
    
    // Elevate Alerts based on last time motion was not recorded.
    // setting local alert first
    if(localAlert == false && (timeDiffSec(Time.now(), noMotionAt) >= (localAlertTimePeriod * 60))) {
        Particle.publish("DEBUG", "Checking Trends");
        // Check if overall trend has been going up
        int trend = 0;
        for (int i = 0; i < tempRecs.size() - 2; ++i) {
            int diff = tempRecs[i+1] - tempRecs[i];
            if (diff >= 0) {
               trend++;
            } else if (diff < 0) {
               trend--;
            }
        }
        
        Particle.publish("DEBUG", "Done Checking Trends");
        if(trend > 0) {
            // Stove has been left on for last 10 minutes
            Particle.publish("LocalAlert", String(Time.now()) + ":" + String(noMotionAt));
            localAlert = true;
            resetNeeded = true;
            tempAtAlert= tempC;
            digitalWrite(buzzerPin, HIGH);
        }
        
    } else if(localAlert == true && resetNeeded == true) {
        // Setting ConnectedAlert (push notifications/emails)
        if((timeDiffSec(Time.now(), noMotionAt) >= ((localAlertTimePeriod * 60) + (ConnectedAlertTimePeriod * 60))) && eventPublished == false) {
            Particle.publish("ConnectedAlert", "TRUE");
            eventPublished = true;
        }
    }
    
    // If no motion detected, flag and set noMotionAt to current time. 
    // If localAlert already set don't trigger value updates as timer already initiated. 
    if(localAlert == false && (distDiffCalc(cm, lastCM, '<', 10) || movement == false)) {
        // Delay 1 second and recheck values to ensure correct flag setting
        delay(1000);
        double reCheckDist = rangefinder.getDistanceCM();
        bool reCheckMovement = readPIR();
        if(distDiffCalc(cm, reCheckDist, '<', 10) && reCheckMovement == false && noMotionAt == 0) {
            noMotionAt = Time.now();
            Particle.publish("noMotionAt Set", String(noMotionAt));
        }
    } else if(noMotionAt != 0 && localAlert == false && (movement == true || distDiffCalc(cm, lastCM, '>', 10))) {
        noMotionAt = 0;
        Particle.publish("noMotionAt", "RESET");
    }
    
    // update values for next round
    lastCM = cm;
    lastMovement = movement;
    
    
    // Store last temp reading every 10 minutes
    if(tempC >= 0) {
        if(Time.now() % 60 == 0) {
            if(tempRecs.size() == 10){
                tempRecs.pop_front();
            }
            tempRecs.push_back(tempC);
        }
    }

    // delay for 5 seconds cool off between readings.
    delay(2500);
    
}

int timeDiffSec(long int newTime, long int oldTime) {
    if(oldTime == 0 || oldTime > newTime) {
        return 0;
    } else {
        return (newTime - oldTime);
    }
} 

bool readPIR() {
    int pirOutput = digitalRead(outPin);
    if(pirOutput == LOW ) {
        return false;
    } else if(pirOutput == HIGH) {
        return true;
    } else {
        Particle.publish("SENSOR ERROR", "PIR Sensor is not working correctly. Sensor may need repairing.");
        exit;
    }
}

bool distDiffCalc(double len1, double len2, char oper, double threshold) {
    if(oper == '<') {
        if(fabs(len2 - len1) < threshold) {
            return true;
        }
    } else if(oper == '>') {
        if(fabs(len2 - len1) > threshold) {
            return true;
        }   
    }
    
    return false;
}
