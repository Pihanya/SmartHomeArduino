#include <StandardCplusplus.h>
#include <system_configuration.h>
#include <unwind-cxx.h>
#include <utility.h>

#include <string>
#include <sstream>
#include <vector>

#include <StaticThreadController.h>
#include <Thread.h>
#include <ThreadController.h>

#include <MFRC522.h>
#include <DHT.h>
#include <MQ2.h>
#include <DS1302.h>

using namespace std;

/**===================== RFID =====================**/
#define RST_PIN 9           // Configurable, see typical pin layout above
#define SS_PIN 10          // Configurable, see typical pin layout above

#define ALLOWN_KEYS 2
#define UID_SIZE 4
byte allownKeys[ALLOWN_KEYS][UID_SIZE] =  {
  {0x76, 0x2f, 0x2D, 0x03}, // 76 2F 2D 03
  {0x06, 0x7c, 0xb1, 0x65}, // 06 7C B1 65
};

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

/**===================== DHT11 =====================**/
#define DHT11_PIN 2

DHT dht;

/**===================== GAS =====================**/
#define GAS_ANALOG_PIN A0

MQ2 gas(GAS_ANALOG_PIN);

/**===================== CLOCK =====================**/
DS1302 rtc(2, 3, 4);

/**===================== THREADS =====================**/
Thread serialThread = Thread();
Thread rfidThread = Thread();
Thread dhtThread = Thread();
Thread gasThread = Thread();

bool IS_SERIAL_BLOCKED = false;

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

bool isAllownCard(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < ALLOWN_KEYS; i++) {
    if (byteArrayEquals(buffer, bufferSize, allownKeys[i], UID_SIZE)) {
      return true;
    }
  }

  return false;
}

bool byteArrayEquals(byte *buffer, byte bufferSize, byte *buffer1, byte bufferSize1) {
  if (bufferSize != bufferSize1) {
    return false;
  }

  for (int i = 0; i < bufferSize; i++) {
    if (buffer[i] != buffer1[i])
      return false;
  }

  return true;
}

int pushSerial(int mth, vector<int> data) {
  Serial.print(mth);

  if (data.length == 0) {
    return -1;
  }

  IS_SERIAL_BLOCKED = true;
  Serial.print(' ');
  for (vector<int>::iterator it = data.begin(); it != data.end(); ++it) {
    Serial.print(*it);
    
    if (it != data.end() - 1) {
      Serial.print(' ');
    } else {
      Serial.print('|');
    }
  }

  IS_SERIAL_BLOCKED = false;
}
/**===================== ARDUINO COMMUNICATION PROCOTOL =====================**/

/** [5] int room, int value **/
void lightning(vector<int> data) {
  pushSerial(5, data);
}

/** [6] int room **/
/** Returns value of the lightning (enabled/disabled) (int value) **/
int getLightning(vector<int> data) {
  return pushSerial(6, data);
}

/** [7] int room, int color, [int toSave] **/
void color(vector<int> &data) {
  pushSerial(7, data);
}

/** [8] int room **/
/** Returns three colors of lightning (int red, int green, int blue) **/
int getColor(vector<int> data) {
  return pushSerial(8, data);
}

/** [9] int room, int mode, int value [int duration] **/
void mode(vector<int> data) {
  pushSerial(9, data);
}

/** [10] int room **/
/** Returns value of the mode in room (int value)**/
int getMode(vector<int> data) {
  return pushSerial(10, data);
}

/**===================== SERIAL COMMUNICATION =====================**/

vector<int> handleArguments(vector<int> data) {
  int mth = *data.begin();
  data = vector<int>(data.begin() + 1, data.end());

  vector<int> response;

  switch (mth) {
    case 1:
      set(data);
      break;

    case 2:
      response.push_back(pGet(data));
      break;

    case 3:
      feature(data);
      break;

    case 4:
      response.push_back(getFeature(data));
      break;

    case 5:
      lightning(data);
      break;

    case 6:
      response.push_back(getLightning(data));
      break;

    case 7:
      color(data);
      break;

    case 8:
      return getColor(data);
      break;

    case 9:
      mode(data);
      break;

    case 10:
      response.push_back(getMode(data));
      break;
  }

  return response;
}

void handleSerial() {
  if (IS_SERIAL_BLOCKED)
    return;

  if (Serial.available() > 0) {
    vector<int> data = split(Serial.readStringUntil('|').c_str(), ' ');

    if (Serial.available() > 0) {
      Serial.read();
    }

    vector<int> response = handleArguments(data);

    if (response.size() > 0) {
      for (vector<int>::iterator it = response.begin(); it != response.end(); ++it) {
        Serial.print(*it);

        if (it != response.end() - 1) {
          Serial.print(' ');
        } else {
          Serial.print('|');
        }
      }
    }
  }
}

/**===================== RFID HANDLER =====================**/
void handleRFID() {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  Serial.println(isAllownCard(mfrc522.uid.uidByte, mfrc522.uid.size) ? "Allown" : "NOT");
}

/**===================== DHT HANDLER =====================**/
void handleDHT() {
  int hum = dht.getHumidity();
  int temp = dht.getTemperature();

  Serial.print(hum);
  Serial.print(" ");
  Serial.println(temp);
}

/**===================== GAS HANDLER =====================**/
#define GAS_CALIBRATION_CYCLES 5
#define GAS_CALIBRATION_CYCLE_DELAY 1000

#define GAS_LPG_DIFFERENCE 150
#define GAS_CO_DIFFERENCE 150
#define GAS_SMOKE_DIFFERENCE 150

float lpgAvg, coAvg, smokeAvg, lpg, co, smoke;

void handleGas() {
  lpg = gas.readLPG();
  co = gas.readCO();
  smoke = gas.readSmoke();

  if (lpg - lpgAvg >= GAS_LPG_DIFFERENCE) {

  } else if (co - coAbg >= GAS_CO_DIFFERENCE) {

  } else if (smoke - smokeAbg >= GAS_SMOKE_DIFFERENCE) {

  }
}

void calibrateGas() {
  float lpgSum = 0;
  float coSum = 0;
  float smokeSum = 0;

  for (int i = 0; i < GAS_CALIBRATION_CYCLES; i++) {
    lpgSum += gas.readLPG;
    coSum = gas.readCO();
    smokeSum = gas.readSmoke();

    delay(GAS_CALIBRATION_CYCLE_DELAY);
  }

  lpg = lpgSum / GAS_CALIBRATION_CYCLES;
  co = coSum / GAS_CALIBRATION_CYCLES;
  smoke = smokeSum / GAS_CALIBRATION_CYCLES;
}

/**===================== BODY =====================**/
void setup() {
  Serial.begin(9600);

  while (!Serial);

  gas.begin();

  calibrateGas();

  SPI.begin();                // Init SPI bus
  mfrc522.PCD_Init();         // Init MFRC522 card

  dht.setup(DHT11_PIN);

  serialThread.onRun(handleSerial);
  serialThread.setInterval(10);

  rfidThread.onRun(handleRFID);
  rfidThread.setInterval(100);

  dhtThread.onRun(handleDHT);
  dhtThread.setInterval(5000);

  gasThread.onRun(handleGas);
  gasThread.setInterval(200);
}

void loop() {
  if (serialThread.shouldRun()) {
    serialThread.run();
  }

  if (rfidThread.shouldRun()) {
    rfidThread.run();
  }

  if (dhtThread.shouldRun()) {
    dhtThread.run();
  }

  if (gasThread.shouldRun()) {
    gasThread.run();
  }
}
