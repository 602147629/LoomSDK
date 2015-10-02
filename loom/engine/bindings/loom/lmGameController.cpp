/*
* ===========================================================================
* Loom SDK
* Copyright 2011, 2012, 2013
* The Game Engine Company, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* ===========================================================================
*/

/*
* This class is used to manage game controllers using SDL.
* It also serves as an interface between SDL and LoomSDK to allow game controller
* manipulation through LoomScript.
*/

#include "loom/engine/bindings/loom/lmGameController.h"
#include "loom/common/core/log.h"

using namespace LS;

/** A pool of controllers. */
LoomGameController LoomGameController::controllers[MAX_CONTROLLERS];

lmDefineLogGroup(controllerLogGroup, "loom.controller", 1, LoomLogInfo);

LoomGameController::LoomGameController()
{
    is_connected = false;
    is_haptic = false;
    controller = 0;
    instance_id = -1;
    haptic = 0;
}

/** Opens all connected game controllers.
*  This is usually only done at start of program.
*  Other controllers are usually added by using SDL_CONTROLLERDEVICEADDED event. */
void LoomGameController::openAll()
{
    int joyIndex = 0;
    for (int i = 0; i < SDL_NumJoysticks() && joyIndex < MAX_CONTROLLERS; i++)
    {
        LoomGameController::controllers[joyIndex++].open(i);
    }
}

/** Closes all opened game controllers.
*  This is done right before ending the program. */
void LoomGameController::closeAll()
{
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {
        LoomGameController::controllers[i].close();
    }
}

/** Opens a game controller from the game controller pool.
*  This is usually used with the SDL_CONTROLLERDEVICEADDED event.
*  Device index can be read from an SDL event using `event.cdevice.which` */
int LoomGameController::addDevice(int deviceID)
{
    // Iterate through the game controller pool and open a controller if a slot is free
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
    {
        LoomGameController& gc = controllers[i];
        if (!gc.is_connected)
        {
            gc.open(deviceID);
            return i;
        }
    }

    return -1;
}

/** Closes a game controller in the game controller pool.
*  This is usually used with the SDL_CONTROLLERDEVICEREMOVED event.
*  Device index can be read from an SDL event using `event.cdevice.which` */
int LoomGameController::removeDevice(int deviceID)
{
    // We need to get device's index in the game controller pool first in order to close it.
    int controllerIndex = getControllerIndex(deviceID);
    if (controllerIndex < 0) return -1;
    LoomGameController& gc = controllers[controllerIndex];
    gc.close();
    return controllerIndex;
}

/** Returns the number of opened game controllers. */
int LoomGameController::numDevices()
{
    int devices = 0;
    for (int i = 0; i < MAX_CONTROLLERS; i++)
    {
        if (LoomGameController::controllers[i].is_connected)
            ++devices;
    }
    return devices;
}

/** Returns the game controller's index in the game controller pool using the device's id. */
int LoomGameController::getControllerIndex(SDL_JoystickID instance)
{
    for (int i = 0; i < MAX_CONTROLLERS; i++)
    {
        if (LoomGameController::controllers[i].is_connected && LoomGameController::controllers[i].instance_id == instance) {
            return i;
        }
    }
    return -1;
}

/** Returns the pointer to a game controller on a specific index in the 'controllers' pool.
 * If index is negative it returns the first connected game controller in pool. */
LoomGameController *LoomGameController::getGameController(int index = -1)
{
    if (index < 0)
    {
        for (int i = 0; i < MAX_CONTROLLERS; i++)
            if (LoomGameController::controllers[i].is_connected)
                return &controllers[i];
    }
    else if (index < MAX_CONTROLLERS)
    {
        if (LoomGameController::controllers[index].is_connected)
            return &controllers[index];
    }
    return nullptr;
}

/** Returns true if device has rumble support. */
bool LoomGameController::isHaptic()
{
    return this->is_haptic;
}

/** Stops the device's rumble effect. */
void LoomGameController::stopRumble()
{
    // Do nothing if it is not a valid controller
    if (!this->is_connected) return;

    // If device is haptic, stop rumble
    if (this->is_haptic)
        SDL_HapticRumbleStop(this->getHaptic());
}

/** Starts the device's rumble effect.
*  Intensity sets the  is a value between 0 and 1
*  The duration of rumble is set in milliseconds. Using this */
void LoomGameController::startRumble(float intensity, Uint32 duration)
{
    // Do nothing if it is not a valid controller
    if (!this->is_connected) return;

    // Force value to be strictly between 0 and 1
    if (intensity > 1) intensity = 1.0f;
    if (intensity < 0) intensity = 0.0f;

    // If device is haptic, begin rumble
    if (this->is_haptic)
        SDL_HapticRumblePlay(this->getHaptic(), intensity, duration);
}

/** Returns true if queried button is pressed, false if queried button is not pressed. */
bool LoomGameController::getButton(int buttonID)
{
    if (buttonID > SDL_CONTROLLER_BUTTON_INVALID && buttonID < SDL_CONTROLLER_BUTTON_MAX)
        return SDL_GameControllerGetButton(this->getController(), (SDL_GameControllerButton) buttonID) == 1 ? true : false;
    return false;
}

/** Returns value of queried axis. */
int LoomGameController::getAxis(int axisID)
{
    if (axisID > SDL_CONTROLLER_AXIS_INVALID && axisID < SDL_CONTROLLER_AXIS_MAX)
        return SDL_GameControllerGetAxis(this->getController(), (SDL_GameControllerAxis) axisID);
    return 0;
}

/** Returns the SDL_Haptic object, useful for controlling rumble effects. */
SDL_Haptic *LoomGameController::getHaptic()
{
    return haptic;
}

/** Returns the SDL_GameController object. */
SDL_GameController *LoomGameController::getController()
{
    return controller;
}

/** Relays button presses to LoomSDK through delegate. */
void LoomGameController::buttonDown(SDL_Event event)
{
    _ButtonEventDelegate.pushArgument(event.cbutton.button);
    _ButtonEventDelegate.pushArgument(true);
    _ButtonEventDelegate.invoke();
}

/** Relays button releases to LoomSDK through delegate. */
void LoomGameController::buttonUp(SDL_Event event)
{
    _ButtonEventDelegate.pushArgument(event.cbutton.button);
    _ButtonEventDelegate.pushArgument(false);
    _ButtonEventDelegate.invoke();
}

/** Relays axis movement to LoomSDK through delegate. */
void LoomGameController::axisMove(SDL_Event event)
{
    _AxisMovedDelegate.pushArgument(event.caxis.axis);
    _AxisMovedDelegate.pushArgument(event.caxis.value);
    _AxisMovedDelegate.invoke();
}

/** Opens a game controller using device ID. */
void LoomGameController::open(int deviceID)
{
    // Check if device is a known controller
    if (SDL_IsGameController(deviceID))
    {
        controller = SDL_GameControllerOpen(deviceID);
    }
    else
    {
        return;
    }

    controller = SDL_GameControllerOpen(deviceID);

    // We need to open the controller as a joystick so we can check if it has haptic capabilities
    // Since a SDL_GameController is an extension of SDL_Joystick, the joystick does not need to be opened or closed
    SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
    instance_id = SDL_JoystickInstanceID(joystick);
    is_connected = true;

    if (SDL_JoystickIsHaptic(joystick))
    {
        haptic = SDL_HapticOpenFromJoystick(joystick);
        // lmLogDebug(controllerLogGroup, "Haptic Effects: %d", SDL_HapticNumEffects(haptic));
        // lmLogDebug(controllerLogGroup, "Haptic Query: %x", SDL_HapticQuery(haptic));

        // Checks if rumble is supported
        if (SDL_HapticRumbleSupported(haptic))
        {
            if (SDL_HapticRumbleInit(haptic) != 0)
            {
                // lmLogDebug(controllerLogGroup, "Haptic Rumble Init: %s", SDL_GetError());
                SDL_HapticClose(haptic);
                haptic = 0;
            }
            else
            {
                is_haptic = true;
            }
        }
        else
        {
            SDL_HapticClose(haptic);
            haptic = 0;
        }
    }
}

/** Close the game controller */
void LoomGameController::close()
{
    if (is_connected) {
        is_connected = false;
        if (haptic) {
            is_haptic = false;
            SDL_HapticClose(haptic);
            haptic = 0;
        }
        SDL_GameControllerClose(controller);
        controller = 0;
    }
}

int registerLoomGameController(lua_State *L)
{
    beginPackage(L, "loom.platform")

        .beginClass<LoomGameController>("GameController")
        .addConstructor<void(*)(void)>()

        .addStaticMethod("getGameController", &LoomGameController::getGameController)
        .addStaticMethod("numDevices", &LoomGameController::numDevices)
        .addMethod("isHaptic", &LoomGameController::isHaptic)
        .addMethod("stopRumble", &LoomGameController::stopRumble)
        .addMethod("startRumble", &LoomGameController::startRumble)
        .addMethod("getButton", &LoomGameController::getButton)
        .addMethod("getAxis", &LoomGameController::getAxis)

        .addVarAccessor("onButtonEvent", &LoomGameController::getButtonEventDelegate)
        .addVarAccessor("onAxisMoved", &LoomGameController::getAxisMovedDelegate)

        .endClass()

        .endPackage();

    return 0;
}


void installLoomGameController()
{
    LOOM_DECLARE_NATIVETYPE(LoomGameController, registerLoomGameController);
}
