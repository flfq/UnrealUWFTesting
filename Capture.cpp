#include <iostream>
#include "Capture.h"

Capture::Capture() {}

    Capture::Capture(long long start_time, unsigned char state, float start_value)
            : start_time(start_time), state(state), start_value(start_value) {
    }

Capture::Capture(long long start_time, long long end_time, float start_value, float end_value, unsigned char state)
            : start_time(start_time), end_time(end_time), start_value(start_value), end_value(end_value), state(state) {}

void Capture::end(long long end_time, float end_value) {
        this->end_time = end_time;
        this->end_value = end_value;
        std::cout << "Capture: " << this->start_time << ", " << this->end_time << ", " << this->start_value << ", "
             << this->end_value << ", " << static_cast<unsigned>(this->state) << std::endl;
    }

    long long Capture::getStart_time() const {
        return start_time;
    }

    long long Capture::getEnd_time() const {
        return end_time;
    }

    float Capture::getStart_value() const {
        return start_value;
    }

    float Capture::getEnd_value() const {
        return end_value;
    }

    unsigned char Capture::getState() const {
        return state;
    }