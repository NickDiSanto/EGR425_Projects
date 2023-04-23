#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"
#include <I2C_RW.h>

////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////
// TODO 3: Register for openweather account and get API key
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "1782fc98b6542031d19c97e30fc607e5";

// TODO 1: WiFi variables
String wifiNetworkName = "Dumbledores_Office";
String wifiPassword = "Lemon_Drops";

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;  // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)

// LCD variables
int sWidth;
int sHeight;

// Weather/zip variables
String strWeatherIcon;
String strWeatherDesc;
String cityName;
double tempNow;
double tempMin;
double tempMax;

static String units = "imperial";
static String unit = "F";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
static String screen = "weather";
static int zipNums[] = {9, 2, 5, 0, 4};
static int zipCurrentElement = 0;

const int PIN_SDA = 32;
const int PIN_SCL = 33;
const int I2C_FREQ = 400000;

long ts;
int prox = 0;
int als = 100;
float* liveTempHumidity;

///////////////////////////////////////////////////////////////
// Register defines
///////////////////////////////////////////////////////////////
#define VCNL_I2C_ADDRESS 0x60
#define VCNL_REG_PROX_DATA 0x08
#define VCNL_REG_ALS_DATA 0x09
#define VCNL_REG_WHITE_DATA 0x0A

#define VCNL_REG_PS_CONFIG 0x03
#define VCNL_REG_ALS_CONFIG 0x00
#define VCNL_REG_WHITE_CONFIG 0x04

#define SHT_TEMP_HUMID_CONFIG 0x44
#define SHT_TEMP_HUMID_ADDRESS 0xFD


bool wasPressed(int x1,int y1,int x2,int y2, Point p){
    bool betweenX = (x1 > p.x && x2 < p.x) || (x1 < p.x && x2 > p.x);
    bool betweenY = (y1 > p.y && y2 < p.y) || (x1 < p.y && y2 > p.y);
    return betweenX && betweenY;
  }

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
String httpGETRequest(const char* serverName);
void drawWeatherImage(String iconId, int resizeMult);
void drawZipFetcher();
void fetchWeatherDetails();
void drawWeatherDisplay();
void drawHumidityAndTempDisplay();

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup() {
    // Initialize the device
    M5.begin();
    
    // Set screen orientation and get height/width 
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();
    M5.Lcd.setBrightness(0);

    // // Connect to VCNL/SHT sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, SHT_TEMP_HUMID_CONFIG, I2C_FREQ, PIN_SDA, PIN_SCL);

    // // Write registers to initialize/enable VCNL sensors
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_WHITE_CONFIG, 2, 0x0000, " to enable raw white light sensor", true);

    delay(10);

    // TODO 2: Connect to WiFi
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n\nConnected to WiFi network with IP address:", wifiNetworkName.c_str());
    Serial.println(WiFi.localIP());

    timeClient.begin();
    timeClient.setTimeOffset(-28800);
    ts=millis();
}
///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() {
    M5.update();
    timeClient.update();

    if (M5.BtnA.isPressed()) {
        M5.Spk.DingDong();
        
        if (units == "imperial") {
            units = "metric";
            unit = "C";
        } else {
            units = "imperial";
            unit = "F";
        }
    }

    // Screen is weather display
    if (screen == "weather") {
        if (M5.BtnB.isPressed()) {
            M5.Spk.DingDong();
            screen = "zip";
        }

        if (M5.BtnC.isPressed()) {
            M5.Spk.DingDong();
            screen = "humidity";
        }

        // Only execute every so often
        if ((millis() - lastTime) > timerDelay) {
            if (WiFi.status() == WL_CONNECTED) {

                fetchWeatherDetails();
                drawWeatherDisplay();
                
            } else {
                Serial.println("WiFi Disconnected");
            }

            // Update the last time to NOW
            lastTime = millis();
        }

    // Screen is zip selector
    } else if (screen == "zip") {
        if (M5.BtnC.isPressed()) {
            M5.Spk.DingDong();
            screen = "weather";
        }
        // first
        if (wasPressed(65, 110, 25, 60, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[0] < 9) {
                zipNums[0]++;
            } else {
                zipNums[0] = 0;
            }
        }

        if (wasPressed(65, 135, 25, 185, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[0] > 0) {
                zipNums[0]--;
            } else {
                zipNums[0] = 9;
            }
        }
        // second
        if (wasPressed(70, 110, 105, 70, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[1] < 9) {
                zipNums[1]++;
            } else {
                zipNums[1] = 0;
            }
        }

        if (wasPressed(70, 135, 105, 185, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[1] > 0) {
                zipNums[1]--;
            } else {
                zipNums[1] = 9;
            }
        }

        // third
        if (wasPressed(135, 110, 175, 60, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[2] < 9) {
                zipNums[2]++;
            } else {
                zipNums[2] = 0;
            }
        }

        if (wasPressed(135, 135, 175, 185, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[2] > 0) {
                zipNums[2]--;
            } else {
                zipNums[2] = 9;
            }
        }

        // fourth
        if (wasPressed(195, 110, 235, 60, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[3] < 9) {
                zipNums[3]++;
            } else {
                zipNums[3] = 0;
            }
        }

        if (wasPressed(195, 135, 235, 225, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[3] > 0) {
                zipNums[3]--;
            } else {
                zipNums[3] = 9;
            }
        }

        // fifth
        if (wasPressed(255, 110, 295, 60, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[4] < 9) {
                zipNums[4]++;
            } else {
                zipNums[4] = 0;
            }
        }

        if (wasPressed(255, 135, 295, 225, M5.Touch.getPressPoint())) {
            M5.Spk.DingDong();
            if (zipNums[4] > 0) {
                zipNums[4]--;
            } else {
                zipNums[4] = 9;
            }
        }

        // Only execute every so often
        if ((millis() - lastTime) > timerDelay) {
            if (WiFi.status() == WL_CONNECTED) {

                drawZipFetcher();
                
            } else {
                Serial.println("WiFi Disconnected");
            }

            // Update the last time to NOW
            lastTime = millis();
        }

    // Screen is live humidity/temp
    } else {
        if (M5.BtnC.isPressed()) {
            M5.Spk.DingDong();
            screen = "weather";
        }

        // Only execute every so often
        if ((millis() - lastTime) > timerDelay) {
            if (WiFi.status() == WL_CONNECTED) {

                drawHumidityAndTempDisplay();
                
            } else {
                Serial.println("WiFi Disconnected");
            }

            // Update the last time to NOW
            lastTime = millis();
        }
    }

    if(millis() >= ts+100){
        ts=millis();

        // // I2C call to read sensor proximity data and print
        prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
        
        // I2C call to read sensor ambient light data and print
        als = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
        als = als * 0.1; // See pg 12 of datasheet - we are using ALS_IT (7:6)=(0,0)=80ms integration time = 0.10 lux/step for a max range of 6553.5 lux        
        // Serial.printf("als: %i\n\n",als);


        // I2C call to read temp/humidity and print
        liveTempHumidity = I2C_RW::readTempHumidityData(SHT_TEMP_HUMID_ADDRESS, 6, "to read temp/humidity data", true);
    }

    if(prox>30){
        // M5.Axp.SetLcdVoltage(2500);
        M5.Lcd.fillScreen(BLACK);
    }else{
        if(als>75){
            M5.Axp.SetLcdVoltage(3300);
        }
        else if(als<15){
            M5.Axp.SetLcdVoltage(2600);
        }else if(als>15 && als<40){
            M5.Axp.SetLcdVoltage(3000);
        }
    }
    
}


/////////////////////////////////////////////////////////////////
// This method fetches the weather details from the OpenWeather
// API and saves them into the fields defined above
/////////////////////////////////////////////////////////////////
void fetchWeatherDetails() {
    //////////////////////////////////////////////////////////////////
    // Hardcode the specific city,state,country into the query
    Examples: https://api.openweathermap.org/data/2.5/weather?q=riverside,ca,usa&units=imperial&appid=YOUR_API_KEY
    //////////////////////////////////////////////////////////////////
    String serverURL = urlOpenWeather + "zip=" + zipNums[0] + zipNums[1] + zipNums[2] + zipNums[3] + zipNums[4] + ",us&units=" + units + "&appid=" + apiKey;
    // String serverURL = urlOpenWeather + "q=riverside,ca,usa&units=" + units + "&appid=" + apiKey;
    //Serial.println(serverURL); // Debug print

    //////////////////////////////////////////////////////////////////
    // Make GET request and store reponse
    //////////////////////////////////////////////////////////////////
    String response = httpGETRequest(serverURL.c_str());
    //Serial.print(response); // Debug print
    
    //////////////////////////////////////////////////////////////////
    // Import ArduinoJSON Library and then use arduinojson.org/v6/assistant to
    // compute the proper capacity (this is a weird library thing) and initialize
    // the json object
    //////////////////////////////////////////////////////////////////
    const size_t jsonCapacity = 768+250;
    DynamicJsonDocument objResponse(jsonCapacity);

    //////////////////////////////////////////////////////////////////
    // Deserialize the JSON document and test if parsing succeeded
    //////////////////////////////////////////////////////////////////
    DeserializationError error = deserializeJson(objResponse, response);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    serializeJsonPretty(objResponse, Serial); // Debug print

    //////////////////////////////////////////////////////////////////
    // Parse Response to get the weather description and icon
    //////////////////////////////////////////////////////////////////
    JsonArray arrWeather = objResponse["weather"];
    JsonObject objWeather0 = arrWeather[0];
    String desc = objWeather0["main"];
    String icon = objWeather0["icon"];
    String city = objResponse["name"];

    // ArduionJson library will not let us save directly to these
    // variables in the 3 lines above for unknown reason
    strWeatherDesc = desc;
    strWeatherIcon = icon;
    cityName = city;

    // Parse response to get the temperatures
    JsonObject objMain = objResponse["main"];
    tempNow = objMain["temp"];
    tempMin = objMain["temp_min"];
    tempMax = objMain["temp_max"];

    // Serial.printf("NOW: %.1f %s and %s\tMIN: %.1f %s\tMax: %.1f %s\n", tempNow, unit, strWeatherDesc, tempMin, unit, tempMax, unit);
}

/////////////////////////////////////////////////////////////////
// Update the display based on the weather variables defined
// at the top of the screen.
/////////////////////////////////////////////////////////////////
void drawWeatherDisplay() {
    //////////////////////////////////////////////////////////////////
    // Draw background - light blue if day time and navy blue of night
    //////////////////////////////////////////////////////////////////
    uint16_t primaryTextColor;
    if (strWeatherIcon.indexOf("d") >= 0) {
        M5.Lcd.fillScreen(TFT_CYAN);
        primaryTextColor = TFT_DARKGREY;
    } else {
        M5.Lcd.fillScreen(TFT_NAVY);
        primaryTextColor = TFT_WHITE;
    }
    
    //////////////////////////////////////////////////////////////////
    // Draw the icon on the right side of the screen - the built in 
    // drawBitmap method works, but we cannot scale up the image
    // size well, so we'll call our own method
    //////////////////////////////////////////////////////////////////
    //M5.Lcd.drawBitmap(0, 0, 100, 100, myBitmap, TFT_BLACK);
    drawWeatherImage(strWeatherIcon, 3);
    
    //////////////////////////////////////////////////////////////////
    // Draw the temperatures and city name
    //////////////////////////////////////////////////////////////////
    int pad = 10;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("LO:%0.f%s\n", tempMin, unit);
    
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(10);
    if((tempNow>65.0 && unit=="F") || (tempNow>18.0 && unit=="C") ){
      M5.Lcd.setTextColor(ORANGE);
    }else if((tempNow<50.0 && unit=="F") || (tempNow<10.0 && unit=="C")){
      M5.Lcd.setTextColor(primaryTextColor);
    }
    M5.Lcd.printf("%0.f%s\n", tempNow, unit);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("HI:%0.f%s\n", tempMax, unit);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    if(cityName.length()>7){
      M5.Lcd.setTextSize(2);
    }
    if(cityName.length()>10){
      M5.Lcd.setTextSize(1);
    }
    M5.Lcd.printf("%s\n\n", cityName.c_str());

    M5.Lcd.setTextColor(TFT_PINK);
    M5.Lcd.setTextSize(4);
    String time = timeClient.getFormattedTime();
    String amPm = "AM";
    
    if (time.substring(0, 2).toInt() > 12) {
        time = time.substring(0, 2).toInt() - 12 + time.substring(time.length() - 6);
        amPm = "PM";
    }
    M5.Lcd.print(time + " " + amPm);
}

/////////////////////////////////////////////////////////////////
// This method takes in a URL and makes a GET request to the
// URL, returning the response.
/////////////////////////////////////////////////////////////////
String httpGETRequest(const char* serverURL) {
    
    // Initialize client
    HTTPClient http;
    http.begin(serverURL);

    // Send HTTP GET request and obtain response
    int httpResponseCode = http.GET();
    String response = http.getString();

    // Check if got an error
    if (httpResponseCode > 0)
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    else {
        Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
        Serial.printf("Server Response: %s\n", response);
    }

    // Free resources and return response
    http.end();
    return response;
}

/////////////////////////////////////////////////////////////////
// This method takes in an image icon string (from API) and a 
// resize multiple and draws the corresponding image (bitmap byte
// arrays found in EGR425_Phase1_weather_bitmap_images.h) to scale (for 
// example, if resizeMult==2, will draw the image as 200x200 instead
// of the native 100x100 pixels) on the right-hand side of the
// screen (centered vertically). 
/////////////////////////////////////////////////////////////////
void drawWeatherImage(String iconId, int resizeMult) {

    // Get the corresponding byte array
    const uint16_t * weatherBitmap = getWeatherBitmap(iconId);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
    int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
    //int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally
    
    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++) {
        for (int x = 0; x < imgSqDim; x++) {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = weatherBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0) {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++) {
                    for (int j = 0; j < resizeMult; j++) {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}

void drawZipFetcher() {
    M5.Touch.begin();
    M5.Lcd.fillScreen(TFT_YELLOW);
    M5.Lcd.setTextColor(TFT_NAVY);
    M5.Lcd.setTextSize(6);

    // GAVIN TODO: Should include little icons that will change the numbers if touched

    int pad = 10;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.print("Set ZIP:\n\n");
   
    // dec 1
    M5.Lcd.drawLine(35, 155, 42, 165, TFT_NAVY);
    M5.Lcd.drawLine(49, 155, 42, 165, TFT_NAVY);
    // inc 1
    M5.Lcd.drawLine(35, 90, 42, 80, TFT_NAVY);
    M5.Lcd.drawLine(49, 90, 42, 80, TFT_NAVY);

    // dec 2
    M5.Lcd.drawLine(88, 155, 95, 165, TFT_NAVY);
    M5.Lcd.drawLine(102, 155, 95, 165, TFT_NAVY);
    // inc 2
    M5.Lcd.drawLine(88, 90, 95, 80, TFT_NAVY);
    M5.Lcd.drawLine(102, 90, 95, 80, TFT_NAVY);

    // dec 3
    M5.Lcd.drawLine(152, 155, 159, 165, TFT_NAVY);
    M5.Lcd.drawLine(166, 155, 159, 165, TFT_NAVY);
    // inc 3
    M5.Lcd.drawLine(152, 90, 159, 80, TFT_NAVY);
    M5.Lcd.drawLine(166, 90, 159, 80, TFT_NAVY);

    // dec 4
    M5.Lcd.drawLine(212, 155, 219, 165, TFT_NAVY);
    M5.Lcd.drawLine(226, 155, 219, 165, TFT_NAVY);
    // inc 4
    M5.Lcd.drawLine(212, 90, 219, 80, TFT_NAVY);
    M5.Lcd.drawLine(226, 90, 219, 80, TFT_NAVY);

    // dec 5
    M5.Lcd.drawLine(272, 155, 279, 165, TFT_NAVY);
    M5.Lcd.drawLine(286, 155, 279, 165, TFT_NAVY);
    // inc 4
    M5.Lcd.drawLine(272, 90, 279, 80, TFT_NAVY);
    M5.Lcd.drawLine(286, 90, 279, 80, TFT_NAVY);

    M5.Lcd.setTextSize(5);
    M5.Lcd.printf(" %d %d %d %d %d\n", zipNums[0], zipNums[1], zipNums[2], zipNums[3], zipNums[4]);
}

/////////////////////////////////////////////////////////////////
// Update the display based on the weather variables defined
// at the top of the screen.
/////////////////////////////////////////////////////////////////
void drawHumidityAndTempDisplay() {
    M5.Lcd.fillScreen(TFT_DARKGREEN);
    // M5.Lcd.setTextSize(6);

    int pad = 10;
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.setTextSize(3);
    // TODO: Replace with humidity data
    // M5.Lcd.printf("Humidity:%0.f%s\n", tempMin, unit);
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    // M5.Lcd.setTextSize(10);
    if(liveTempHumidity[0]>18.0){
      M5.Lcd.setTextColor(ORANGE);
    }else if(liveTempHumidity[0]<10.0 && unit=="C"){
      M5.Lcd.setTextColor(CYAN);
    }

    if (unit == "F") {
        liveTempHumidity[0] = (liveTempHumidity[0]*1.8) + 32;
    }
    M5.Lcd.printf("Temp read:%0.f%s\n\n", liveTempHumidity[0], unit);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_RED);

    M5.Lcd.printf("Humidity read:%0.f%% \n\n", liveTempHumidity[1]);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_PINK);
    M5.Lcd.setTextSize(4);

    M5.Lcd.print("LIVE DATA!\n\n");

    String time = timeClient.getFormattedTime();
    String amPm = "AM";

    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setTextSize(3);
    
    if (time.substring(0, 2).toInt() > 12) {
        time = time.substring(0, 2).toInt() - 12 + time.substring(time.length() - 6);
        amPm = "PM";
    }
    M5.Lcd.print(" Last reading:\n " + time + " " + amPm);
}

//////////////////////////////////////////////////////////////////////////////////
// For more documentation see the following links:
// https://github.com/m5stack/m5-docs/blob/master/docs/en/api/
// https://docs.m5stack.com/en/api/core2/lcd_api
//////////////////////////////////////////////////////////////////////////////////