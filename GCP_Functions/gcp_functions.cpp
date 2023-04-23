#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include <string>
#include "FS.h"                 // SD Card ESP32
#include <EEPROM.h>             // read and write from flash memory
#include <NTPClient.h>          // Time Protocol Libraries
#include <WiFiUdp.h>            // Time Protocol Libraries
#include <Adafruit_VCNL4040.h>  // Sensor libraries
#include "Adafruit_SHT4x.h"     // Sensor libraries
#include <string>

const String URL_GCF_UPLOAD = "https://us-west2-regal-fortress-380821.cloudfunctions.net/egr-425-2023";
const String URL_GCF_GET = "https://us-west2-regal-fortress-380821.cloudfunctions.net/egr-425-get";

String wifiNetworkName = "Dumbledores_Office";
String wifiPassword = "Lemon_Drops";

// Initialize library objects (sensors and Time protocols)
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelayMs = 3000;

const uint16_t primaryTextColor = TFT_BLACK;
const String userId = "NickDiSanto";

uint16_t prox;
uint16_t ambientLight;
uint16_t whiteLight;

sensors_event_t rHum, temp;

float accX;
float accY;
float accZ;

bool isDrawingSensorDisplay = true;
bool isFetchingButtons = false;
bool isDisplayingResults = false;

String user = "NickDiSanto";
int duration = 5;
String dataType = "Temp";
String curMessage = "";

bool wasPressed(int x1,int y1,int x2,int y2, Point p) {
    bool betweenX = (x1 > p.x && x2 < p.x) || (x1 < p.x && x2 > p.x);
    bool betweenY = (y1 > p.y && y2 < p.y) || (x1 < p.y && y2 > p.y);
    return betweenX && betweenY;
}


struct deviceDetails {
    int prox;
    int ambientLight;
    int whiteLight;
    double rHum;
    double temp;
    double accX;
    double accY;
    double accZ;
};


int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders);
int httpGetDataWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders);
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details);
bool gcfGetData(String serverUrl, String userId);
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details);
String generateGetDataHeader(String userId, String dataType, int  timeDuration);
String writeDataToFile(byte * fileData, size_t fileSizeInBytes);
int getNextFileNumFromEEPROM();
double convertFintoC(double f);
double convertCintoF(double c);
void drawSensorDisplay();
void fetchButtons();
void displayResults();


void setup() {
    M5.begin();
    M5.IMU.Init();
    M5.Touch.begin();

    // Initialize VCNL4040
    if (!vcnl4040.begin()) {
        Serial.println("Couldn't find VCNL4040 chip");
        while (1) delay(1);
    }
    Serial.println("Found VCNL4040 chip");

    // Initialize SHT40
    if (!sht4.begin()) {
        Serial.println("Couldn't find SHT4x");
        while (1) delay(1);
    }
    Serial.println("Found SHT4x sensor");
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

    timeClient.begin();
    timeClient.setTimeOffset(3600 * -7);
    curMessage.reserve(1024);
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() {
    M5.update();
    M5.Touch.update();

    if (!isDisplayingResults && !isFetchingButtons) {
    // Read VCNL4040 Sensors
    Serial.printf("Live/local sensor readings:\n");
    prox = vcnl4040.getProximity();
    ambientLight = vcnl4040.getLux();
    whiteLight = vcnl4040.getWhiteLight();
    Serial.printf("\tProximity: %d\n", prox);
    Serial.printf("\tAmbient light: %d\n", ambientLight);
    Serial.printf("\tRaw white light: %d\n", whiteLight);
    M5.Touch.update();
    // Read SHT40 Sensors
    sht4.getEvent(&rHum, &temp); // populate temp and humidity objects with fresh data
    Serial.printf("\tTemperature: %.2fC\n", temp.temperature);
    Serial.printf("\tHumidity: %.2f %%rH\n", rHum.relative_humidity);

    // Read M5's Internal Accelerometer (MPU 6886)
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    accX *= 9.8;
    accY *= 9.8;
    accZ *= 9.8;
    Serial.printf("\tAccel X=%.2fm/s^2\n", accX);        
    Serial.printf("\tAccel Y=%.2fm/s^2\n", accY);
    Serial.printf("\tAccel Z=%.2fm/s^2\n", accZ);
    
    // Get current time as timestamp of last update
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    
    unsigned long long epochMillis = ((unsigned long long)epochTime - 61200)*1000;
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    Serial.printf("\nCurrent Time:\n\tEpoch (ms): %llu", epochMillis);
    Serial.printf("\n\tFormatted: %d/%d/%d ", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_year+1900);
    Serial.printf("%02d:%02d:%02d %s\n\n", timeClient.getHours() % 12, timeClient.getMinutes(), timeClient.getSeconds(), timeClient.getHours() < 12 ? "AM" : "PM");
    
    
        // Device details
        deviceDetails details;
        details.prox = prox;
        details.ambientLight = ambientLight;
        details.whiteLight = whiteLight;
        details.temp = temp.temperature;
        details.rHum = rHum.relative_humidity;
        details.accX = accX;
        details.accY = accY;
        details.accZ = accZ;
	    gcfGetWithHeader(URL_GCF_UPLOAD, userId, epochTime, &details);
        delay(timerDelayMs);

    }
    if (M5.BtnB.isPressed()) {
            M5.Spk.DingDong();
            gcfGetData(URL_GCF_GET,  user);
            isFetchingButtons = false;
            isDisplayingResults = true;
            isDrawingSensorDisplay = false;
            delay(timerDelayMs);

    }else if (M5.BtnA.isPressed()) {
            M5.Spk.DingDong();
            isDrawingSensorDisplay = false;
            isFetchingButtons = true;
            isDisplayingResults = false;
            delay(timerDelayMs);

    }else if (M5.BtnC.isPressed()) {
            M5.Spk.DingDong();
            isDisplayingResults = false;
            isDrawingSensorDisplay = true;
            isFetchingButtons = false;
            user = "NickDiSanto";
            duration = 5;
            dataType = "Temp";
            delay(timerDelayMs);

    }
    if (isDrawingSensorDisplay) {
        drawSensorDisplay();
        delay(750);
    }
    if (isFetchingButtons) {
        fetchButtons();
        delay(750);
    }
    if (isDisplayingResults) {
        displayResults();
        delay(750);
    }

}


bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details) {
    // Allocate arrays for headers
	const int numHeaders = 1;
    String headerKeys [numHeaders] = {"M5-Details"};
    String headerVals [numHeaders];

    // Add formatted JSON string to header
    headerVals[0] = generateM5DetailsHeader(userId, time, details);
    
    // Attempt to post the file
    Serial.println("Attempting post data.");
    int resCode = httpGetWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}


bool gcfGetData(String serverUrl, String userId) {
    // Allocate arrays for headers
	const int numHeaders = 1;
    String headerKeys [numHeaders] = {"M5-Details"};
    String headerVals [numHeaders];
    // Add formatted JSON string to header
    headerVals[0] = generateGetDataHeader(userId, dataType, duration);
    
    // Attempt to post the file
    Serial.println("Attempting get data.");
    int resCode = httpGetDataWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}



String generateGetDataHeader(String userId, String dataType, int  timeDuration) {
    // Allocate M5-Details Header JSON object
    StaticJsonDocument<650> objHeaderM5Details; //DynamicJsonDocument  objHeaderGD(600);
    
    // Add VCNL details
    objHeaderM5Details["userId"] = userId;
    objHeaderM5Details["dataType"] = dataType;
    objHeaderM5Details["timeDuration"] = timeDuration;
    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderM5Details) + 1;
    char cHeaderM5Details [jsonSize];
    serializeJson(objHeaderM5Details, cHeaderM5Details, jsonSize);
    String strHeaderM5Details = cHeaderM5Details;
    //Serial.println(strHeaderM5Details.c_str()); // Debug print
    // Return the header as a String
    return strHeaderM5Details;
}


String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details) {
    // Allocate M5-Details Header JSON object
    StaticJsonDocument<650> objHeaderM5Details; //DynamicJsonDocument  objHeaderGD(600);
    
    // Add VCNL details
    JsonObject objVcnlDetails = objHeaderM5Details.createNestedObject("vcnlDetails");
    objVcnlDetails["prox"] = details->prox;
    objVcnlDetails["al"] = details->ambientLight;
    objVcnlDetails["rwl"] = details->whiteLight;

    // Add SHT details
    JsonObject objShtDetails = objHeaderM5Details.createNestedObject("shtDetails");
    objShtDetails["temp"] = details->temp;
    objShtDetails["rHum"] = details->rHum;

    // Add M5 Sensor details
    JsonObject objM5Details = objHeaderM5Details.createNestedObject("m5Details");
    objM5Details["ax"] = details->accX;
    objM5Details["ay"] = details->accY;
    objM5Details["az"] = details->accZ;

    // Add Other details
    JsonObject objOtherDetails = objHeaderM5Details.createNestedObject("otherDetails");
    objOtherDetails["timeCaptured"] = time;
    objOtherDetails["userId"] = userId;

    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderM5Details) + 1;
    char cHeaderM5Details [jsonSize];
    serializeJson(objHeaderM5Details, cHeaderM5Details, jsonSize);
    String strHeaderM5Details = cHeaderM5Details;
    //Serial.println(strHeaderM5Details.c_str()); // Debug print

    // Return the header as a String
    return strHeaderM5Details;
}


int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());

    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Post the headers (NO FILE)
    int httpResCode = http.GET();

    // Print the response code and message
    Serial.printf("HTTP%scode: %d\n%s\n\n", httpResCode > 0 ? " " : " ERROR ", httpResCode, http.getString().c_str());
    http.end();

    // Free resources and return response code
    return httpResCode;
}


int httpGetDataWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Post the headers (NO FILE)
    int httpResCode = http.GET();
    // Print the response code and message
	curMessage = http.getString();
    Serial.printf("HTTP%scode: %d\n%s\n\n", httpResCode > 0 ? " " : " ERROR ", httpResCode, curMessage.c_str());
    http.end();
    // Free resources and return response code
    
    return httpResCode;
}


String writeDataToFile(byte * fileData, size_t fileSizeInBytes) {
    // Print status
    Serial.println("Attempting to write file to SD Card...");

    // Obtain file system from SD card
    fs::FS &sdFileSys = SD;

    // Generate path where new picture will be saved on SD card and open file
    int fileNumber = getNextFileNumFromEEPROM();
    String path = "/file_" + String(fileNumber) + ".txt";
    File file = sdFileSys.open(path.c_str(), FILE_WRITE);

    // If file was opened successfully
    if (file) {
        // Write image bytes to the file
        Serial.printf("\tSTATUS: %s FILE successfully OPENED\n", path.c_str());
        file.write(fileData, fileSizeInBytes);
        Serial.printf("\tSTATUS: %s File successfully WRITTEN (%d bytes)\n\n", path.c_str(), fileSizeInBytes);

        // Update picture number
        EEPROM.write(0, fileNumber);
        EEPROM.commit();
    }
    else {
        Serial.printf("\t***ERROR: %s file FAILED OPEN in writing mode\n***", path.c_str());
        return "";
    }

    // Close file
    file.close();

    // Return file name
    return path;
}


int getNextFileNumFromEEPROM() {
    #define EEPROM_SIZE 1
    EEPROM.begin(EEPROM_SIZE);
    int fileNumber = 0;               // Init to 0 in case read fails
    fileNumber = EEPROM.read(0) + 1;
    return fileNumber;
}


double convertFintoC(double f) { return (f - 32) * 5.0 / 9.0; }
double convertCintoF(double c) { return (c * 9.0 / 5.0) + 32; }


void drawSensorDisplay() {

    M5.Lcd.fillScreen(TFT_WHITE);
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(3);

    int pad = 12;
    M5.Lcd.setCursor(pad, pad);

    M5.Lcd.print("LIVE DATA!\n");

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, M5.Lcd.getCursorY());

    M5.Lcd.printf("\n Prox: %d\n Amb Light: %d\n White Light: %d\n Hum: %.2f %%\n Temp: %.2f C\n AccX: %.2f\n AccY: %.2f\n AccZ: %.2f\n",
    prox, ambientLight, whiteLight, rHum.relative_humidity, temp.temperature, accX, accY, accZ);

    M5.Lcd.setCursor(0, M5.Lcd.getCursorY());

    M5.Lcd.printf(" User: %s\n\n", userId);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());

    M5.Lcd.setTextSize(3);

    int hours = timeClient.getHours() - 17;
    if (hours < 0) {
        hours += 24;
    }
        
    M5.Lcd.printf("%02d:%02d:%02d %s\n", hours % 12, timeClient.getMinutes(), timeClient.getSeconds(), hours < 12 ? "AM" : "PM");
}


void fetchButtons() {

    M5.Lcd.fillScreen(TFT_WHITE);
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(1);

    M5.Lcd.fillRect(10, 10, 80, 40, user == "NickDiSanto" ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(10, 70, 80, 40, user == "gavin" ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(10, 130, 80, 40, user == "all" ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(110, 10, 80, 40, duration == 5 ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(110, 70, 80, 40, duration == 30 ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(110, 130, 80, 40, duration == 120 ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(210, 10, 80, 40, dataType == "Temp" ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(210, 70, 80, 40, dataType == "Humid" ? TFT_GREEN : TFT_ORANGE);
    M5.Lcd.fillRect(210, 130, 80, 40, dataType == "Lux" ? TFT_GREEN : TFT_ORANGE);

    M5.Lcd.setCursor(19, 25);
    M5.Lcd.print("NickDiSanto");

    M5.Lcd.setCursor(36, 85);
    M5.Lcd.print("Gavin");

    M5.Lcd.setCursor(41, 145);
    M5.Lcd.print("All");

    M5.Lcd.setCursor(136, 25);
    M5.Lcd.print("5 sec");

    M5.Lcd.setCursor(131, 85);
    M5.Lcd.print("30 sec");

    M5.Lcd.setCursor(128, 145);
    M5.Lcd.print("120 sec");

    M5.Lcd.setCursor(235, 25);
    M5.Lcd.print("Temp");

    M5.Lcd.setCursor(231, 85);
    M5.Lcd.print("Humid");

    M5.Lcd.setCursor(235, 145);
    M5.Lcd.print("Lux");

    M5.Lcd.setTextSize(2);

    M5.Lcd.setCursor(35, 202);
    M5.Lcd.print("Btn B to Get Average");

    if (wasPressed(0, 0, 100, 60, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        user = "NickDiSanto";
    }
    else if (wasPressed(0, 61, 100, 120, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        user = "gavin";
    }
    else if (wasPressed(0, 121, 100, 180, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        user = "all";
    }

    else if (wasPressed(200, 60, 101, 0, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        duration = 5;
    }
    else if (wasPressed(200, 120, 101, 61, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        duration = 30;
    }
    else if (wasPressed(200, 180, 101, 121, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        duration = 120;
    }

    else if (wasPressed(300, 60, 201, 0, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        dataType = "Temp";
    }
    else if (wasPressed(300, 120, 201, 61, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        dataType = "Humid";
    }
    else if (wasPressed(300, 180, 201, 121, M5.Touch.getPressPoint())) {
        M5.Spk.DingDong();
        dataType = "Lux";
    }
}


void displayResults() {

    M5.Lcd.fillScreen(TFT_WHITE);
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(2);

    int pad = 10;
    M5.Lcd.setCursor(pad, pad);

    M5.Lcd.setTextSize(2);
    M5.Lcd.println(curMessage.substring(0, 17).c_str());

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 10);
    
    // TODO: Add actual data here
    M5.Lcd.println(curMessage.substring(19, 46).c_str());

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 10);

    // M5.Lcd.printf("Range: %02d:%02d:%02d%s — %02d:%02d:%02d%s", )
    M5.Lcd.println(curMessage.substring(45, 69).c_str());

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 10);
    M5.Lcd.println(curMessage.substring(70, 89).c_str());
    M5.Lcd.println(curMessage.substring(91, 108).c_str());

}