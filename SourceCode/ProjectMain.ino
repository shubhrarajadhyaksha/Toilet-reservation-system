/*
  ESP 32 GoT Source Code
  Written By: Vincent Ibanez for
  COEN 315 Game of Thrones Project
*/

#include <WiFi.h> //test for local
#include <WiFiAP.h> //test for local
#include <WiFiClient.h>
#include <WebServer.h>
#include <string.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

//Device Mode
//Select Device Type, set to 1, set others to 0.
#define DEVICE_BD  1
#define DEVICE_TP  0
#define DEVICE_AF  0

//WiFi Mode
// AP mode for testing, Client mode for production
#define WIFI_MODE_CLIENT 1
#define WIFI_MODE_AP     0



//TP Pin Assignments
#define TPsensor 27
#define TPled 5
#define TPbutton 26

//AF Pin Assignment
#define AFbutton 16 // Pin connected to the pushbutton (AF Button)
#define AFled 5 // Pin connected to the pushbutton (AF Button)
#define SERV1 17 // Pin connected to the Servomotor
Servo s1;

//BD Pin Assignments
#define BDLbutton 4
#define BDRbutton 2
#define BDlockSensor 0


//OLED Screen Parameters
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

IPAddress myIP;

//Client Wifi Credentials - Where the ESP32 connects to.
const char* client_ssid = "Pedrobasuraman";
const char* client_password = "tuknets12";

//AP Credentials - For testing
const char *ssid = "vibanez";
const char *password = "";

//shared variables/ status flags
char bd_uname[9] = "USERNAME";

int bd_rsrvd    = 0;
int bd_drlck    = 0;
int bd_clnrqst  = 0;
int bd_mtnrqst  = 0;

int tp_lvlok    = 0;
int tp_rqst     = 0;

int RbuttonPress = 0;
int LbuttonPress = 0;
int TPbuttonPress = 0;
int AFbuttonPress = 0;

int af_launch  = 0;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
// Variables below capture the last time the input pin was toggled
unsigned long lastDebounceTimeTPbutton = 0;
unsigned long lastDebounceTimeLbutton = 0;
unsigned long lastDebounceTimeRbutton = 0;
unsigned long lastDebounceTimeBDsensor = 0;
unsigned long lastDebounceTimeAFbutton = 0;

// the debounce time threshold; increase if the output flickers
unsigned long debounceDelay = 200;

//Mutual Exclusion Variables, to ensure no race condition
portMUX_TYPE mux_TPButton = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux_LButton = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux_RButton = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux_BDsensor = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mux_AFButton = portMUX_INITIALIZER_UNLOCKED;


//Triggers the Air Freshener Spray in the AF Module
void AFlaunchServo (void) {
    delay(100);
    s1.attach(SERV1);
    s1.write(30);
    delay(200);
    s1.write(20);
    delay(100);
    s1.detach();
    delay(3000);
    digitalWrite(AFled, LOW);
}

//Initiallize the display graphics parameters
void displaySetup() {
    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(WHITE);
    display.setCursor(10, 0);
    // Scroll in various directions, pausing in-between:
    // This helps so the OLED display does not cause burn-in
    display.startscrollright(0x00, 0x0F);
}

//Write string to the OLED display
void displaytext(char disp_name[10], char disp_name2[10]) {
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(WHITE);
    display.setCursor(10, 0);
    display.println(F(disp_name));
    display.setCursor(10, 20);
    display.println(F(disp_name2));
    display.display(); // Show initial text
    delay(100);
    // Scroll in various directions, pausing in-between:
    // This helps so the OLED display does not cause burn-in
    display.startscrollright(0x00, 0x0F);
    delay(1000);
}

//Clear the display
void stopdisplaytext() {
    display.stopscroll();
    display.clearDisplay();
    display.clearDisplay();
}

//The next functions are the interrupt handlers to allow asynchronously registering a button press
//TP Button Interrupt handler
void IRAM_ATTR handleTPbuttonPress() {
    if ((millis() - lastDebounceTimeTPbutton) > debounceDelay) {
        portENTER_CRITICAL_ISR(&mux_TPButton);
        TPbuttonPress = 1;
        portEXIT_CRITICAL_ISR(&mux_TPButton);
    }
    //reset debounce time
    lastDebounceTimeTPbutton = millis();
}

//AF Button Interrupt handler
void IRAM_ATTR handleAFbuttonPress() {
    if ((millis() - lastDebounceTimeAFbutton) > debounceDelay) {
        digitalWrite(AFled, HIGH);
        portENTER_CRITICAL_ISR(&mux_AFButton);
        AFbuttonPress = 1;
        portEXIT_CRITICAL_ISR(&mux_AFButton);
    }

    //reset debounce time
    lastDebounceTimeAFbutton = millis();
}

//BD L Button Interrupt handler
void IRAM_ATTR handleLbuttonPress() {
    if ((millis() - lastDebounceTimeLbutton) > debounceDelay) {
        portENTER_CRITICAL_ISR(&mux_LButton);
        LbuttonPress = 1;
        portEXIT_CRITICAL_ISR(&mux_LButton);
    }

    //reset debounce time
    lastDebounceTimeLbutton = millis();
}

//BD R Button Interrupt handler
void IRAM_ATTR handleRbuttonPress() {
    if ((millis() - lastDebounceTimeRbutton) > debounceDelay) {
        portENTER_CRITICAL_ISR(&mux_RButton);
        RbuttonPress = 1;
        portEXIT_CRITICAL_ISR(&mux_RButton);
    }

    //reset debounce time
    lastDebounceTimeRbutton = millis();
}

//The next functions are the API calls that allows communicating actions to the IOT server
//BD Update Status Function
void APIgetBDStat() {
    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/status";

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        String line;
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');

        client.readStringUntil(':');
        line = client.readStringUntil(',');
        bd_rsrvd = line.toInt();

        client.readStringUntil(':');
        line = client.readStringUntil(',');
        strncpy ( bd_uname, &line[0], 8 );

        client.readStringUntil(',');

        client.readStringUntil(',');

        client.readStringUntil(',');

        client.readStringUntil(':');
        line = client.readStringUntil(',');
        bd_clnrqst = line.toInt();

        client.readStringUntil(':');
        line = client.readStringUntil('\r');
        bd_mtnrqst = line.toInt();
    }
}

//BD Cleaning Request Function
void APImainbdClnrqst() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/clnrqst";

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//BD Cleaning Clear Request Function
void APImainbdClnClr() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/clnclr";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//BD Maintenance Request Function
void APImainbdMntcRqst() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/mntcrqst";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//BD Maintenance Clear Request Function
void APImainbdMntcClr() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/mntcclr";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//BD Door Lock Function
void APImainbdDrlck() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/drlck";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//BD Door Unlock Function
void APImainbdDrunlck() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/mainbd/drunlck";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        String line;
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');

        client.readStringUntil(':');
        line = client.readStringUntil(',');
        bd_rsrvd = line.toInt();

        client.readStringUntil(':');
        line = client.readStringUntil(',');
        strncpy ( bd_uname, &line[0], 8 );

        client.readStringUntil(',');

        client.readStringUntil(',');

        client.readStringUntil(',');

        client.readStringUntil(':');
        line = client.readStringUntil(',');
        bd_clnrqst = line.toInt();

        client.readStringUntil(':');
        line = client.readStringUntil('\r');
        bd_mtnrqst = line.toInt();
    }
}

//TP Update Status Function
void APIgetTPStat() {
    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/tp/status";

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        String line;
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil(':');
        line = client.readStringUntil(',');
        tp_lvlok = line.toInt();
        client.readStringUntil(':');
        line = client.readStringUntil('\r');
        tp_rqst = line.toInt();
    }
}

//TP Level OK Send Function
void APItplvlokset() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/tp/lvlokset";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//TP Level Not OK Send Function
void APItplvlokreset() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/tp/lvlokreset";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//TP Request Function
void APITPRqstSt() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/tp/setrqst";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//TP Clear Request Function
void APITPRqstRst() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/tp/clrrqst";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}

//AF Update Status Function
void APIgetSprayStat() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/af/sprayStat";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil('\r');
        client.readStringUntil(':');
        String line = client.readStringUntil('\r');
        Serial.println(line);
        af_launch = line.toInt();
    }
    if (af_launch == 1) digitalWrite(AFled, HIGH);

}

//AF Clear Spray Flag Function
void APIclearSpray() {
    Serial.print("connecting to ");
    Serial.println("www.vibanez.com");

    WiFiClient client;
    const int httpPort = 8071;
    if (!client.connect("www.vibanez.com", httpPort)) {
        Serial.println("connection failed");
        return;
    }

    String url = "/af/sprayClr";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: www.vibanez.com\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 1000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server
    // Extract response
    while(client.available()) {
        client.readStringUntil('\r');
    }
}


// Device Setup Function
void setup()
{
    //Set Serial Speed
    Serial.begin(115200);
    //Set I2C Clock to a slow speed in order to properly address I2C Addressing issues
    Wire.setClock(10000);

    // Setup the Access Point - Test Mode Only
    if (WIFI_MODE_AP == 1) {
        Serial.println("Configuring access point...");
        WiFi.softAP(ssid, password);
        myIP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(myIP);
    }

    if (WIFI_MODE_CLIENT == 1) {
        //Set WIFI Access Point Mode
        WiFi.mode(WIFI_STA);

        //Client Setup - Connect to a Wifi Network
        Serial.println();
        Serial.println("ESP32: Client Connecting to ");
        Serial.println(client_ssid);
        WiFi.begin(client_ssid, client_password);

        //Wait until connected
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        myIP = WiFi.localIP();

        Serial.println();
        Serial.println("ESP32: Client WiFi connected");
        Serial.println("ESP32: Client IP address: ");
        Serial.println(myIP);
    }

    //Run the BD Device Specific Setup
    if (DEVICE_BD == 1) {
        //Set Buttons
        pinMode(BDLbutton, INPUT);
        pinMode(BDRbutton, INPUT);
        pinMode(BDlockSensor, INPUT);
        attachInterrupt(digitalPinToInterrupt(BDLbutton), handleLbuttonPress, FALLING);
        attachInterrupt(digitalPinToInterrupt(BDRbutton), handleRbuttonPress, FALLING);
        Serial.println("ESP32: Monitoring BD interrupts");

        // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
        if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
            Serial.println(F("SSD1306 allocation failed"));
            for(;;); // Don't proceed, loop forever
        }

        //Clear the display after initializing
        stopdisplaytext();
    }

    //Run the TP Device Specific Setup
    if (DEVICE_TP == 1) {
        //Set Buttons
        pinMode(TPbutton, INPUT);
        pinMode(TPled, OUTPUT);
        pinMode(TPsensor, INPUT);
        attachInterrupt(digitalPinToInterrupt(TPbutton), handleTPbuttonPress, FALLING);
        Serial.println("ESP32: Monitoring TP interrupts");
    }

    //Run the AP Device Specific Setup
    if (DEVICE_AF == 1) {
        //Set Buttons
        pinMode(AFbutton, INPUT);
        pinMode(AFled, OUTPUT);
        attachInterrupt(digitalPinToInterrupt(AFbutton), handleAFbuttonPress, FALLING);
        Serial.println("ESP32: Monitoring AF interrupts");
    }

    Serial.println("ESP32: got_demo ready");
}

//This is the Main loop logic
void loop(void) {

    //Run the BD Device Specific Logic
    if (DEVICE_BD ==1) {
        //Service button press first
        if(RbuttonPress != 0) {
            portENTER_CRITICAL(&mux_RButton);
            RbuttonPress = 0; //handle the interrupt
            portEXIT_CRITICAL(&mux_RButton);
            stopdisplaytext();

            if (bd_mtnrqst == 0) {
                bd_mtnrqst = 1; //Toggle status flag
                APImainbdMntcRqst();
                displaytext("MAINTENCE","REQUESTED");
                Serial.print("Maintenance Requested! ");
            }
            else {
                bd_mtnrqst = 0; //Toggle status flag
                APImainbdMntcClr();
                displaytext("MAINTENCE","CANCELLED");
                Serial.print("Maintenance Request Cancelled! ");
            }
        }

        if(LbuttonPress != 0) {
            portENTER_CRITICAL(&mux_LButton);
            LbuttonPress = 0; //handle the interrupt
            portEXIT_CRITICAL(&mux_LButton);
            stopdisplaytext();

            if (bd_clnrqst == 0) {
                bd_clnrqst = 1; //Toggle status flag
                APImainbdClnrqst();
                displaytext("CLEANING","REQUESTED");
                Serial.print("Cleaning Requested! ");
            }
            else {
                bd_clnrqst = 0; //Toggle status flag
                APImainbdClnClr();
                displaytext("CLEANING","CANCELLED");
                Serial.print("Cleaning Request Cancelled! ");
            }
        }

        // Check lock status next
        if (digitalRead(BDlockSensor) == bd_drlck) // Status change detected
            if (digitalRead(BDlockSensor) == 0) {
                bd_drlck = 1; //Toggle status flag
                APImainbdDrlck();
            } else {
                bd_drlck = 0; //Toggle status flag
                APImainbdDrunlck();
        }

        stopdisplaytext();
        APIgetBDStat();
        if (bd_mtnrqst == 1) {
            //Display out of order
            displaytext("Out of","Order :(");
        }
        else if (bd_drlck == 1) {
            displaytext("Stall is","OCCUPIED");
        }
        else { //(bd_drlck == 0) //Unlocked
            if (bd_rsrvd == 1) {
                //Display Username of next user
                displaytext("Waiting 4",bd_uname);
            }
            else {
                //Display stall available
                displaytext("Stall is","AVAILABLE");
            }
        }
    }

    //Run the TP Device Specific Logic
    if (DEVICE_TP == 1) {

        //Service button press first
        if(TPbuttonPress != 0) {
            portENTER_CRITICAL(&mux_TPButton);
            TPbuttonPress = 0; //handle the interrupt
            portEXIT_CRITICAL(&mux_TPButton);

            if (tp_rqst == 0) {
                tp_rqst = 1; //Toggle status flag
                APITPRqstSt();
                Serial.print("Cleaning Requested! ");
            }
            else {
                tp_rqst = 0; //Toggle status flag
                APITPRqstRst();
                Serial.print("Cleaning Request Cancelled! ");
            }
        }

        // Check TP Level next
        if (digitalRead(TPsensor) != tp_lvlok) { //changed
            if (digitalRead(TPsensor) == 0) { //lowered position
                tp_lvlok = 0;
                APItplvlokreset();

            } else { //Normal "OK" position
                tp_lvlok = 1;
                APItplvlokset();
            }
        }

        //Logic for TP LED Status
        if (tp_lvlok == 0)
            //Solid TP LED
            digitalWrite(TPled, HIGH);
        else if (tp_rqst == 1) {
            //blinking TP LED
            digitalWrite(TPled, HIGH);
            delay(200);
            digitalWrite(TPled, LOW);
            delay(400);
        } else
            digitalWrite(TPled, LOW);

        //TP update status
        APIgetTPStat();
    }

    //Run the AF Device Specific Logic
    if (DEVICE_AF == 1) {

        //Poll the server if a spray request was generated
        APIgetSprayStat();

        //Service button press first
        if(AFbuttonPress  != 0) {
            portENTER_CRITICAL(&mux_AFButton);
            AFbuttonPress = 0;
            portEXIT_CRITICAL(&mux_AFButton);
            Serial.println("Spray the goodness! ");
            AFlaunchServo();
        }

        //Next Service spray request from server
        if(af_launch  != 0) {
            Serial.println("Spray the goodness! ");
            AFlaunchServo();

            //Clear server request flag
            APIclearSpray();
        }
    }

}