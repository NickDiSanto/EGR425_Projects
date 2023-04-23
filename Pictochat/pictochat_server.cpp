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

BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *drawArray;
static BLEAdvertisedDevice *bleRemoteServer;
bool deviceConnected = false;
int timer = 0;
static boolean doConnect = false;
static boolean doScan = false;
static std::vector<std::vector<int>> pointsToSend;
static std::vector<std::vector<int>> pointsReceived;

#define SERVICE_UUID "7d507c13-81fc-4b34-9bde-6d5b71c0bb41"
#define PICTO_DATA_UUID "b77e5cf1-99fe-43b2-9b26-8240bf8e4e01"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("Device connected...");
    }
    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }

};
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks{
    void onWrite(){
        Serial.println("message recieved");
        // Get the data from the BLE characteristic
        // Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
        uint8_t* pData = drawArray->getData();
        size_t length = drawArray->getLength();

        for (int i = 0; i < length; i+=4){
            std::vector<int> temp;
            for (int j = 0; j < 4; j++){
                temp.push_back(pData[i + j]);
            }
            pointsReceived.push_back(temp);
        }

        for (int i = 0; i < pointsReceived.size(); i++) {
            for (int j = 0; j < 4; j++) {
                Serial.printf("point: %d  ", pointsReceived.at(i).at(j));
            }
            Serial.println();
        }
    }
};

void broadcastBleServer() {    
    // Initializing the server, a service and a characteristic 
    static const uint16_t MAX_CHARACTERISTIC_LENGTH = 512;
    BLEDevice::setMTU(MAX_CHARACTERISTIC_LENGTH);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyServerCallbacks());
    bleService = bleServer->createService(SERVICE_UUID);
    drawArray = bleService->createCharacteristic(PICTO_DATA_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    uint8_t data[] = {30,55,10,0};
    drawArray->setValue(data, sizeof(data));
    drawArray->setCallbacks(new MyCharacteristicCallbacks());
    bleService->start();

    // Start broadcasting (advertising) BLE service
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x12); // Use this value most of the time 
    // bleAdvertising->setMinPreferred(0x06); // Functions that help w/ iPhone connection issues 
    // bleAdvertising->setMinPreferred(0x00); // Set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined...you can connect with your phone!"); 
}

class PointObject {
    public:
        Point point;
        int penThickness;
        int penColor;
};

// Time variables
unsigned long lastTime;
static const unsigned long timerDelay = 500;

static int sWidth;
static int sHeight;

static int penThickness;
static int penColor;

static bool isClientTurn;
static bool isErasing;

void drawBackground();

bool wasPressed(int x1,int y1,int x2,int y2, Point p) {
    bool betweenX = (x1 > p.x && x2 < p.x) || (x1 < p.x && x2 > p.x);
    bool betweenY = (y1 > p.y && y2 < p.y) || (x1 < p.y && y2 > p.y);
    return betweenX && betweenY;
}
static void updateChatState() {
    // uint8_t data[] = {30, 55,10, 1, 35, 60, 10, 1, 50, 60, 10, 1, 80, 60, 10, 1, 35, 90, 10, 1, 78, 60, 10, 1, 55, 99, 10, 1, 100, 60, 10, 1, 240, 60, 10, 1, 35, 190, 10, 1, 200, 60, 10, 1};
    // drawArray->setValue(data, sizeof(data));
    int count=0;
    for (const auto& inner_vector : pointsToSend) {
        for (const auto& value : inner_vector) {
            Serial.printf("%d, ", value);
            count++;
        }
    }
    Serial.printf("\n COUNT:: %d\n", count);

    uint8_t p2s[count];
    for (int i=0; i<count; i+=4) {
        for (int j=0; j<4; j++) {
            // Serial.printf("%d, ", val);
            uint8_t byt = static_cast<uint8_t>(pointsToSend[i/4][j]);
            Serial.printf("%d, ", byt);
            // Do something with byte
            p2s[i+j]=byt;
            Serial.printf("%d, ", p2s[i+j]);
        }
    }
    Serial.println("\n");
    for(int i =0;i<count; i++){
        Serial.printf("%d, ", p2s[i]);
    }
    Serial.printf("\nSIZE:: %d\n", sizeof(p2s));
    drawArray->setValue(p2s, sizeof(p2s));
    drawArray->notify();
    // delete[] p2s;
    M5.Lcd.fillScreen(TFT_WHITE);
    drawBackground();
}


void setup() {
    M5.begin();
    M5.IMU.Init();
    M5.Touch.begin();

    String bleDeviceName = "UserOne";
    BLEDevice::init(bleDeviceName.c_str());
    broadcastBleServer();

    penThickness = 2;
    penColor = TFT_BLACK;
    
    isClientTurn = false;
    isErasing = false;

    lastTime = 0;
    
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

    if (deviceConnected) {
        if (!isClientTurn) {
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
                // Redrawing previous message for 5 sec
                if (!pointsReceived.empty()) {
                    M5.Lcd.fillScreen(TFT_WHITE);
                    drawBackground();
                    for (std::vector<int> point : pointsReceived) {
                        M5.Lcd.fillCircle(point[0], point[1], penThickness, penColor);
                    }
                    delay(5000);
                    // Redrawing current image
                    M5.Lcd.fillScreen(TFT_WHITE);
                    drawBackground();
                    for (std::vector<int> point : pointsToSend) {
                        M5.Lcd.fillCircle(point[0], point[1], penThickness, penColor);
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
                isErasing = false;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(0, 110, 30, 169, M5.Touch.getPressPoint())) {
                penColor = TFT_RED;
                isErasing = false;
                M5.Spk.DingDong();
                delay(100);
            } else if (wasPressed(0, 170, 30, sHeight, M5.Touch.getPressPoint())) {
                penColor = TFT_BLUE;
                isErasing = false;
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
                // Drawing points received for 5 sec
                for (std::vector<int> point : pointsReceived) {
                    M5.Lcd.fillCircle(point[0], point[1], point[2], point[3]);
                }
                pointsToSend.clear();
                delay(5000);
                isClientTurn=!isClientTurn;
            } else {
                // Serial.print("Waiting for message...");
                delay(1000);
            }
            // // Update the last time to NOW
            lastTime = millis();
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
