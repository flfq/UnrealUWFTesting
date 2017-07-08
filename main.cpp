#pragma comment(lib, "Ws2_32.lib")
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <winsock2.h>
#include "CircularBuffer.h.h"

#include "Moment.h"
#include "Capture.h"


#define BUFLEN 512
#define PORT 55056

#define STATE_RISING '0'
#define STATE_FALLING '1'
#define STATE_STABLE '2'

#define LOG_INCOMING 0

#define MOMENT_BUFFER_SIZE 100

using namespace std;

static const int FROM_RISING_TO_FALLING[] = { -7000, -1000 };
static const int FROM_RISING_TO_STABLE[] = { -4000, -500 };
static const int FROM_FALLING_TO_RISING[] = { 7000, 2000 };
static const int FROM_FALLING_TO_STABLE[] = { 4000, 500 };
static const int FROM_STABLE_TO_RISING[] = { 4000, 500 };
static const int FROM_STABLE_TO_FALLING[] = { -4000, -500 };

static const int FALLING_THRESHOLD = -2;
static const int RISING_THRESHOLD = 2;

// TODO: do we actually need this?
static const double CAPTURES_MATCH_GESTURE_THRESHOLD = 0.2;

class Capture;

class Moment;

unsigned int numGestures;
vector<string> preGestureNames;
vector<vector<unsigned int>> numPreCaptures;
vector<vector<vector<Capture>>> preCaptures;

// for each dimension for each time a moment
vector<CircularBuffer<Moment *> *> momentBuffer;
vector<int> state;
// for each dimension for each time a capture
vector<vector<Capture*>*> captureBuffer;
float cooldown_value = 0.0f;
int cooldown_time = 0;
int cooldown_num = 0;

bool isFirstSample = true;

unsigned char getByteFromBuffer(char *buf, int offset) {
	unsigned char result;
	memcpy(&result, &buf[offset], 1);

	return result;
}

float getFloatFromBuffer(char *buf, int offset) {
	float tmp;
	float result;
	memcpy(&tmp, &buf[offset], 4);

	char *floatToConvert = (char *)&tmp;
	char *returnFloat = (char *)&result;
	returnFloat[0] = floatToConvert[3];
	returnFloat[1] = floatToConvert[2];
	returnFloat[2] = floatToConvert[1];
	returnFloat[3] = floatToConvert[0];

	return result;
}

long long getLongFromBuffer(char *buf, int offset) {
	long long tmp;
	long long result;
	memcpy(&tmp, &buf[offset], 8);

	char *longToConvert = (char *)&tmp;
	char *returnLong = (char *)&result;
	returnLong[0] = longToConvert[7];
	returnLong[1] = longToConvert[6];
	returnLong[2] = longToConvert[5];
	returnLong[3] = longToConvert[4];
	returnLong[4] = longToConvert[3];
	returnLong[5] = longToConvert[2];
	returnLong[6] = longToConvert[1];
	returnLong[7] = longToConvert[0];

	return result;
}

void checkCaptureBuffer() {
	for (int i = 0; i < numGestures; i++) {
		for (int j = 1; j < 2; j++) {
			for (int captureOffset = 0; captureOffset <= (int) captureBuffer[j]->size() - (int) numPreCaptures[i][j]; captureOffset++) {

				vector<vector<int>> dtw(numPreCaptures[i][j] + 1, vector<int>(captureBuffer[j]->size() - captureOffset + 1));
				for (int k = 1; k < numPreCaptures[i][j]; k++) {
					dtw[k][0] = 999999;
				}
				for (int k = 1; k < captureBuffer[j]->size() - captureOffset; k++) {
					dtw[0][k] = 999999;
				}
				dtw[0][0] = 0;

				for (int k = 0; k < numPreCaptures[i][j]; k++) {
					for (int l = 0; l < captureBuffer[j]->size() - captureOffset; l++) {
						int cost = -1;
						if (preCaptures[i][j][k].getState() == (*(*captureBuffer[j])[l + captureOffset]).getState()) {
							cost = 0;
						}
						else if (preCaptures[i][j][k].getState() == STATE_FALLING) {
							if ((*(*captureBuffer[j])[l + captureOffset]).getState() == STATE_STABLE) {
								cost = 2;
							}
							else if ((*(*captureBuffer[j])[l + captureOffset]).getState() == STATE_RISING) {
								cost = 5;
							}
						}
						else if (preCaptures[i][j][k].getState() == STATE_STABLE) {
							if ((*(*captureBuffer[j])[l + captureOffset]).getState() == STATE_FALLING) {
								cost = 2;
							}
							else if ((*(*captureBuffer[j])[l + captureOffset]).getState() == STATE_RISING) {
								cost = 2;
							}
						}
						else if (preCaptures[i][j][k].getState() == STATE_RISING) {
							if ((*(*captureBuffer[j])[l + captureOffset]).getState() == STATE_FALLING) {
								cost = 5;
							}
							else if ((*(*captureBuffer[j])[l + captureOffset]).getState() == STATE_STABLE) {
								cost = 2;
							}
						}
						dtw[k + 1][l + 1] = cost + min(dtw[k][l + 1], min(dtw[k + 1][l], dtw[k][l]));
					}
				}
				if (dtw[numPreCaptures[i][j] - 1][captureBuffer[j]->size() - captureOffset - 1] / (captureBuffer[j]->size() - captureOffset) < CAPTURES_MATCH_GESTURE_THRESHOLD) {
					if (true) {
						cout << "Gesture matched!!!" << endl;
						for (int i = 0; i < 6; i++) {
							captureBuffer[i]->clear();
						}
					}
				}
			}
		}
	}
}

void goToRising(int i, Moment *moment) {
	cout << "State is RISING" << endl;
	if (captureBuffer[i]->size() > 0) {
		captureBuffer[i]->back()->end(momentBuffer[i]->getBack()->getTime(),
			momentBuffer[i]->getElem(momentBuffer[i]->getNumValuesInBuffer()
				- cooldown_num)->getValue());
	}
	captureBuffer[i]->push_back(new Capture(moment->getTime(),
		STATE_RISING,
		moment->getValue()));
	state[i] = STATE_RISING;
	cooldown_value = 0;
	cooldown_time = 0;
	cooldown_num = 0;
	checkCaptureBuffer();
}

void goToStable(int i, Moment *moment) {
	cout << "State is STABLE" << endl;
	if (captureBuffer[i]->size() > 0) {
		captureBuffer[i]->back()->end(momentBuffer[i]->getBack()->getTime(),
			(*(momentBuffer[i]))[momentBuffer[i]->getNumValuesInBuffer()
			- cooldown_num]->getValue());
	}
	captureBuffer[i]->push_back(new Capture(moment->getTime(),
		STATE_STABLE,
		moment->getValue()));
	state[i] = STATE_STABLE;
	cooldown_value = 0;
	cooldown_time = 0;
	cooldown_num = 0;
	checkCaptureBuffer();
}

void goToFalling(int i, Moment *moment) {
	cout << "State is FALLING" << endl;
	if (captureBuffer[i]->size() > 0) {
		captureBuffer[i]->back()->end(momentBuffer[i]->getBack()->getTime(),
			(*(momentBuffer[i]))[momentBuffer[i]->getNumValuesInBuffer()
			- cooldown_num]->getValue());
	}
	captureBuffer[i]->push_back(new Capture(moment->getTime(),
		STATE_FALLING,
		moment->getValue()));
	state[i] = STATE_FALLING;
	cooldown_value = 0;
	cooldown_time = 0;
	cooldown_num = 0;
	checkCaptureBuffer();
}

int main() {
	cout << "UWF testing" << endl;
	cout << "Testing file \"test.uwf\"" << endl << endl;
	ifstream in;
	in.open("C:\\Users\\jverb\\Documents\\Git\\UnrealUWFTesting\\test.uwf");
	if (!in.is_open()) {
		return 0;
	}
	in >> numGestures;
	for (int i = 0; i < numGestures; i++) {
		string gesture_text;
		unsigned int _numDimensions;
		string _gestureName;
		in >> gesture_text;
		in >> _numDimensions;
		in >> _gestureName;
		//numDimensions.push_back(_numDimensions);
		preGestureNames.push_back(_gestureName);
		numPreCaptures.push_back(vector<unsigned int>());
		preCaptures.push_back(vector<vector<Capture>>());
		for (int j = 0; j < 6; j++) {
			string dimension_text;
			unsigned int _numCaptures;
			in >> dimension_text;
			in >> _numCaptures;
			numPreCaptures.back().push_back(_numCaptures);
			preCaptures.back().push_back(vector<Capture>());
			for (int k = 0; k < numPreCaptures.back().back(); k++) {
				string capture_text;
				long start_time;
				long end_time;
				float start_value;
				float end_value;
				unsigned char state;
				in >> capture_text >> start_time >> end_time >> start_value >> end_value >> state;
				preCaptures.back().back().push_back(Capture(start_time, end_time, start_value, end_value, state));
			}
		}
	}
	in.close();

	SOCKET s;
	struct sockaddr_in server, si_other;
	int slen;
	char buf[BUFLEN];
	WSADATA wsa;

	slen = sizeof(si_other);

	//Initialise winsock
	printf("Initialising Winsock...\n");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//Create a socket
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		printf("Could not create socket : %d", WSAGetLastError());
	}
	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	//Bind
	if (bind(s, (struct sockaddr *) &server, sizeof(server)) == SOCKET_ERROR) {
		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	puts("Bind done");





	// for each dimension for each time a moment
	for (int i = 0; i < 6; i++) {
		momentBuffer.push_back(new CircularBuffer<Moment *>(MOMENT_BUFFER_SIZE));
	}
	for (int i = 0; i < 6; i++) {
		state.push_back(STATE_STABLE);
	}
	// for each dimension for each time a capture
	for (int i = 0; i < 6; i++) {
		captureBuffer.push_back(new vector<Capture*>());
	}



	//keep listening for data
	while (1) {
		//printf("Waiting for data...");
		fflush(stdout);

		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', BUFLEN);

		//try to receive some data, this is a blocking call
		if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == SOCKET_ERROR) {
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}

		//print details of the client/peer and the data received
		//printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
		//printf("Data: %s\n", buf);
		float xrot = getFloatFromBuffer(buf, 0);
		float yrot = getFloatFromBuffer(buf, 4);
		float zrot = getFloatFromBuffer(buf, 8);
		long long rot_timestamp = getLongFromBuffer(buf, 12);

		float xacc = getFloatFromBuffer(buf, 20);
		float yacc = getFloatFromBuffer(buf, 24);
		float zacc = getFloatFromBuffer(buf, 28);
		long long acc_timestamp = getLongFromBuffer(buf, 32);

		float xtouch = getFloatFromBuffer(buf, 40);
		float ytouch = getFloatFromBuffer(buf, 44);
		unsigned char touch_state = getByteFromBuffer(buf, 48);
		long long touch_timestamp = getLongFromBuffer(buf, 49);

		Moment *moment[6];
		moment[0] = new Moment(xrot, rot_timestamp);
		moment[1] = new Moment(yrot, rot_timestamp);
		moment[2] = new Moment(zrot, rot_timestamp);
		moment[3] = new Moment(xacc, rot_timestamp);
		moment[4] = new Moment(yacc, rot_timestamp);
		moment[5] = new Moment(zacc, rot_timestamp);

#if LOG_INCOMING != 0
		printf("Rotation: %f, %f, %f, %lld - Acceleration: %f, %f, %f, %lld - Touch: %f, %f, %c, %lld\n",
			xrot,
			yrot,
			zrot,
			rot_timestamp,
			xacc,
			yacc,
			zacc,
			acc_timestamp,
			xtouch,
			ytouch,
			touch_state,
			touch_timestamp);
#endif

		for (int i = 1; i < 2; i++) {
			const unsigned int type = i < 3 ? 0u : 1u;
			if (state[i] == STATE_RISING) {
				const float diff = moment[i]->getValue() - momentBuffer[i]->getBack()->getValue();
				if (diff < RISING_THRESHOLD) {
					cooldown_value += diff - RISING_THRESHOLD;
					cooldown_time += moment[i]->getTime() - momentBuffer[i]->getBack()->getTime();
					cooldown_num++;
					if (cooldown_value * cooldown_time < FROM_RISING_TO_FALLING[type]) {
						goToFalling(i, moment[i]);
					}
					else if (cooldown_value * cooldown_time < FROM_RISING_TO_STABLE[type]) {
						goToStable(i, moment[i]);
					}
				}
				else {
					cooldown_value /= 3;
				}
			}
			else if (state[i] == STATE_FALLING) {
				const float diff = moment[i]->getValue() - momentBuffer[i]->getBack()->getValue();
				if (diff >= FALLING_THRESHOLD) {
					cooldown_value += diff - FALLING_THRESHOLD;
					cooldown_time += moment[i]->getTime() - momentBuffer[i]->getBack()->getTime();
					cooldown_num++;
					if (cooldown_value * cooldown_time >= FROM_FALLING_TO_RISING[type]) {
						goToRising(i, moment[i]);
					}
					else if (cooldown_value * cooldown_time >= FROM_FALLING_TO_STABLE[type]) {
						goToStable(i, moment[i]);
					}
				}
				else {
					cooldown_value /= 3;
				}
			}
			else if (state[i] == STATE_STABLE) {
				if (isFirstSample) {
					isFirstSample = false;
				}
				else {
					const float diff = moment[i]->getValue() - momentBuffer[i]->getBack()->getValue();
					if (diff < FALLING_THRESHOLD || diff > RISING_THRESHOLD) {
						cooldown_value += diff;
						cooldown_time += moment[i]->getTime() - momentBuffer[i]->getBack()->getTime();
						cooldown_num++;
						if (cooldown_value * cooldown_time >= FROM_STABLE_TO_RISING[type]) {
							goToRising(i, moment[i]);
						}
						else if (cooldown_value * cooldown_time < FROM_STABLE_TO_FALLING[type]) {
							goToFalling(i, moment[i]);
						}
					}
					else {
						cooldown_value /= 3;
					}
				}
			}
			momentBuffer[i]->push_back(moment[i]);
		}
	}
}

#pragma clang diagnostic pop