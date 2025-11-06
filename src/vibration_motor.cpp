#include "vibration_motor.h"

/**
 * VibrationMotor Implementation
 * 
 * Non-blocking vibration motor control using ESP32 PWM (LEDC)
 */

VibrationMotor::VibrationMotor(uint8_t pin, uint8_t channel, uint32_t frequency, uint8_t resolution)
    : pwmPin(pin),
      pwmChannel(channel),
      pwmFrequency(frequency),
      pwmResolution(resolution),
      isVibrating(false),
      currentIntensity(0),
      vibrationStartTime(0),
      vibrationDuration(0),
      timedVibration(false) {
}

bool VibrationMotor::begin() {
    // Configure PWM channel
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    
    // Attach channel to pin
    ledcAttachPin(pwmPin, pwmChannel);
    
    // Start with motor off
    ledcWrite(pwmChannel, 0);
    
    return true;
}

uint32_t VibrationMotor::intensityToDutyCycle(uint8_t intensity) {
    // Clamp intensity to 0-100
    if (intensity > 100) {
        intensity = 100;
    }
    
    // Calculate maximum duty cycle value
    uint32_t maxDutyCycle = (1 << pwmResolution) - 1;
    
    // Convert percentage to duty cycle
    return (maxDutyCycle * intensity) / 100;
}

void VibrationMotor::startContinuous(uint8_t intensity) {
    currentIntensity = intensity;
    timedVibration = false;
    
    if (intensity > 0) {
        isVibrating = true;
        uint32_t dutyCycle = intensityToDutyCycle(intensity);
        ledcWrite(pwmChannel, dutyCycle);
    } else {
        stop();
    }
}

void VibrationMotor::startTimed(uint8_t intensity, unsigned long durationMs) {
    currentIntensity = intensity;
    vibrationDuration = durationMs;
    vibrationStartTime = millis();
    timedVibration = true;
    
    if (intensity > 0 && durationMs > 0) {
        isVibrating = true;
        uint32_t dutyCycle = intensityToDutyCycle(intensity);
        ledcWrite(pwmChannel, dutyCycle);
    } else {
        stop();
    }
}

void VibrationMotor::stop() {
    isVibrating = false;
    timedVibration = false;
    currentIntensity = 0;
    ledcWrite(pwmChannel, 0);
}

void VibrationMotor::updateState() {
    // Check if timed vibration has completed
    if (isVibrating && timedVibration) {
        unsigned long elapsed = millis() - vibrationStartTime;
        if (elapsed >= vibrationDuration) {
            stop();
        }
    }
}

void VibrationMotor::setIntensity(uint8_t intensity) {
    currentIntensity = intensity;
    
    if (isVibrating) {
        uint32_t dutyCycle = intensityToDutyCycle(intensity);
        ledcWrite(pwmChannel, dutyCycle);
    }
}

bool VibrationMotor::getIsVibrating() const {
    return isVibrating;
}

uint8_t VibrationMotor::getIntensity() const {
    return currentIntensity;
}

unsigned long VibrationMotor::getRemainingTime() const {
    if (!isVibrating || !timedVibration) {
        return 0;
    }
    
    unsigned long elapsed = millis() - vibrationStartTime;
    if (elapsed >= vibrationDuration) {
        return 0;
    }
    
    return vibrationDuration - elapsed;
}

void VibrationMotor::startPulsePattern(uint8_t intensity, unsigned long onTimeMs, unsigned long offTimeMs, uint16_t cycles) {
    // This is a simplified implementation that starts continuous vibration
    // For full pulse pattern support, implement a more complex state machine
    // in updateState() with pattern tracking
    startContinuous(intensity);
}

String VibrationMotor::getStatus() const {
    String status = "Vibration Motor Status:\n";
    status += "  State: " + String(isVibrating ? "VIBRATING" : "STOPPED") + "\n";
    status += "  Intensity: " + String(currentIntensity) + "%\n";
    status += "  Mode: " + String(timedVibration ? "TIMED" : "CONTINUOUS") + "\n";
    
    if (timedVibration && isVibrating) {
        unsigned long remaining = getRemainingTime();
        status += "  Remaining: " + String(remaining) + "ms\n";
    }
    
    return status;
}
