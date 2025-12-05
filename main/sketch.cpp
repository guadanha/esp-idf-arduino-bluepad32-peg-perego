// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"

#include <Arduino.h>
#include <Bluepad32.h>
#include <math.h>
#include "ble_server.h"

#include "bts7960.h"

//
// README FIRST, README FIRST, README FIRST
//
// Bluepad32 has a built-in interactive console.
// By default, it is enabled (hey, this is a great feature!).
// But it is incompatible with Arduino "Serial" class.
//
// Instead of using "Serial" you can use Bluepad32 "Console" class instead.
// It is somewhat similar to Serial but not exactly the same.
//
// Should you want to still use "Serial", you have to disable the Bluepad32's console
// from "sdkconfig.defaults" with:
//    CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Console.printf("CALLBACK: Controller is connected, index=%d\n", i);
            // Additionally, you can get certain gamepad properties like:
            // Model, VID, PID, BTAddr, flags, etc.
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Console.println("CALLBACK: Controller connected, but could not found empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Console.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            motor_control(M1_RPWM_CHANNEL, M1_LPWM_CHANNEL, M1_EN_PIN, 2, 0);
            motor_control(M2_RPWM_CHANNEL, M2_LPWM_CHANNEL, M2_EN_PIN, 2, 0);
        }
    }

    if (!foundController) {
        Console.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void dumpGamepad(ControllerPtr ctl) {
    Console.printf(
        "idx=%d, dpad: 0x%02x, buttons: 0x%04x, axis L: %4d, %4d, axis R: %4d, %4d, brake: %4d, throttle: %4d, "
        "misc: 0x%02x, gyro x:%6d y:%6d z:%6d, accel x:%6d y:%6d z:%6d\n",
        ctl->index(),        // Controller Index
        ctl->dpad(),         // D-pad
        ctl->buttons(),      // bitmask of pressed buttons
        ctl->axisX(),        // (-511 - 512) left X Axis
        ctl->axisY(),        // (-511 - 512) left Y axis
        ctl->axisRX(),       // (-511 - 512) right X axis
        ctl->axisRY(),       // (-511 - 512) right Y axis
        ctl->brake(),        // (0 - 1023): brake button
        ctl->throttle(),     // (0 - 1023): throttle (AKA gas) button
        ctl->miscButtons(),  // bitmask of pressed "misc" buttons
        ctl->gyroX(),        // Gyro X
        ctl->gyroY(),        // Gyro Y
        ctl->gyroZ(),        // Gyro Z
        ctl->accelX(),       // Accelerometer X
        ctl->accelY(),       // Accelerometer Y
        ctl->accelZ()        // Accelerometer Z
    );
}

/**
 * @brief Mapeia um valor de um intervalo para outro de forma linear.
 *
 * @param value O valor a ser mapeado.
 * @param in_min O limite inferior da faixa de entrada.
 * @param in_max O limite superior da faixa de entrada.
 * @param out_min O limite inferior da faixa de saída.
 * @param out_max O limite superior da faixa de saída.
 * @return O valor mapeado na faixa de saída.
 */
int map_range(int value, int in_min, int in_max, int out_min, int out_max) {
    if (value < in_min) {
        return 0;
    } else if (value > (in_max - 20)) {
        value = in_max;
    }
    // Evita divisão por zero.
    if (in_min == in_max) {
        return out_min; // Ou outro valor de erro/limite
    }

    // Fórmula de mapeamento linear:
    // (Valor - Mínimo Entrada) * (Máximo Saída - Mínimo Saída) / (Máximo Entrada - Mínimo Entrada) + Mínimo Saída
    
    // Usamos (long) para evitar overflow em cálculos intermediários 
    // antes da divisão, garantindo precisão mesmo com 'int'.
    return (int)((long)(value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

void processGamepad(ControllerPtr ctl) {
    static int marcha = 0;
    if (ctl->x()) {
        marcha = 1;
        ctl->playDualRumble(0, 250, 0x80, 0x40);
    } else if (ctl->b()) {
        marcha = 0;
        ctl->playDualRumble(0, 250, 0x80, 0x40);
    }

    static int state = 0;
    if (ctl->brake()) { // (0 - 1023): brake button
        if (state != 1) {
            marcha = 0;
            motor_control(M1_RPWM_CHANNEL, M1_LPWM_CHANNEL, M1_EN_PIN, 0, 0);
            motor_control(M2_RPWM_CHANNEL, M2_LPWM_CHANNEL, M2_EN_PIN, 0, 0);
            // orange
            ctl->setColorLED(255, 165, 0);
            state = 1;
        }
    } else if ((ctl->a() && ctl->throttle()) || (ctl->axisRY() > 15)) {
        marcha = 0;
        int duty_t = map_range(ctl->throttle(), 15, 1023, 250, 800);
        int duty_ry = map_range(ctl->axisRY(), 15, 512, 250, 800);
        int duty = duty_t >= duty_ry ? duty_t : duty_ry;
        int  duty_l = duty;
        int duty_r = duty;

        if (abs(ctl->axisX()) > 15) {
            int dir = map_range(abs(ctl->axisX()), 15, 512, 0, duty);
            Console.printf("dir %d \n", dir);
            if (ctl->axisX() > 0) {
                duty_r = duty_r - dir;
            } else {
                duty_l = duty_l - dir;
            }
        }

        static int old_duty_l = 0;
        static int old_duty_r = 0;
        if (duty_l != old_duty_l || duty_r != old_duty_r) {
            motor_control(M1_RPWM_CHANNEL, M1_LPWM_CHANNEL, M1_EN_PIN, -1, duty_l);
            motor_control(M2_RPWM_CHANNEL, M2_LPWM_CHANNEL, M2_EN_PIN, -1, duty_r);
            Console.printf("RÉ duty: %d, throttle: %d, axixRY %d  duty l %d duty r %d\n", duty, ctl->throttle(), ctl->axisRY(), duty_l, duty_r);
            old_duty_l = duty_l;
            old_duty_r = duty_r;
        }
        if (state != 2) {
            // yelow
            ctl->setColorLED(255, 255, 0);
            state = 2;
        }

    } else if (ctl->throttle() || (ctl->axisRY() < -15)) {
        int out_max = 1023;

        if (marcha == 0) {
            out_max = 600;
        } 

        int duty_t = map_range(ctl->throttle(), 15, 1023, 250, out_max);
        int duty_ry = map_range(abs(ctl->axisRY()), 15, 512, 250, out_max);
        int duty = duty_t >= duty_ry ? duty_t : duty_ry;
        int duty_r = duty;
        int duty_l = duty;

        if (abs(ctl->axisX()) > 15) {
            int dir = map_range(abs(ctl->axisX()), 15, 512, 0, duty);
            Console.printf("dir %d \n", dir);

            if (ctl->axisX() > 0) {
                duty_r = duty_r - dir;
            } else {
                duty_l = duty_l - dir;
            }
        }

        static int old_duty_l = 0;
        static int old_duty_r = 0;
        if (duty_l != old_duty_l || duty_r != old_duty_r) {
            motor_control(M1_RPWM_CHANNEL, M1_LPWM_CHANNEL, M1_EN_PIN, 1, duty_l);
            motor_control(M2_RPWM_CHANNEL, M2_LPWM_CHANNEL, M2_EN_PIN, 1, duty_r);
            Console.printf("Frente duty: %d marcha %d throttle: %d, axixRY %d axixX %d duty_r %d duty_l %d\n", duty, marcha, ctl->throttle(), ctl->axisRY(), ctl->axisX(), duty_r, duty_l);
            old_duty_l = duty_l;
            old_duty_r = duty_r;
        }
        static int old_marcha = 0;
        if (state != 3 || old_marcha != marcha) {
            // Light Green
            if (marcha == 0) {
                ctl->setColorLED(0, 25, 25);
            } else {
                ctl->setColorLED(0, 255, 0);
            }
            state = 3;
            old_marcha = marcha;
        }
    } else { // Motor disable
        if (state != 4) {
            motor_control(M1_RPWM_CHANNEL, M1_LPWM_CHANNEL, M1_EN_PIN, 2, 0);
            motor_control(M2_RPWM_CHANNEL, M2_LPWM_CHANNEL, M2_EN_PIN, 2, 0);
            // white
            ctl->setColorLED(255, 255, 255);
            state = 4;
        }
    }
    // Another way to query controller data is by getting the buttons() function.
    // See how the different "dump*" functions dump the Controller info.
    //dumpGamepad(ctl);
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else {
                Console.printf("Unsupported controller\n");
            }
        }
    }
}

// Arduino setup function. Runs in CPU 1
void setup() {
    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // Setup the Bluepad32 callbacks, and the default behavior for scanning or not.
    // By default, if the "startScanning" parameter is not passed, it will do the "start scanning".
    // Notice that "Start scanning" will try to auto-connect to devices that are compatible with Bluepad32.
    // E.g: if a Gamepad, keyboard or mouse are detected, it will try to auto connect to them.
    bool startScanning = true;
    BP32.setup(&onConnectedController, &onDisconnectedController, startScanning);

    // Notice that scanning can be stopped / started at any time by calling:
    // BP32.enableNewBluetoothConnections(enabled);

    // "forgetBluetoothKeys()" should be called when the user performs
    // a "device factory reset", or similar.
    // Calling "forgetBluetoothKeys" in setup() just as an example.
    // Forgetting Bluetooth keys prevents "paired" gamepads to reconnect.
    // But it might also fix some connection / re-connection issues.
    BP32.forgetBluetoothKeys();

    // Enables mouse / touchpad support for gamepads that support them.
    // When enabled, controllers like DualSense and DualShock4 generate two connected devices:
    // - First one: the gamepad
    // - Second one, which is a "virtual device", is a mouse.
    // By default, it is disabled.
    BP32.enableVirtualDevice(false);

    // Enables the BLE Service in Bluepad32.
    // This service allows clients, like a mobile app, to setup and see the state of Bluepad32.
    // By default, it is disabled.
    BP32.enableBLEService(false);

    BP32.enableNewBluetoothConnections(false);

    BLE_SERVER_SETUP();

    Console.begin(115200);

    ledc_init();
}

// Arduino loop function. Runs in CPU 1.
void loop() {
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();

    // The main loop must have some kind of "yield to lower priority task" event.
    // Otherwise, the watchdog will get triggered.
    // If your main loop doesn't have one, just add a simple `vTaskDelay(1)`.
    // Detailed info here:
    // https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time

    //     vTaskDelay(1);
    delay(100);
}
