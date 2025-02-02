//
// Created by ianzh on 1/28/2025.
//

#include "Encoder.h"
#include <Arduino.h>

void Encoder::setup(int ENC_A, int ENC_B) {
    this->ENC_A = ENC_A;
    this->ENC_B = ENC_B;
    pinMode(ENC_A, INPUT);
    pinMode(ENC_B, INPUT);
    lastB = digitalRead(ENC_B);
}

/*
Welcome to my mega scuffed encoder solution

CW:
A: 1 B: 1
A: 0 B: 1
A: 0 B: 0
A: 1 B: 0

CCW:
A: 1 B: 0
A: 0 B: 0
A: 0 B: 1
A: 1 B: 1

This solution looks at A = 0 and B's transition to determine direction.
*/

int Encoder::readDir() {
    int stateA = digitalRead(ENC_A);
    int stateB = digitalRead(ENC_B);

    if (stateB == HIGH && lastB == LOW) { // Other inputs are malformed
        if (stateA == LOW) {
            return -1;
        }

        if (stateA == HIGH) {
            return 1;
        }
    }

    lastB = stateB;
    return 0;
}
