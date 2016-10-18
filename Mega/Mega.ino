#include <StandardCplusplus.h>
#include <system_configuration.h>
#include <unwind-cxx.h>
#include <utility.h>

#include <StaticThreadController.h>
#include <Thread.h>
#include <ThreadController.h>

#include <string>
#include <sstream>
#include <vector>

using namespace std;

/** UTILITIES **/
#define ROOMS_AMOUNT 9
#define COLOR_RED_MASK 0xFF
#define COLOR_GREEN_MASK 0xFF00
#define COLOR_BLUE_MASK 0xFF0000

#define COLOR_RED_SHIFT 0
#define COLOR_GREEN_SHIFT 8
#define COLOR_BLUE_SHIFT 16

/** THREADS **/
Thread serialThread = Thread();
Thread modelThread = Thread();

bool IS_SERIAL_BLOCKED = false;

#define THREAD_SERIAL_INTERVAL 10
#define THREAD_MODE_INTERVAL 10
#define THREAD_LIGHTNING_INTERVAL 10

/** LIGHTNING **/
vector<bool> ROOMS_LIGHTNING_ENABLED(ROOMS_AMOUNT, false);
vector<vector<int>> ROOMS_LIGHTNING_COLORS (ROOMS_AMOUNT, vector<int>(3, 255));
vector<vector<int>> ROOMS_LIGHTNING_PINS(ROOMS_AMOUNT, vector<int>(3, 0));

/** MODES **/
vector<int> ROOMS_MODES(ROOMS_AMOUNT, 0);
vector<long> ROOMS_MODES_ENDINGS(ROOMS_AMOUNT, 0xffffffff);

bool IS_GUARD_MODE_ON = false;
bool IS_EMERGENCY_MODE_ON = false;

/**===================== UTILITIES =====================**/
void split(const string &s, char delim, vector<int> &elems) {
  char* cstr = new char [s.length() + 1];
  split(strcpy(cstr, s.c_str()), delim, elems);
}

void split(char* s, char delim, vector<int> &elems) {
  char* p = strtok(s, " ");

  while (p != 0) {
    elems.push_back(atoi(p));
    p = strtok(NULL, " ");
  }
}

vector<int> split(const string &s, char delim) {
  vector<int> elems;
  split(s, delim, elems);
  return elems;
}

vector<int> decodeColor(int color) {
  vector<int> colors;

  colors.push_back((color & COLOR_RED_MASK) >> COLOR_RED_SHIFT);
  colors.push_back((color & COLOR_GREEN_MASK) >> COLOR_GREEN_SHIFT);
  colors.push_back((color & COLOR_BLUE_MASK) >> COLOR_BLUE_SHIFT);

  return colors;
}

int encodeColor(vector<int> data) {
  return ((data[2] << COLOR_BLUE_SHIFT) & COLOR_BLUE_MASK) + ((data[1] << COLOR_GREEN_SHIFT) & COLOR_GREEN_MASK) + ((data[0] << COLOR_RED_SHIFT) & COLOR_RED_MASK);
}

/**===================== ARDUINO COMMUNICATION PROCOTOL =====================**/
/** [1] int slot, int value **/
void set(vector<int> data) {
  digitalWrite(data[0], data[1]);
}

/** [2] int slot **/
/** Returns value of the slot (int value) **/
int get(vector<int> data) {
  return digitalRead(data[0]);
}

/** [3] int name, int value **/
vector<int> feature(vector<int> data) {
  for (vector<int>::iterator it = data.begin(); it != data.end(); ++it) {
    Serial1.print(*it);

    if (it != data.end() - 1) {
      Serial1.print(' ');
    } else {
      Serial1.print('\n');
    }
  }
}

/** [4] int name **/
/** Returns value of the feature (int value) **/
int getFeature(vector<int> data) {
  Serial1.print(data[0]);
  Serial1.print('\n');

  IS_SERIAL_BLOCKED = true;

  delay(50);

  Serial1.readStringUntil('\n');
  Serial1.read();

  IS_SERIAL_BLOCKED = false;
}

/** [5] int room, int value **/
void lightning(vector<int> data) {
  if (data[1] <= 0 && ROOMS_LIGHTNING_ENABLED[data[0]]) {
    Serial.println("data[1] <= 0 && ROOMS_LIGHTNING_ENABLED[data[0]]");
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][0], 0);
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][1], 0);
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][2], 0);

    ROOMS_LIGHTNING_ENABLED[data[0]] = false;
  } else if (data[1] >= 1 && !ROOMS_LIGHTNING_ENABLED[data[0]]) {
    Serial.println("data[1] >= 1 && !ROOMS_LIGHTNING_ENABLED[data[0]]");
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][0], ROOMS_LIGHTNING_COLORS[data[0]][0]);
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][1], ROOMS_LIGHTNING_COLORS[data[0]][1]);
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][2], ROOMS_LIGHTNING_COLORS[data[0]][2]);

    ROOMS_LIGHTNING_ENABLED[data[0]] = true;
  }
}

/** [6] int room **/
/** Returns value of the lightning (enabled/disabled) (int value) **/
int getLightning(vector<int> data) {
  return ROOMS_LIGHTNING_ENABLED[data[0]] ? 0 : 1;
}

/** [7] int room, int color, [int toSave] **/
void color(vector<int> &data) {
  vector<int> colors = decodeColor(data[1]);

  if (data.size() < 3 || data[2] == 1) {
    ROOMS_LIGHTNING_COLORS[data[0]][0] = colors[0];
    ROOMS_LIGHTNING_COLORS[data[0]][1] = colors[1];
    ROOMS_LIGHTNING_COLORS[data[0]][2] = colors[2];
  }

  if (ROOMS_LIGHTNING_ENABLED[data[0]]) {
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][0], ROOMS_LIGHTNING_COLORS[data[0]][0]);
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][1], ROOMS_LIGHTNING_COLORS[data[0]][1]);
    analogWrite(ROOMS_LIGHTNING_PINS[data[0]][2], ROOMS_LIGHTNING_COLORS[data[0]][2]);
  }
}

/** [8] int room **/
/** Returns three colors of lightning (int red, int green, int blue) **/
int getColor(vector<int> data) {
  return encodeColor(ROOMS_LIGHTNING_COLORS[data[0]]);
}

/** [9] int room, int mode, int value [int duration] **/
void mode(vector<int> data) {
  if (data[1] == 4) {
    IS_GUARD_MODE_ON = data[2] == 1 ? true : false;
  } else if (data[1] == 5) {
    IS_EMERGENCY_MODE_ON = data[2] == 1 ? true : false;
  } else {
    ROOMS_MODES[data[0]] = data[2] == 1 ? data[1] : 1;
    ROOMS_MODES_ENDINGS[data[0]] = data.size() < 4 || data[3] == 0 ? 0xffffffff : millis() + data[3];
  }
}

/** [10] int room **/
/** Returns value of the mode in room (int value)**/
int getMode(vector<int> data) {
  if (IS_GUARD_MODE_ON) {
    return 4;
  } else if (IS_EMERGENCY_MODE_ON) {
    return 5;
  } else {
    return ROOMS_MODES[data[0]];
  }
}

/**===================== SERIAL COMMUNICATION =====================**/
vector<int> handleArguments(vector<int> data) {
  int mth = *data.begin();
  data = vector<int>(data.begin() + 1, data.end());

  vector<int> response;

  switch (mth) {
    case 1:
      set(data);
      response.push_back(1);
      break;

    case 2:
      response.push_back(get(data));
      break;

    case 3:
      feature(data);
      response.push_back(1);
      break;

    case 4:
      response.push_back(getFeature(data));
      break;

    case 5:
      lightning(data);
      response.push_back(1);
      break;

    case 6:
      response.push_back(getLightning(data));
      break;

    case 7:
      color(data);
      response.push_back(1);
      break;

    case 8:
      response.push_back(getColor(data));
      break;

    case 9:
      mode(data);
      response.push_back(1);
      break;

    case 10:
      response.push_back(getMode(data));
      break;
  }

  return response;
}

void handleSerial() {
  if (IS_SERIAL_BLOCKED) {
    return;
  }

  if (Serial1.available() > 0) {
    vector<int> data = split(Serial1.readStringUntil('\n').c_str(), ' ');

    if (Serial1.available() > 0) {
      Serial1.read();
    }

    vector<int> response = handleArguments(data);

    if (response.size() > 0) {
      for (vector<int>::iterator it = response.begin(); it != response.end(); ++it) {
        Serial1.print(*it);

        if (it != response.end() - 1) {
          Serial1.print(' ');
        } else {
          Serial1.print('\n');
        }
      }
    }
  }
}

/**===================== MODES =====================**/
void handleModes() {
  for (vector<int>::iterator it = ROOMS_MODES.begin(); it != ROOMS_MODES.end(); ++it) {

  }
}

/**===================== BODY =====================**/
void setup() {
  ROOMS_LIGHTNING_PINS = {{2, 3, 4}, {5, 5, 5}, {6, 6, 6}, {7, 7, 7}, {8, 8, 8}, {9, 9, 9}, {10, 10, 10}, {11, 11, 11}, {12, 12, 12}, {13, 13, 13}};

  for (vector<vector<int>>::iterator it = ROOMS_LIGHTNING_PINS.begin(); it != ROOMS_LIGHTNING_PINS.end(); ++it) {
    for (vector<int>::iterator it1 = it->begin(); it1 != it->end(); ++it1) {
      pinMode(*it1, OUTPUT);
      analogWrite(*it1, 0);
    }
  }

  Serial.begin(115200);
  Serial.setTimeout(100);

  Serial1.begin(115200);
  Serial1.setTimeout(100);

  serialThread.onRun(handleSerial);
  serialThread.setInterval(THREAD_SERIAL_INTERVAL);

  modelThread.onRun(handleModes);
  modelThread.setInterval(THREAD_MODE_INTERVAL);
}

void loop() {
  if (serialThread.shouldRun()) {
    serialThread.run();
  }

  if (modelThread.shouldRun()) {
    serialThread.run();
  }
}
