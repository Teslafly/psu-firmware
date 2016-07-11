/*
 * EEZ PSU Firmware
 * Copyright (C) 2016-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "psu.h"
#include "fan.h"

namespace eez {
namespace psu {
namespace fan {

TestResult test_result = psu::TEST_FAILED;

static int fan_speed = 0;
static int rpm = 0;

static unsigned long test_start_time;

////////////////////////////////////////////////////////////////////////////////

enum RpmMeasureState {
	RPM_MEASURE_STATE_INIT,
	RPM_MEASURE_STATE_T1,
	RPM_MEASURE_STATE_T2,
	RPM_MEASURE_STATE_FINISHED,
};

static int rpm_measure_interrupt_number;
static RpmMeasureState rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
static unsigned long rpm_measure_t1;
static unsigned long rpm_measure_t2;

void finish_rpm_measure();
void rpm_measure_interrupt_handler();

void start_rpm_measure() {
	rpm_measure_state = RPM_MEASURE_STATE_INIT;
	rpm_measure_t1 = 0;
	rpm_measure_t2 = 0;

	digitalWrite(FAN_PWM, 1);
	delay(2);
	attachInterrupt(rpm_measure_interrupt_number, rpm_measure_interrupt_handler, CHANGE);
}

void rpm_measure_interrupt_handler() {
	int x = digitalRead(FAN_SENSE);
	if (rpm_measure_state == RPM_MEASURE_STATE_INIT && x) {
		rpm_measure_state = RPM_MEASURE_STATE_T1;
	} else if (rpm_measure_state == RPM_MEASURE_STATE_T1 && !x) {
		rpm_measure_t1 = micros();
		rpm_measure_state = RPM_MEASURE_STATE_T2;
	} else if (rpm_measure_state == RPM_MEASURE_STATE_T2 && x) {
		rpm_measure_t2 = micros();
		rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
		finish_rpm_measure();
	}
}

void finish_rpm_measure() {
	detachInterrupt(rpm_measure_interrupt_number);
	analogWrite(FAN_PWM, fan_speed);

	if (rpm_measure_state == RPM_MEASURE_STATE_FINISHED) {
		unsigned long dt = rpm_measure_t2 - rpm_measure_t1;
		dt *= 2; // duty cycle is 50%
		dt *= 2; // 2 impulse per revolution
		rpm = (int)(60L * 1000 * 1000 / dt);
	} else {
		rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
		rpm = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool init() {
	rpm_measure_interrupt_number = digitalPinToInterrupt(FAN_SENSE);

	return test();
}

void test_start() {
	if (OPTION_FAN) {
		digitalWrite(FAN_PWM, 1);
		test_start_time = millis();
	}
}

bool test() {
	if (OPTION_FAN) {
		unsigned long time_since_test_start = millis() - test_start_time;
		if (time_since_test_start < 250) {
			delay(300 - time_since_test_start);
		}

		start_rpm_measure();

		for (int i = 0; i < 25 && rpm_measure_state != RPM_MEASURE_STATE_FINISHED; ++i) {
			delay(1);
		}

		if (rpm_measure_state != RPM_MEASURE_STATE_FINISHED) {
			finish_rpm_measure();
			test_result = psu::TEST_FAILED;
		} else {
			test_result = psu::TEST_OK;
			DebugTraceF("Fan RPM: %d", rpm);
		}
	} else {
		test_result = psu::TEST_SKIPPED;
	}

	if (test_result == psu::TEST_FAILED) {
		psu::generateError(SCPI_ERROR_FAN_TEST_FAILED);
	}

	return test_result != psu::TEST_FAILED;
}

void tick(unsigned long tick_usec) {
}

}
}
} // namespace eez::psu::fan