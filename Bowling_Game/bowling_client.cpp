#include <M5Core2.h>
#include <array>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <sstream>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////
// BLE Server 
///////////////////////////////////////////////////////////////

BLERemoteCharacteristic *bowlerOneData;
static BLEAdvertisedDevice *bleRemoteServer;
bool deviceConnected = false;
int timer = 0;
static boolean doConnect = false;
static boolean doScan = false;

#define SERVICE_UUID "7d507c13-81fc-4b34-9bde-6d5b71c0bb41"
#define BOWLER_ONE_DATA_UUID "b77e5cf1-99fe-43b2-9b26-8240bf8e4e01"

class MyClientCallback : public BLEClientCallbacks{
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
        //drawScreenTextWithBackground("LOST connection to device.\n\nAttempting re-connection...", TFT_RED);
    }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Print device found
        Serial.print("BLE Advertised Device found:");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());
        BLEUUID serviceUuid(SERVICE_UUID);

        // More debugging print
        // Serial.printf("\tAddress: %s\n", advertisedDevice.getAddress().toString().c_str());
        // Serial.printf("\tHas a ServiceUUID: %s\n", advertisedDevice.haveServiceUUID() ? "True" : "False");
        // for (int i = 0; i < advertisedDevice.getServiceUUIDCount(); i++) {
        //    Serial.printf("\t\t%s\n", advertisedDevice.getServiceUUID(i).toString().c_str());
        // }
        // Serial.printf("\tHas our service: %s\n\n", advertisedDevice.isAdvertisingService(serviceUuid) ? "True" : "False");
        
        // We have found a device, let us now see if it contains the service we are looking for.
        
        if (advertisedDevice.haveServiceUUID() && 
                advertisedDevice.isAdvertisingService(serviceUuid) && 
                advertisedDevice.getName() == "BowlerOne") {
            Serial.print("FOUND");
            BLEDevice::getScan()->stop();
            bleRemoteServer = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }

    }     
};       

///////////////////////////////////////////////////////////////


static const int maxScore = 50;
static const float dt = 0.05;
static const unsigned long timerDelay = 500;
static int pad;

static bool isPlayerOneTurn;

static int playerOneScore;
static int playerTwoScore;

static bool isPickingBowlingOption;
static bool isPowerBowl;
static bool isDrawingBowlingScreen;
static bool isDrawingPins;
bool isReading;

static int numPinsHit;
std::array<float, 3> curVel {};

unsigned long lastTime;
long ts;

int sWidth;
int sHeight;

bool wasPressed(int x1,int y1,int x2,int y2, Point p){
    bool betweenX = (x1 > p.x && x2 < p.x) || (x1 < p.x && x2 > p.x);
    bool betweenY = (y1 > p.y && y2 < p.y) || (x1 < p.y && y2 > p.y);
    return betweenX && betweenY;
  }

void drawFinalScoreboard();
void drawBowlingOptions();
void drawBowlingScreen();
void calculateThrow();
void drawPins();
bool connectToServer();

static void updateGameState(){
    if (bowlerOneData->canRead()) {
        std::string value=bowlerOneData->readValue();
        Serial.printf("update: %s\n", value.c_str());
        std::istringstream iss(value.c_str());
        std::vector<std::string> vals;
        std::string val;
        while (iss >> val) {
            vals.push_back(val);
        }
        if(std::stoi(vals[2]) == 2 && isPlayerOneTurn){
            playerOneScore=std::stoi(vals[0]);
            isPlayerOneTurn=false;
        }else if(std::stoi(vals[2]) == 2 && !isPlayerOneTurn){
            isPlayerOneTurn=true;
            playerTwoScore += numPinsHit;
            value=std::to_string(playerOneScore)+" "+std::to_string(playerTwoScore)+" "+std::to_string(1);
            if (bowlerOneData->canWrite()) {
            bowlerOneData->writeValue(value);
            }
        }
    }    
}


static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    Serial.printf("\tData: %s", (char *)pData);
    std::uint16_t value = pBLERemoteCharacteristic->readUInt16();
    Serial.printf("\tValue was: %d", value);
}


void setup() {
    // Initialize the device
    M5.begin();
    M5.IMU.Init();
    M5.Touch.begin();

    String bleDeviceName = "BowlerOne";
    BLEDevice::init("");
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
    
    isPlayerOneTurn = true;

    playerOneScore = 0;
    playerTwoScore = 0;

    isPickingBowlingOption = true;
    isPowerBowl = false;
    isDrawingBowlingScreen = false;
    isDrawingPins = false;
    isReading = false;

    lastTime = 0;
    ts = millis();
    
    // Set screen orientation and get height/width 
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();
    M5.Lcd.setBrightness(0);
}


void loop() {
    M5.update();
    if (doConnect == true)
        {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
            doConnect = false;
            delay(3000);
        }
        else {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
            delay(3000);
        }
    }

    if (deviceConnected) {
        // Only execute every so often --> for the buttons we might want to update more often
        if ((millis() - lastTime) > timerDelay) {
            
            if (playerOneScore >= maxScore || playerTwoScore >= maxScore) {
                isPickingBowlingOption = false;
                drawFinalScoreboard();
            }
            if(isPlayerOneTurn){
                updateGameState();
                Serial.print("others turn");
            }else{
              if (isPickingBowlingOption) {
                    drawBowlingOptions();
                }

                if (isDrawingBowlingScreen) {
                    drawBowlingScreen();
                }

                if (isDrawingPins) {
                    drawPins();
                    isDrawingPins = false;
                    isPickingBowlingOption = true;
                    updateGameState();
                }  
            }
            

            // Update the last time to NOW
            lastTime = millis();
        }
    }
}


void drawBowlingOptions() {
    M5.Lcd.fillScreen(TFT_PINK);
    
    M5.Lcd.fillRect(30, 50, 100, sHeight - 90, TFT_DARKGREY);
    M5.Lcd.fillRect(sWidth - 130, 50, 100, sHeight - 90, TFT_DARKGREY);

    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_BLACK);
    pad = 10;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.printf("P1:%i", playerOneScore);

    M5.Lcd.setCursor(sWidth - 100, pad);
    M5.Lcd.printf("P2:%i", playerTwoScore);

    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(35, 100);
    M5.Lcd.print("Contact");
    M5.Lcd.setCursor(35, 125);
    M5.Lcd.print(" Bowl");

    M5.Lcd.setCursor(sWidth - 125, 100);
    M5.Lcd.print(" Power");
    M5.Lcd.setCursor(sWidth - 125, 125);
    M5.Lcd.print(" Bowl");

    if (wasPressed(0, 0, 130, sHeight, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        isPowerBowl = false;
        isPickingBowlingOption = false;
        isDrawingBowlingScreen = true;
        
        // FIXME: power selection barely works
    } else if (wasPressed(sWidth, sHeight, sWidth - 130, 0, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        isPowerBowl = true;
        isPickingBowlingOption = false;
        isDrawingBowlingScreen = true;
    }

    delay(1000);
}


void drawBowlingScreen() {
    M5.Lcd.fillScreen(TFT_BLACK);
    pad = 100;
    M5.Lcd.setCursor(44, pad);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.print("Press to bowl");

    if (wasPressed(0, 0, sWidth, sHeight, M5.Touch.getPressPoint()) and ts+50 <= millis()) {
        ts = millis();
        isReading = true;
        float x;
        float y;
        float z;
        M5.IMU.getAccelData(&x, &y, &z);
        // Remove the first element and shift the remaining elements left
        // Add a new element at the end
        std::array<float, 3> accel = {x, y, z};
        Serial.printf("X:%f\nY:%f\nZ:%f\n", x, y, z);
        for (int i = 0; i < 3; i++) {
            curVel[i] = curVel[i] + (dt * accel[i]);
        }
    }

    if (isReading && M5.Touch.getPressPoint() == Point(-1, -1)) {
        M5.Lcd.clear();
        calculateThrow();
        M5.Lcd.setCursor(0, 45);
        M5.Lcd.printf("X:%f\nY:%f\nZ:%f\n", curVel[0], curVel[1], curVel[2]);
        isReading = false;
        curVel[0] = 0.0;
        curVel[1] = 0.0;
        curVel[2] = 0.0;

        isDrawingBowlingScreen = false;
        isDrawingPins = true;
    }
}


void calculateThrow() {
    float absVel = std::sqrt(curVel[0] * curVel[0] + curVel[1] * curVel[1] + curVel[2] * curVel[2]);
    float timeToStrike = 20.0 / absVel;
    float driftX = timeToStrike * std::sqrt(curVel[0] * curVel[0]); // abs of the x drift

    if (isPowerBowl) {
        absVel = 1.0;
        driftX *= 1.3;
    }

    if (driftX > 4.0 || absVel < 0.14) {
        numPinsHit = 0;
    } else if (driftX > 3.5) {
        if (absVel > 0.25) {
            numPinsHit = 2;
        } else {
            numPinsHit = 1;
        }
    } else if (driftX > 3.0) {
        if (absVel > 0.25) {
            numPinsHit = 4;
        } else {
            numPinsHit = 3;
        }
    } else if (driftX > 2.5) {
        if (absVel > 0.25) {
            numPinsHit = 6;
        } else {
            numPinsHit = 5;
        }
    } else if (driftX > 2.0) {
        if (absVel > 0.25) {
            numPinsHit = 8;
        } else {
            numPinsHit = 7;
        }
    } else {
        if (absVel > 0.25) {
            numPinsHit = 10;
        } else {
            numPinsHit = 9;
        }
    }
}


void drawPins() {
    M5.Lcd.fillScreen(TFT_YELLOW);
    int numPinsLeft = 10 - numPinsHit;

    for (int i = 0; i < numPinsLeft; i++) {
        // Pin is in back row
        if (i < 4) {
            M5.Lcd.fillCircle(sWidth/3 + i * 32, sHeight/2 - 50, 14, BLACK);
            continue;

        // Pin is in second to back row
        } else if (i < 7) {
            M5.Lcd.fillCircle(sWidth/3 + (i - 4) * 32 + 16, sHeight/2 - 24, 14, BLACK);
            continue;

        // Pin is in second to front row
        } else if (i < 9) {
            M5.Lcd.fillCircle(sWidth/3 + (i - 6) * 32, sHeight/2 + 2, 14, BLACK);
            continue;

        // Pin is in front
        } else {
            M5.Lcd.fillCircle(sWidth/3 + 48, sHeight/2 + 28, 14, BLACK);
        }
    }

    for (int i = 0; i < numPinsHit; i++) {
        if (i == 0) {
            M5.Lcd.fillCircle(sWidth/3 + 100, sHeight/2 + 12, 14, RED);
            continue;
        } else if (i == 1) {
            M5.Lcd.fillCircle(sWidth/3 + 132, sHeight/2 - 16, 14, RED);
            continue;
        } else if (i == 2) {
            M5.Lcd.fillCircle(sWidth/3, sHeight/2 + 4, 14, RED);
            continue;
        } else if (i == 3) {
            M5.Lcd.fillCircle(sWidth/3 + 148, sHeight/2 - 58, 14, RED);
            continue;
        } else if (i == 4) {
            M5.Lcd.fillCircle(sWidth/3 + 46, sHeight/2 - 24, 14, RED);
            continue;
        } else if (i == 5) {
            M5.Lcd.fillCircle(sWidth/3 - 32, sHeight/2 - 34, 14, RED);
            continue;
        } else if (i == 6) {
            M5.Lcd.fillCircle(sWidth/3 + 116, sHeight/2 - 72, 14, RED);
            continue;
        } else if (i == 7) {
            M5.Lcd.fillCircle(sWidth/3 + 76, sHeight/2 - 96, 14, RED);
            continue;
        } else if (i == 8) {
            M5.Lcd.fillCircle(sWidth/3 + 40, sHeight/2 - 104, 14, RED);
            continue;
        } else if (i == 9) {
            M5.Lcd.fillCircle(sWidth/3 - 20, sHeight/2 - 72 , 14, RED);
            continue;
        }
    }

    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_BLACK);
    pad = 10;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.printf("P1:%i", playerOneScore);

    M5.Lcd.setCursor(sWidth - 100, pad);
    M5.Lcd.printf("P2:%i", playerTwoScore);

    pad = 50;
    M5.Lcd.setCursor(pad, sHeight - 40);

    if (numPinsLeft == 0) {
        M5.Lcd.print("STRIKE!!!!");
    } else if (numPinsLeft == 10) {
        M5.Lcd.print("GUTTERBALL!");
    } else {
        M5.Lcd.printf("%i pin(s) hit!", numPinsHit);
    }

    delay(5000);
}


void drawFinalScoreboard() {
    M5.Lcd.fillScreen(TFT_DARKGREEN);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setTextSize(3);
    pad = 30;
    M5.Lcd.setCursor(50, pad);

    M5.Lcd.printf("P1 Score: %i\n\n", playerOneScore);
    M5.Lcd.setCursor(50, M5.Lcd.getCursorY());
    M5.Lcd.printf("P2 Score: %i\n\n", playerTwoScore);

    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());

    if (playerOneScore > playerTwoScore) {
        M5.Lcd.print(" P1 Wins!!\n\n\n");
    } else {
        M5.Lcd.print(" P2 Wins!!\n\n\n");
    }

    pad = 20;
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.print("Touch screen to restart");

    if (wasPressed(0, 0, sWidth, sHeight, M5.Touch.getPressPoint())) {
        playerOneScore = 0;
        playerTwoScore = 0;
        isPlayerOneTurn = true;
        isPickingBowlingOption = true;
        isPowerBowl = false;
        isDrawingBowlingScreen = false;
        isDrawingPins = false;
    }
}

bool connectToServer(){
    // Create the client
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());
    Serial.println("\tClient connected");
    BLEUUID serviceUuid(SERVICE_UUID);
    BLEUUID bowlerOneDataUuid(BOWLER_ONE_DATA_UUID);

    // Connect to the remote BLE Server.
    if (!bleClient->connect(bleRemoteServer))
        Serial.printf("FAILED to connect to server (%s)\n", bleRemoteServer->getName().c_str());
    Serial.printf("\tConnected to server (%s)\n", bleRemoteServer->getName().c_str());

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);
    if (bleRemoteService == nullptr) {
        Serial.printf("Failed to find our service UUID: %s\n", serviceUuid.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our service UUID: %s\n", serviceUuid.toString().c_str());

    bowlerOneData = bleRemoteService->getCharacteristic(bowlerOneDataUuid);
    if (bowlerOneData == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", bowlerOneDataUuid.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", bowlerOneDataUuid.toString().c_str());

    // Read the value of the characteristic
    if (bowlerOneData->canRead()) {
        std::string value = bowlerOneData->readValue();
        Serial.printf("The characteristic value was: %s", value.c_str());
        delay(3000);
    }
    
    // Check if server's characteristic can notify client of changes and register to listen if so
    if (bowlerOneData->canNotify())
        bowlerOneData->registerForNotify(notifyCallback);
    deviceConnected = true;
    doConnect=false;
    return true;
}
