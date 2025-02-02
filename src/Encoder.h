//
// Created by ianzh on 1/28/2025.
//

#ifndef ENCODER_H
#define ENCODER_H

class Encoder {
public:
    int lastB;
    int ENC_A;
    int ENC_B;

    Encoder() {
    }

    int readDir();

    void loop();

    void setup(int ENC_A, int ENC_B);
};


#endif //ENCODER_H
