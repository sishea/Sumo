#include <ZumoBuzzer.h>
#include <ZumoMotors.h>
#include <Pushbutton.h>
#include <QTRSensors.h>
#include <ZumoReflectanceSensorArray.h>

//#define LOG_SENSOR	      // write sensor readings to serial port
//#define LOG_STATE	        // write state to serial port
//#define MUTE		          // disable buzzer

#define LED 13              // the digital pin that the user LED is attached to
#define DISTANCE_SENSOR A1  // the analog pin that the distance sensor is attached to

// Line Sensor Parameter
// (this might need to be tuned for different lighting conditions, surfaces, etc.)
#define QTR_THRESHOLD  1500 // microseconds

// Distance Sensor Parameters
// (this might need to be tuned for different lighting conditions, sensor alignment, etc.)
#define DETECTED_RANGE 100  // an object within this range is a target
#define CLOSE_RANGE   300   // object is in range to attack

#if defined(LOG_SENSOR) || defined(LOG_STATE)
// no movement to set up sensors and state machine
#define REVERSE_SPEED     0 // 0 is stopped, 400 is full speed
#define TURN_SPEED        0
#define FORWARD_SPEED     0
#define ATTACK_SPEED      0
#else
// Motor Speed Parameters
// (these might need to be tuned for different motor types)
#define REVERSE_SPEED     200 // 0 is stopped, 400 is full speed
#define TURN_SPEED        200
#define FORWARD_SPEED     200
#define ATTACK_SPEED      400
#endif

#define REVERSE_DURATION  250 // ms
#define TURN_DURATION     500 // ms
#define HUNT_DURATION     500 // ms

// Command Queue
#define MAX_COMMANDS 10
unsigned long startTime = millis();   // the time in milliseconds that the command queue started executingCommands
unsigned long nextCommand = millis(); // the time that the queue should execute the next command
unsigned int commandIndex;
unsigned int numberOfCommands;
int leftSpeed[MAX_COMMANDS];
int rightSpeed[MAX_COMMANDS];
unsigned int commandDuration[MAX_COMMANDS];
unsigned int turnDirection = 1;

// Finite State Machine Setup
//  States
enum State {
	SURVIVE, HUNT, TARGET, ATTACK//, EVADE
};
State state;
//  Events
bool opponentDetected;
bool edgeDetected;
//bool inContact;
bool executingCommands;

//  Guards / properties
#define NUM_SENSORS 6
unsigned int sensor_values[NUM_SENSORS];
unsigned int distance;

// Hardware Setup
ZumoMotors motors;
Pushbutton button(ZUMO_BUTTON); // pushbutton on pin 12
ZumoReflectanceSensorArray sensors(QTR_NO_EMITTER_PIN);

// Sound Effects
#ifndef MUTE
ZumoBuzzer buzzer;
const char charge_sound_effect[] PROGMEM = "O4 T100 V15 L4 MS g12>c12>e12>G6>E12 ML>G2"; // "charge" melody - use V0 to suppress sound effect; v15 for max volume
//const char bump_sound_effect[] PROGMEM = "O4 T100 V15 L4 g16<g2"; // "bump" sound
const char edge_sound_effect[] PROGMEM = "O4 T100 V15 L4 g12>c12"; // "edge" sound
const char hunt_sound_effect[] PROGMEM = "O4 T100 V15 L4 g12"; // "hunt" sound
const char target_sound_effect[] PROGMEM = "O4 T100 V15 L4 >E12"; // "target" sound
const char attack_sound_effect[] PROGMEM = "O4 T100 V15 L4 ML>G2"; // "attack" sound
#endif // !MUTE

// Program Startup
void setup()
{
	// uncomment if necessary to correct motor directions
	//motors.flipLeftMotor(true);
	//motors.flipRightMotor(true);

	// Wait for the button to be pressed and release befor a countdown to the start
	pinMode(LED, HIGH);
	waitForButtonAndCountDown();

#if defined(LOG_SENSOR) || defined(LOG_STATE)
	Serial.begin(9600);
#endif
}

// Program Execution
void loop()
{
	// If the button is pressed while the robot is operating, stop and wait for another press to go again
	if (button.isPressed())
	{
		motors.setSpeeds(0, 0);
		button.waitForRelease();
		waitForButtonAndCountDown();
	}

	// Go!
	updateFromSensors();
	checkStateTransition();
	executeCommands();

#ifdef LOG_SENSOR
	delay(500);
#endif
}

void waitForButtonAndCountDown()
{
	digitalWrite(LED, HIGH);
	button.waitForButton();
	digitalWrite(LED, LOW);

	// play audible countdown

#ifndef MUTE
	buzzer.playFromProgramSpace(charge_sound_effect);
#endif
	delay(2500);

	hunt();
}

// This function reads the sensors and updates the events and parameters that determine behaviour
void updateFromSensors()
{
#ifdef LOG_SENSOR
	String debugString = "Distance: ";
	debugString += analogRead(DISTANCE_SENSOR);
	debugString += "  Left: ";
	debugString += sensor_values[0];
	debugString += "  Right: ";
	debugString += sensor_values[5];
	debugString += "  Bump: ";
	Serial.println(debugString);
#endif

	// set if either line sensor detects an edge
	sensors.read(sensor_values);
	edgeDetected = (sensor_values[0] < QTR_THRESHOLD) || (sensor_values[5] < QTR_THRESHOLD);

	// set if the distance sensor sees something
	distance = analogRead(DISTANCE_SENSOR);
	opponentDetected = distance >= DETECTED_RANGE;

	// accelerometer
	// TODO

	// compass
	// TODO
	//  rotationComplete = false;
}

// This is a (very) simple implementation of a Finite State Machine to represent behaviour.
//
// The code checks the events and guards with context of the current state and determines whether to transition to another behavioural state.
void checkStateTransition()
{
	// check for transition from or execute the current state
	switch (state)
	{
	case SURVIVE:
		if (!executingCommands) hunt();
		break;
	//case EVADE:
	//	if (edgeDetected) survive();
	//	else if (!executingCommands) hunt();
	//	break;
	case ATTACK:
		if (edgeDetected) survive();
		else if (!opponentDetected) hunt();
		// Oponent still Detected - check guard
		else if (distance < CLOSE_RANGE) target();
		break;
	case TARGET:
		if (edgeDetected) survive();
		//else if (inContact) evade();
		// Lost oponent
		else if (!opponentDetected) hunt();
		// Oponent still Detected - check guard
		// Attack if close
		else if (distance >= CLOSE_RANGE) attack();
		// Continue to target
		break;
	default:  // HUNT
		if (edgeDetected) survive();
		//else if (inContact) evade();
		else if (opponentDetected)
		{
			// Oponent Detected - check guard
			if (distance >= CLOSE_RANGE) attack();
			else target();
		}
		else if (!executingCommands) hunt();
		break;
	}
}

// Survival Mode - rotate away from the border
void survive()
{
#ifdef LOG_STATE
	Serial.println("SURVIVE");
#endif

	state = SURVIVE;

#ifndef MUTE
	buzzer.playFromProgramSpace(edge_sound_effect);
#endif

	if (sensor_values[0] < QTR_THRESHOLD) turnDirection = 1;
	else turnDirection = -1;

	clearCommands();

	addCommand(-REVERSE_SPEED, -REVERSE_SPEED, REVERSE_DURATION);

	if (sensor_values[0] < QTR_THRESHOLD && sensor_values[5] < QTR_THRESHOLD)
	{
		addCommand(turnDirection * TURN_SPEED, -turnDirection * TURN_SPEED, TURN_DURATION);
	}

	addCommand(turnDirection * TURN_SPEED, -turnDirection * TURN_SPEED, TURN_DURATION);
	addCommand(FORWARD_SPEED, FORWARD_SPEED, REVERSE_DURATION);
	startExecutingCommands();
}

// Hunt - search for the oponent
void hunt()
{
#ifdef LOG_STATE
	Serial.println("HUNT");
#endif

	state = HUNT;

#ifndef MUTE
	buzzer.playFromProgramSpace(hunt_sound_effect);
#endif

	clearCommands();
	addCommand(FORWARD_SPEED, FORWARD_SPEED, HUNT_DURATION);
	addCommand(turnDirection * TURN_SPEED, -turnDirection * TURN_SPEED, HUNT_DURATION);
	startExecutingCommands();
}

// Target - once the opponent is detected, position for an attack
void target()
{
#ifdef LOG_STATE
	Serial.println("TARGET");
#endif

	state = TARGET;

#ifndef MUTE
	buzzer.playFromProgramSpace(target_sound_effect);
#endif

	clearCommands();
	addCommand(FORWARD_SPEED, FORWARD_SPEED, 0);
	startExecutingCommands();
}

// Got them where you want them? Attack!
void attack()
{
#ifdef LOG_STATE
	Serial.println("ATTACK");
#endif

	state = ATTACK;

#ifndef MUTE
	buzzer.playFromProgramSpace(attack_sound_effect);
#endif

	clearCommands();
	addCommand(ATTACK_SPEED, ATTACK_SPEED, 0);
	startExecutingCommands();
}

// *** Command Queue ***
// stop executing commands and reset the queue
void clearCommands()
{
	executingCommands = false;
	numberOfCommands = 0;
	nextCommand = millis();
}

// start executing commands from the start of the queue
void startExecutingCommands()
{
	executingCommands = true;
	commandIndex = 0;
	startTime = millis();
	nextCommand = startTime;  // execute the first command immediately
}

// add a command to the queue
void addCommand(int left, int right, int duration)
{
	if (numberOfCommands < MAX_COMMANDS)
	{
		// set the command parameters
		leftSpeed[numberOfCommands] = left;
		rightSpeed[numberOfCommands] = right;
		commandDuration[numberOfCommands] = duration;

		// increment the number of commands
		numberOfCommands++;
	}
}

void executeCommands()
{
// check if the current command is completed
	if (millis() >= nextCommand)
	{
		// execute the next command
		if (commandIndex < numberOfCommands)
		{
			motors.setSpeeds(leftSpeed[commandIndex], rightSpeed[commandIndex]);
			nextCommand = nextCommand + commandDuration[commandIndex];
			commandIndex++;
		}
		// commands completed
		else executingCommands = false;
	}
}
