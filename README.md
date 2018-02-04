# Sumo Robot Basics 
* *Sensors* - a device which detects or measures a physical property
  * Digital – reflectance / line sensors
  * Analog – distance sensor
* *Controller* - a device that manages the behaviour of other devices or systems
  * Reads sensor inputs;
  * Determines action; and
  * Manages actuators.
* *Actuators* - a device that converts an electrical control signal into mechanical motion
  * Drive motors; Buzzer

# Code Overview
The code is created with a *very* simple state machine and command queue. States and transitions are shown below.

![State Machine](https://github.com/sishea/Sumo/blob/master/Images/StateMachine.png)
**Trigger \[Guards\] / Activity**

## State Machine Code Snippet
```C
void checkStateTransition()
{
  // check for transition from or execute the current state
  switch(state)
  {
  case SURVIVE:
    if (!executingCommands) hunt();
 case ATTACK:
    if (edgeDetected) survive();
    else if (!opponentDetected) hunt();
  case TARGET:
    if (edgeDetected) survive();
    else if (!opponentDetected) hunt();
    // check guard
    else if (distance >= CLOSE_RANGE) attack();
  default:  // HUNT
    if (edgeDetected) survive();
    else if (opponentDetected)
    {
      // check guard
      if(distance >= CLOSE_RANGE) attack();
      else target();
    }
  }
}
```

## Command Queue Code Snippet

**Commands \[Left Speed, Right Speed, Duration\]**

```C
// Survival Mode - rotate away from the border or evade opponent
void survive()
{
  state = SURVIVE;

  if(sensor_values[0] < QTR_THRESHOLD) turnDirection = 1;
  else turnDirection = -1;

  clearCommands();

  addCommand(-200, -200, 250);

  if (sensor_values[0] < QTR_THRESHOLD && sensor_values[5] < QTR_THRESHOLD)
  {
    addCommand(turnDirection * 200, -turnDirection * 200, 500);
  }  

  addCommand(turnDirection * 200, -turnDirection * 200, 250);
  addCommand(200, 200, 500);
  startExecutingCommands();
}
```
# Tips
* Modify speed, time and distance limits in the parameters
* Change the Trigger / Guard in the state transitions
* Change the commands undertaken in the State Changed activities
