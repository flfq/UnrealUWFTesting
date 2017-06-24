#include <iostream>
#include <fstream>
#include <vector>
#include <winsock2.h>
#include "CircularBuffer.h.h"


#define BUFLEN 512
#define PORT 55056

#define STATE_NONE 0
#define STATE_RISING 1
#define STATE_FALLING 2
#define STATE_STABLE 3

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
using namespace std;

class Capture;

unsigned int numGestures;
vector<string> gestureNames;
vector<unsigned int> numDimensions;
vector<vector<unsigned int>> numCaptures;
vector<vector<vector<Capture>>> captures;

class Capture {
  private:
	long start_time;
	long end_time;
	long start_value;
	long end_value;
	unsigned char state;
  
  public:
	Capture(long start_time, long end_time, long start_value, long end_value, unsigned char state) : start_time(
		start_time), end_time(end_time), start_value(start_value), end_value(end_value), state(state) {}
  
  public:
	long getStart_time() const {
		return start_time;
	}
	
	long getEnd_time() const {
		return end_time;
	}
	
	long getStart_value() const {
		return start_value;
	}
	
	long getEnd_value() const {
		return end_value;
	}
	
	unsigned char getState() const {
		return state;
	}
};

class Moment {
  private:
	long value;
	long time;
  
  public:
	Moment() {}
	
	Moment(long valueIn, long timeIn) : value(valueIn), time(timeIn) {}
  
  public:
	long getValue() const {
		return value;
	}
	
	long getTime() const {
		return time;
	}
};

float getByteFromBuffer(char *buf, int offset) {
	float result;
	memcpy(&result, &buf[offset], 1);
	
	return result;
}

float getFloatFromBuffer(char *buf, int offset) {
	float tmp;
	float result;
	memcpy(&tmp, &buf[offset], 4);
	
	char *floatToConvert = (char *) &tmp;
	char *returnFloat = (char *) &result;
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
	
	char *longToConvert = (char *) &tmp;
	char *returnLong = (char *) &result;
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

int main() {
	cout << "UWF testing" << endl;
	cout << "Testing file \"test.uwf\"" << endl << endl;
	ifstream in;
	in.open("E:\\Documents\\Git\\UnrealUWFTesting\\asdf.uwf");
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
		numDimensions.push_back(_numDimensions);
		gestureNames.push_back(_gestureName);
		numCaptures.push_back(vector<unsigned int>());
		captures.push_back(vector<vector<Capture>>());
		for (int j = 0; j < numDimensions.back(); j++) {
			string dimension_text;
			unsigned int _numCaptures;
			in >> dimension_text;
			in >> _numCaptures;
			numCaptures.back().push_back(_numCaptures);
			captures.back().push_back(vector<Capture>());
			for (int k = 0; k < numCaptures.back().back(); k++) {
				string capture_text;
				long start_time;
				long end_time;
				float start_value;
				float end_value;
				unsigned int state;
				in >> capture_text >> start_time >> end_time >> start_value >> end_value >> state;
				captures.back().back().push_back(Capture(start_time, end_time, start_value, end_value, state));
			}
		}
	}
	in.close();
	
	SOCKET s;
	struct sockaddr_in server, si_other;
	int slen, recv_len;
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
	
	vector<CircularBuffer<Moment>> momentBuffer;
	int state = STATE_NONE;
	vector<Capture> captureBuffer;
	float cooldown_value = 0.0f;
	int cooldown_time = 0;
	int cooldown_num = 0;
	
	
	
	//keep listening for data
	while (1) {
		//printf("Waiting for data...");
		fflush(stdout);
		
		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', BUFLEN);
		
		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR) {
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
		char touch_state = getByteFromBuffer(buf, 48);
		long long touch_timestamp = getLongFromBuffer(buf, 49);
		
		printf("Rotation: %f, %f, %f, %lld - Acceleration: %f, %f, %f, %lld - Touch: %f, %f, %c, %lld\n",
			xrot, yrot, zrot, rot_timestamp, xacc, yacc, zacc, acc_timestamp, xtouch, ytouch, touch_state, touch_timestamp);
		
		
		
		/*if (state == STATE_NONE) {
			if (momentBuffer.size() >= 0) {
				if ()
			}
		}*/
	}
	
	closesocket(s);
	WSACleanup();
}

#pragma clang diagnostic pop