#include <M5Core2.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

// BtnA sends message
// BtnB clears screen
// BtnC redraws previous message

BLERemoteCharacteristic *userOneData;
static BLEAdvertisedDevice *bleRemoteServer;
bool deviceConnected = false;
int timer = 0;
static boolean doConnect = false;
static boolean doScan = false;
BLERemoteCharacteristic *drawArray;
static std::vector<std::vector<int>> pointsToSend;
static std::vector<std::vector<int>> pointsReceived;

#define SERVICE_UUID "7d507c13-81fc-4b34-9bde-6d5b71c0bb41"
#define DRAW_ARRAY_UUID "b77e5cf1-99fe-43b2-9b26-8240bf8e4e01"

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Print device found
        Serial.print("BLE Advertised Device found:");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());
        BLEUUID serviceUuid(SERVICE_UUID);

        if (advertisedDevice.haveServiceUUID() && 
                advertisedDevice.isAdvertisingService(serviceUuid) && 
                advertisedDevice.getName() == "UserOne") {
            Serial.print("FOUND");
            BLEDevice::getScan()->stop();
            bleRemoteServer = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }

    }
};

static int sWidth;
static int sHeight;

static int penThickness;
static int penColor;

static bool isClientTurn;

void drawBackground();

bool wasPressed(int x1,int y1,int x2,int y2, Point p) {
    bool betweenX = (x1 > p.x && x2 < p.x) || (x1 < p.x && x2 > p.x);
    bool betweenY = (y1 > p.y && y2 < p.y) || (x1 < p.y && y2 > p.y);
    return betweenX && betweenY;
}

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);

    for (int i = 0; i < length; i += 4){
        std::vector<int> temp;
        for (int j = 0; j < 4; j++){
            temp.push_back(pData[i + j]);
        }
        pointsReceived.push_back(temp);
    }
}

static void updateChatState() {
    int count = 0;
    for (const auto& inner_vector : pointsToSend) {
        for (const auto& value : inner_vector) {
            count++;
        }
    }

    uint8_t p2s[count];
    for (int i = 0; i < count; i += 4) {
        for (int j = 0; j < 4; j++) {
            uint8_t byte = static_cast<uint8_t>(pointsToSend[i/4][j]);
            // Do something with byte
            p2s[i + j] = byte;
        }
    }
    drawArray->writeValue(p2s, sizeof(p2s));
    M5.Lcd.fillScreen(TFT_WHITE);
    drawBackground();
}


bool connectToServer() {
    // Create the client
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());
    Serial.println("\tClient connected");
    BLEUUID serviceUuid(SERVICE_UUID);
    BLEUUID drawUuid(DRAW_ARRAY_UUID);

    // Connect to the remote BLE Server.
    if (!bleClient->connect(bleRemoteServer))
        Serial.printf("FAILED to connect to server (%s)\n", bleRemoteServer->getName().c_str());
    Serial.printf("\tConnected to server (%s)\n", bleRemoteServer->getName().c_str());
    static const uint16_t MAX_CHARACTERISTIC_LENGTH = 512;
    bleClient->setMTU(MAX_CHARACTERISTIC_LENGTH);

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);
    if (bleRemoteService == nullptr) {
        Serial.printf("Failed to find our service UUID: %s\n", serviceUuid.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our service UUID: %s\n", serviceUuid.toString().c_str());

    userOneData = bleRemoteService->getCharacteristic(drawUuid);
    if (userOneData == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", drawUuid.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", drawUuid.toString().c_str());

    // Read the value of the characteristic
    if (userOneData->canRead()) {
        std::string value = userOneData->readValue();
        Serial.printf("The characteristic value was: %s", value.c_str());
        delay(3000);
    }
    
    // Check if server's characteristic can notify client of changes and register to listen if so
    if (userOneData->canNotify())
        userOneData->registerForNotify(notifyCallback);
    deviceConnected = true;
    doConnect=false;
    return true;
}


void setup() {
    M5.begin();
    M5.IMU.Init();
    M5.Touch.begin();

    String bleDeviceName = "UserOne";
    BLEDevice::init("");
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    penThickness = 6;
    penColor = TFT_BLACK;
    
    isClientTurn = false;
    
    // Set screen orientation and get height/width 
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();
    M5.Lcd.setBrightness(0);

    M5.Lcd.fillScreen(TFT_WHITE);
    drawBackground();
}


void loop() {
    M5.update();
    M5.Touch.update();
    drawBackground();

    if (doConnect == true) {
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
        if (isClientTurn) {
            if (M5.BtnA.isPressed()) {
                M5.Spk.DingDong();

                Serial.println("Sending...");
                // Sending to other user and changing turns
                updateChatState();
                Serial.println("Clearing...");
                pointsToSend.clear();
                isClientTurn = !isClientTurn;
                pointsReceived.clear();
            } else if (M5.BtnB.isPressed()) {
                M5.Spk.DingDong();

                // Clear screen
                M5.Lcd.fillScreen(TFT_WHITE);
                drawBackground();
                pointsToSend.clear();
            } else if (M5.BtnC.isPressed()) {
                M5.Spk.DingDong();

                // Redrawing previous message for 10 sec
                if (!pointsReceived.empty()) {
                    M5.Lcd.fillScreen(TFT_WHITE);
                    drawBackground();
                    for (std::vector<int> point : pointsReceived) {
                        M5.Lcd.fillCircle(point[0], point[1], point[2], point[3]);
                    }
                    delay(10000);
                    // Redrawing current image
                    M5.Lcd.fillScreen(TFT_WHITE);
                    drawBackground();
                    for (std::vector<int> point : pointsToSend) {
                        M5.Lcd.fillCircle(point[0], point[1], point[2], point[3]);
                    }
                }
            } else if (wasPressed(0, 0, 50, 55, M5.Touch.getPressPoint())) {
                penColor = TFT_WHITE;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(100, 40, 51, 0, M5.Touch.getPressPoint())) {
                penThickness = 3;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(160, 40, 101, 0, M5.Touch.getPressPoint())) {
                penThickness = 6;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(230, 40, 161, 0, M5.Touch.getPressPoint())) {
                penThickness = 10;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(sWidth, 40, 231, 0, M5.Touch.getPressPoint())) {
                penThickness = 16;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(0, 56, 30, 109, M5.Touch.getPressPoint())) {
                penColor = TFT_BLACK;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(0, 110, 30, 169, M5.Touch.getPressPoint())) {
                penColor = TFT_RED;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(0, 170, 30, sHeight, M5.Touch.getPressPoint())) {
                penColor = TFT_BLUE;
                M5.Spk.DingDong();
                delay(100);
            } else {
                // Drawing points to send
                Point point = M5.Touch.getPressPoint();
                if (point != Point(-1, -1) && point.x - penThickness > 30 && point.y - penThickness > 40) {
                    std::vector<int> pointVec = { point.x, point.y, penThickness, penColor };

                    if (!(std::find(pointsToSend.begin(), pointsToSend.end(), pointVec) != pointsToSend.end())) {
                        M5.Lcd.fillCircle(point.x, point.y, penThickness, penColor);
                        std::vector<int> pointVec = { point.x, point.y, penThickness, penColor };
                        pointsToSend.push_back(pointVec);
                    }

                }
            }
        } else {
            if (!pointsReceived.empty()) {
                Serial.println("Recieved points, drawing...");
                // Drawing points received for 10 sec
                for (std::vector<int> point : pointsReceived) {
                    M5.Lcd.fillCircle(point[0], point[1], point[2], point[3]);
                }
                delay(10000);
                pointsToSend.clear();
                M5.Lcd.fillScreen(TFT_WHITE);
                drawBackground();
                isClientTurn = !isClientTurn;
            } else {
                Serial.print("Waiting for message...");
                delay(1000);
            }
        }
    }
}

void drawBackground() {
    M5.Lcd.drawLine(30, 40, 30, sHeight, TFT_BLACK);
    M5.Lcd.drawLine(30, 40, sWidth, 40, TFT_BLACK);

    M5.Lcd.fillRect(8, 9, 16, 24, TFT_PINK);
    M5.Lcd.setCursor(11, 14);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("E");

    M5.Lcd.fillRect(6, 70, 18, 18, TFT_BLACK);
    M5.Lcd.fillRect(6, 130, 18, 18, TFT_RED);
    M5.Lcd.fillRect(6, 190, 18, 18, TFT_BLUE);

    M5.Lcd.fillCircle(70, 20, 3, penColor);
    M5.Lcd.fillCircle(130, 20, 6, penColor);
    M5.Lcd.fillCircle(195, 20, 10, penColor);
    M5.Lcd.fillCircle(270, 20, 16, penColor);
}
