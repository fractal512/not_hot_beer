#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>

#include "config.h"

// ============================================================
// ПІНИ
// ============================================================

// DS18B20
#define DS18B20_PIN 5

// Кнопки
#define BTN_DOWN 25
#define BTN_UP   26
#define BTN_STOP 32
#define BTN_START 33

// Реле
#define RELAY_LIGHT 18       // одноканальне реле світлового індикатора
#define RELAY_FRIDGE1 19     // HL-52S IN1
#define RELAY_FRIDGE2 23     // HL-52S IN2

// Вбудований світлодіод ESP32
#define BUILTIN_LED 2

// Buzzer
#define BUZZER_PIN 27

// ============================================================
// ЛОГІКА РЕЛЕ
// ============================================================

// Одноканальне реле світлового індикатора
#define LIGHT_RELAY_ON  HIGH
#define LIGHT_RELAY_OFF LOW

// HL-52S
#define FRIDGE_RELAY_ON  LOW
#define FRIDGE_RELAY_OFF HIGH

WebServer server(80);

// ============================================================
// НАЛАШТУВАННЯ
// ============================================================

// Початкова температура
float T_SET = 5.0;

// Крок зміни температури
const float TEMP_STEP = 1.0;

// Мінімальна / максимальна температура
const float TEMP_MIN = 0.0;
const float TEMP_MAX = 30.0;

// Гістерезис
// Холодильники вимикаються при T_inside <= T_SET
// Холодильники вмикаються при T_inside >= T_SET + 2°C
const float HYSTERESIS = 2.0;

// Час роботи світлового індикатора
const unsigned long LIGHT_TIME = 10000UL;

// Інтервал вимірювання температури
const unsigned long SENSOR_INTERVAL = 2000UL;

// Через скільки вимикати OLED
const unsigned long OLED_TIMEOUT = 300000UL;   // 5 хвилин

// ============================================================
// ОБ'ЄКТИ
// ============================================================

OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

// OLED 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
    U8G2_R0,
    U8X8_PIN_NONE
);

// ============================================================
// ЗМІННІ
// ============================================================

float T_INSIDE = NAN;

// Стан холодильників
bool fridgeRunning = false;

// Стан системи
bool systemStarted = false;

// Таймер світлового індикатора
unsigned long lightStartTime = 0;
bool lightRelayActive = false;

// Таймер датчика
unsigned long lastSensorRead = 0;

// ============================================================
// OLED
// ============================================================

bool oledSleeping = false;
unsigned long lastButtonActivity = 0;

// ============================================================
// START BUTTON
// ============================================================

bool startButtonWasPressed = false;
unsigned long startButtonPressTime = 0;

// ============================================================
// ПРОТОТИПИ ФУНКЦІЙ
// ============================================================

void beep();

void updateFridgeLED();

void fridgeRelayOn();
void fridgeRelayOff();

void lightRelayOn();
void lightRelayOff();

void stopEverything();
void startSystem();

void readSensors();
void controlFridges();

void wakeOLED();
void checkOLEDTimeout();

void checkButtons();

void updateLightRelay();

void updateDisplay();

String makeWebPage();

void handleRoot();

void connectWiFi();

// ============================================================
// BEEP
// ============================================================

void beep()
{
    tone(BUZZER_PIN, 2000, 100);
    delay(120);
    noTone(BUZZER_PIN);
}

// ============================================================
// ОНОВЛЕННЯ LED GPIO2
// ============================================================

void updateFridgeLED()
{
    if (fridgeRunning)
    {
        digitalWrite(BUILTIN_LED, HIGH);
    }
    else
    {
        digitalWrite(BUILTIN_LED, LOW);
    }
}

// ============================================================
// КЕРУВАННЯ РЕЛЕ ХОЛОДИЛЬНИКІВ
// ============================================================

void fridgeRelayOn()
{
    digitalWrite(RELAY_FRIDGE1, FRIDGE_RELAY_ON);
    digitalWrite(RELAY_FRIDGE2, FRIDGE_RELAY_ON);

    fridgeRunning = true;

    // Дублюємо стан холодильників на LED GPIO2
    updateFridgeLED();
}

// ------------------------------------------------------------

void fridgeRelayOff()
{
    digitalWrite(RELAY_FRIDGE1, FRIDGE_RELAY_OFF);
    digitalWrite(RELAY_FRIDGE2, FRIDGE_RELAY_OFF);

    fridgeRunning = false;

    // Дублюємо стан холодильників на LED GPIO2
    updateFridgeLED();
}

// ============================================================
// КЕРУВАННЯ СВІТЛОВИМ РЕЛЕ
// ============================================================

void lightRelayOn()
{
    digitalWrite(RELAY_LIGHT, LIGHT_RELAY_ON);

    lightRelayActive = true;
    lightStartTime = millis();
}

// ------------------------------------------------------------

void lightRelayOff()
{
    digitalWrite(RELAY_LIGHT, LIGHT_RELAY_OFF);

    lightRelayActive = false;
}

// ============================================================
// STOP
// ============================================================

void stopEverything()
{
    fridgeRelayOff();

    systemStarted = false;
}

// ============================================================
// START
// ============================================================

void startSystem()
{
    systemStarted = true;

    // Вмикаємо холодильники
    fridgeRelayOn();

    // Вмикаємо світловий індикатор на 10 секунд
    lightRelayOn();
}

// ============================================================
// ЗЧИТУВАННЯ DS18B20
// ============================================================

void readSensors()
{
    ds18b20.requestTemperatures();

    float tDS = ds18b20.getTempCByIndex(0);

    if (tDS != DEVICE_DISCONNECTED_C)
    {
        T_INSIDE = tDS;
    }
}

// ============================================================
// КЕРУВАННЯ ХОЛОДИЛЬНИКАМИ
// ============================================================

void controlFridges()
{
    // Якщо система не запущена,
    // холодильники повинні бути вимкнені
    if (!systemStarted)
    {
        fridgeRelayOff();
        return;
    }

    // Якщо температура ще не отримана
    if (isnan(T_INSIDE))
    {
        return;
    }

    // --------------------------------------------------------
    // ХОЛОДИЛЬНИКИ ПРАЦЮЮТЬ
    // --------------------------------------------------------

    if (fridgeRunning)
    {
        // Досягли заданої температури
        if (T_INSIDE <= T_SET)
        {
            fridgeRelayOff();
        }
    }

    // --------------------------------------------------------
    // ХОЛОДИЛЬНИКИ ВИМКНЕНІ
    // --------------------------------------------------------

    else
    {
        // Температура піднялась на 2 градуси
        if (T_INSIDE >= (T_SET + HYSTERESIS))
        {
            fridgeRelayOn();
        }
    }
}

// ============================================================
// OLED WAKE
// ============================================================

void wakeOLED()
{
    if (oledSleeping)
    {
        oled.setPowerSave(0);
        oledSleeping = false;

        updateDisplay();
    }

    lastButtonActivity = millis();
}

// ============================================================
// OLED SLEEP
// ============================================================

void checkOLEDTimeout()
{
    if (!oledSleeping &&
        millis() - lastButtonActivity >= OLED_TIMEOUT)
    {
        oled.setPowerSave(1);
        oledSleeping = true;
    }
}

// ============================================================
// ПЕРЕВІРКА КНОПОК
// ============================================================

void checkButtons()
{
    bool anyButtonPressed =
        digitalRead(BTN_DOWN)  == LOW ||
        digitalRead(BTN_UP)    == LOW ||
        digitalRead(BTN_STOP)  == LOW ||
        digitalRead(BTN_START) == LOW;

    // --------------------------------------------------------
    // Якщо OLED вимкнений
    // --------------------------------------------------------

    if (oledSleeping)
    {
        if (anyButtonPressed)
        {
            // Перше натискання тільки будить OLED.
            // Функція кнопки НЕ виконується.
            wakeOLED();

            delay(200);
        }

        return;
    }

    // --------------------------------------------------------
    // DOWN
    // --------------------------------------------------------

    if (digitalRead(BTN_DOWN) == LOW)
    {
        beep();

        T_SET -= TEMP_STEP;

        if (T_SET < TEMP_MIN)
        {
            T_SET = TEMP_MIN;
        }

        lastButtonActivity = millis();

        updateDisplay();

        delay(200);
    }

    // --------------------------------------------------------
    // UP
    // --------------------------------------------------------

    if (digitalRead(BTN_UP) == LOW)
    {
        beep();

        T_SET += TEMP_STEP;

        if (T_SET > TEMP_MAX)
        {
            T_SET = TEMP_MAX;
        }

        lastButtonActivity = millis();

        updateDisplay();

        delay(200);
    }

    // --------------------------------------------------------
    // STOP
    // --------------------------------------------------------

    if (digitalRead(BTN_STOP) == LOW)
    {
        beep();

        lastButtonActivity = millis();

        stopEverything();

        updateDisplay();

        delay(300);
    }

    // --------------------------------------------------------
    // START
    // --------------------------------------------------------

    if (digitalRead(BTN_START) == LOW)
    {
        beep();

        lastButtonActivity = millis();

        startSystem();

        updateDisplay();

        delay(300);
    }
}

// ============================================================
// ТАЙМЕР СВІТЛОВОГО ІНДИКАТОРА
// ============================================================

void updateLightRelay()
{
    if (!lightRelayActive)
    {
        return;
    }

    if (millis() - lightStartTime >= LIGHT_TIME)
    {
        lightRelayOff();
    }
}

// ============================================================
// OLED
// ============================================================

void updateDisplay()
{
    if (oledSleeping)
    {
        return;
    }

    oled.clearBuffer();

    // --------------------------------------------------------
    // ШРИФТ
    // --------------------------------------------------------

    oled.setFont(u8g2_font_6x10_tf);

    // ========================================================
    // ЖОВТА ОБЛАСТЬ OLED
    // ========================================================

    // IP-адреса знаходиться у верхніх пікселях дисплея.
    // Для типового двоколірного OLED 128x64
    // це жовта область.
    oled.drawStr(0, 10, "IP:");

    String ipString = WiFi.localIP().toString();

    oled.drawStr(18, 10, ipString.c_str());

    // ========================================================
    // СИНЯ ОБЛАСТЬ OLED
    // ========================================================

    // T inside
    oled.drawStr(0, 23, "T inside:");

    if (isnan(T_INSIDE))
    {
        oled.drawStr(72, 23, "--.- C");
    }
    else
    {
        char buffer[20];

        snprintf(
            buffer,
            sizeof(buffer),
            "%5.1f C",
            T_INSIDE
        );

        oled.drawStr(72, 23, buffer);
    }

    // --------------------------------------------------------
    // T set
    // --------------------------------------------------------

    oled.drawStr(0, 36, "T set:");

    char setBuffer[20];

    snprintf(
        setBuffer,
        sizeof(setBuffer),
        "%5.1f C",
        T_SET
    );

    oled.drawStr(72, 36, setBuffer);

    // --------------------------------------------------------
    // Fridges
    // --------------------------------------------------------

    oled.drawStr(0, 49, "Fridges:");

    if (fridgeRunning)
    {
        oled.drawStr(72, 49, "ON");
    }
    else
    {
        oled.drawStr(72, 49, "OFF");
    }

    // --------------------------------------------------------
    // System
    // --------------------------------------------------------

    oled.drawStr(0, 62, "System:");

    if (systemStarted)
    {
        oled.drawStr(72, 62, "START");
    }
    else
    {
        oled.drawStr(72, 62, "STOP");
    }

    oled.sendBuffer();
}

// ============================================================
// WEB PAGE
// ============================================================

String makeWebPage()
{
    String html;

    html += "<!DOCTYPE html>";
    html += "<html lang='uk'>";
    html += "<head>";

    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";

    // Автоматичне оновлення кожні 10 секунд
    html += "<meta http-equiv='refresh' content='10'>";

    html += "<title>🍻 NOT_HOT_BEER</title>";

    html += "<style>";

    html += "body{";
    html += "font-family:Arial,sans-serif;";
    html += "margin:0;";
    html += "padding:20px;";
    html += "min-height:100vh;\
    background:linear-gradient(\
      to bottom,\
      #fff 0%,\
      #fff 20%,\
      #ffd36a 32%,\
      #f28c18 48%,\
      #d96b00 100%\
    );\
    position:relative;\
    overflow-x:hidden;\
    }\
    body::before{\
    content:'';\
    position:fixed;\
    inset:20% 0 0;\
    pointer-events:none;\
    background-image:\
      radial-gradient(circle,rgba(255,255,255,.65) 0 2px,transparent 3px),\
      radial-gradient(circle,rgba(255,255,255,.5) 0 1.5px,transparent 2.5px),\
      radial-gradient(circle,rgba(255,255,255,.4) 0 1px,transparent 2px);\
    background-size:97px 151px,137px 211px,71px 113px;\
    background-position:10px 120px,50px 30px,80px 180px;\
    animation:bubbles 12s linear infinite;\
    opacity:.8;\
    }\
    @keyframes bubbles{\
    from{transform:translateY(45px);}\
    to{transform:translateY(-170px);}";
    html += "}";

    html += ".container{";
    html += "max-width:600px;";
    html += "margin:auto;";
    html += "background:white;";
    html += "padding:20px;";
    html += "border-radius:12px;";
    html += "box-shadow:0 2px 10px rgba(0,0,0,0.15);";
    html += "}";

    html += "h1{text-align:center;}";

    html += ".row{";
    html += "display:flex;";
    html += "justify-content:space-between;";
    html += "padding:12px;";
    html += "border-bottom:1px solid #ddd;";
    html += "}";

    html += ".value{font-weight:bold;}";

    html += ".on{color:green;font-weight:bold;}";
    html += ".off{color:red;font-weight:bold;}";

    html += "</style>";

    html += "</head>";

    html += "<body>";

    html += "<div class='container'>";

    html += "<h1>🍻 NOT_HOT_BEER</h1>";

    // T inside
    html += "<div class='row'>";
    html += "<span>T inside</span>";
    html += "<span class='value'>";

    if (isnan(T_INSIDE))
    {
        html += "ERROR";
    }
    else
    {
        html += String(T_INSIDE, 1);
        html += " &deg;C";
    }

    html += "</span>";
    html += "</div>";

    // T set
    html += "<div class='row'>";
    html += "<span>T set</span>";
    html += "<span class='value'>";
    html += String(T_SET, 1);
    html += " &deg;C";
    html += "</span>";
    html += "</div>";

    // Fridges
    html += "<div class='row'>";
    html += "<span>Fridges</span>";

    if (fridgeRunning)
    {
        html += "<span class='on'>ON</span>";
    }
    else
    {
        html += "<span class='off'>OFF</span>";
    }

    html += "</div>";

    // System
    html += "<div class='row'>";
    html += "<span>System</span>";

    if (systemStarted)
    {
        html += "<span class='on'>START</span>";
    }
    else
    {
        html += "<span class='off'>STOP</span>";
    }

    html += "</div>";

    // Logo
    html += "<div class='row'>";
    html += "<span>Logo</span>";

    if (lightRelayActive)
    {
        html += "<span class='on'>ON</span>";
    }
    else
    {
        html += "<span class='off'>OFF</span>";
    }

    html += "</div>";

    // OLED
    html += "<div class='row'>";
    html += "<span>OLED</span>";

    if (oledSleeping)
    {
        html += "<span class='off'>SLEEP</span>";
    }
    else
    {
        html += "<span class='on'>ON</span>";
    }

    html += "</div>";

    html += "</div>";

    html += "</body>";
    html += "</html>";

    return html;
}

// ============================================================
// WEB SERVER HANDLERS
// ============================================================

void handleRoot()
{
    server.send(
        200,
        "text/html; charset=utf-8",
        makeWebPage()
    );
}

// ============================================================
// WIFI
// ============================================================

void connectWiFi()
{
    Serial.println();
    Serial.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < 20000)
    {
        delay(500);

        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi connection FAILED.");
    }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    // ========================================================
    // КНОПКИ
    // ========================================================

    // Кнопки підключені між GPIO і GND.
    // Використовуємо внутрішній pull-up ESP32.
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_STOP, INPUT_PULLUP);
    pinMode(BTN_START, INPUT_PULLUP);

    // ========================================================
    // РЕЛЕ
    // ========================================================

    pinMode(RELAY_LIGHT, OUTPUT);
    pinMode(RELAY_FRIDGE1, OUTPUT);
    pinMode(RELAY_FRIDGE2, OUTPUT);

    // ========================================================
    // Дуже важливо:
    // спочатку встановлюємо безпечні OFF-стани
    // ========================================================

    digitalWrite(RELAY_LIGHT, LIGHT_RELAY_OFF);

    digitalWrite(RELAY_FRIDGE1, FRIDGE_RELAY_OFF);
    digitalWrite(RELAY_FRIDGE2, FRIDGE_RELAY_OFF);

    fridgeRunning = false;
    lightRelayActive = false;
    systemStarted = false;

    // ========================================================
    // ВБУДОВАНИЙ LED
    // ========================================================

    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, LOW);

    // ========================================================
    // BUZZER
    // ========================================================

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // ========================================================
    // DS18B20
    // ========================================================

    ds18b20.begin();

    // ========================================================
    // OLED
    // ========================================================

    oled.begin();

    oled.setPowerSave(0);

    oled.clearBuffer();

    oled.setFont(u8g2_font_6x10_tf);

    oled.drawStr(30, 30, "NOT_HOT_BEER");

    oled.sendBuffer();

    delay(1500);

    // ========================================================
    // WIFI
    // ========================================================

    connectWiFi();

    // ========================================================
    // WEB SERVER
    // ========================================================

    server.on("/", handleRoot);

    server.begin();

    Serial.println("Web server started.");

    // ========================================================
    // ПЕРШЕ ВИМІРЮВАННЯ
    // ========================================================

    readSensors();

    updateDisplay();

    // Запам'ятовуємо момент останньої активності
    lastButtonActivity = millis();

    Serial.println();
    Serial.println("==============================");
    Serial.println(" NOT_HOT_BEER");
    Serial.println("==============================");

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    // ========================================================
    // WEB SERVER
    // ========================================================

    server.handleClient();

    // ========================================================
    // КНОПКИ
    // ========================================================

    checkButtons();

    // ========================================================
    // OLED TIMEOUT
    // ========================================================

    checkOLEDTimeout();

    // ========================================================
    // ДАТЧИК
    // ========================================================

    if (millis() - lastSensorRead >= SENSOR_INTERVAL)
    {
        lastSensorRead = millis();

        readSensors();

        controlFridges();

        updateDisplay();

        // ====================================================
        // SERIAL MONITOR
        // ====================================================

        Serial.print("Inside: ");

        if (isnan(T_INSIDE))
        {
            Serial.print("ERROR");
        }
        else
        {
            Serial.print(T_INSIDE);
        }

        Serial.print(" C | Set: ");
        Serial.print(T_SET);

        Serial.print(" C | Fridges: ");

        if (fridgeRunning)
        {
            Serial.print("ON");
        }
        else
        {
            Serial.print("OFF");
        }

        Serial.print(" | Light: ");

        if (lightRelayActive)
        {
            Serial.println("ON");
        }
        else
        {
            Serial.println("OFF");
        }
    }

    // ========================================================
    // СВІТЛОВЕ РЕЛЕ
    // ========================================================

    updateLightRelay();

    delay(10);
}
