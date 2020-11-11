//#define USE_DEEP_SLEEP
//#define USE_SOUND

#include <TFT_eSPI.h>
#include <Wire.h>
#include <MPU6050.h>
#include <EEPROM.h>
#include <soc/rtc.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define FONT_14
#ifdef FONT_14
#include "FontsRus/FreeSans14.h"
#else
#include "FontsRus/FreeSans12.h"
#endif

void drawSettings();
void drawSprite(TFT_eSprite *sprite);
void scaleSprite(TFT_eSprite *src, TFT_eSprite *dst, float scale, uint8_t alpha);
void prepareAnimation(bool isAppear);
char *utf8rus2(const char *source);
void checkInput();
void turnDisplay(bool isOn);
#define turnDisplayOn turnDisplay(true)
#define turnDisplayOff turnDisplay(false)

TFT_eSPI tft = TFT_eSPI();                      // TFT object
TFT_eSprite mainSprite = TFT_eSprite(&tft);     // main sprite object
TFT_eSprite scaledSprite = TFT_eSprite(&tft);   // scaled sprite object
#define TFT_BKLIGHT_PIN 13
int screenTimeout = 30;
#define SCREEN_TIMEOUT (screenTimeout * 1000)
int sleepTimeout = 60;
#define SLEEP_TIMEOUT (sleepTimeout * 1000)
bool displayIsOn = true;

enum GAME_STATES
{
    APPEARANCE,
    SHOW,
    DISAPPEARANCE,
    SETTINGS
};
GAME_STATES gameState;

// Prediction ranges:
//		optimistic:  1 - 10
//		pessimistic: 4 - 12
//		realistic:   1 - 15

#define NUM_LANGUAGES 3
#define NUM_PHRASES 16
const String languages[NUM_LANGUAGES][NUM_PHRASES][3] 
{
    // 0: English
    {
        {"Ask me,", "then", "shake"},  
		
        {"It is", "certain", ""},       // 1
        {"Certainly", "", ""},          // 2
        {"Without", "a doubt", ""},     // 3
        {"You", "may rely", "on it"},   // 4
        {"As", "I see it,", "yes" },    // 5
        {"Most", "likely", "" },        // 6
        {"Yes", "", ""},                // 7
        {"Can't", "predict", ""},       // 13		
        {"Ask", "again", "later"},      // 14
        {"Not", "sure", ""},            // 15
        {"My", "reply is", "no"},       // 8
        {"Very", "doubtful", ""},       // 9
        {"No", "", ""},                 // 10
        {"No", "chance", ""},           // 11
        {"No", "way", ""},              // 12
    },
    // 1: Russian
    {
        {"Спроси и", "встряхни", ""},
		
        {"Это", "точно!", ""},          // 1
        {"Точно", "", ""},              // 2
        {"Без", "сомнения", ""},        // 3
        {"Ты", "можешь", "так считать"},// 4
        {"Как", "я вижу,", "да"},       // 5
        {"Скорее", "всего", ""},        // 6
        {"Да", "", ""},                 // 7
        {"Не", "могу", "сказать"},      // 13		
        {"Спроси", "позже", ""},        // 14
        {"Не", "уверена", ""},          // 15
        {"Мой", "ответ:", "нет"},       // 8
        {"Вряд ли", "", ""},            // 9
        {"Нет", "", ""},                // 10
        {"Без", "шансов", ""},          // 11
        {"Никак", "не выйдет", ""},     // 12
    },
    // 2: Spanish
    {
        {"Pregunte,", "luego", "agite"},	
		
        {"Es", "cierto", ""},					// 1
        {"¡Desde", "luego!", ""},               // 2
        {"Sin", "duda", ""},                    // 3
        {"Puedes", "confiar", "en ello"},       // 4
        {"Como yo", "lo veo,", "sí"},           // 5
        {"Más", "probable", ""},                // 6
        {"si", "", ""},                         // 7
        {"No puedo", "predecir", ""},           // 13		
        {"Pregunta de", "nuevo más", "tarde"},  // 14
        {"No estoy", "seguro", ""},             // 15
        {"Mi", "respuesta", "es no"},           // 8
        {"Muy", "dudoso", ""},                  // 9
        {"No", "", ""},                         // 10
        {"Ninguna", "posibilidad", ""},         // 11
        {"De ninguna", "manera", ""},           // 12
    }
};

const String langNames[NUM_LANGUAGES] = { "English", "Russian", "Spanish"};
byte lang = 0;
byte phraseIdx = 0;
bool doRandSeed = true;

#define MIN_SCALE 0.1
#define MAX_SCALE 0.98
const int animSteps = 24;
const int tr_side = 206;
float angle, d_angle = 1.0, scale = 1.0, d_scale = 1.0 / animSteps;
int rotate_dir = 1;
unsigned long startShowTime, startHideTime, displayOffTime;
int d_x, d_y, showCount;

MPU6050 mpu;

enum INPUT_STATES 
{
    NONE,
    SHAKE,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

int16_t x, y, z, keyMax;
float ax, ay, az, gx, gy, gz;
float avg_ax, avg_ay, avg_az;
float min_ax, max_ax, min_ay, max_ay, min_az, max_az;
int16_t checkInputCount, keyCount, shakeCount = 0;
unsigned long measTime;
INPUT_STATES inputState = NONE;

#define INPUT_INTERVAL 1000
int checkInputInterval = INPUT_INTERVAL;

#ifdef USE_SOUND
bool playerReady = false;
HardwareSerial softSerial(2);
DFRobotDFPlayerMini player;
#endif

void setup() 
{
    Serial.begin(115200);

    // Read current language from EEPROM
    EEPROM.begin(1);
    lang = EEPROM.read(0);
    if (lang >= NUM_LANGUAGES) lang = 0;

    // MPU initialization
    Wire.begin();

    mpu.initialize();
    
    mpu.setInterruptDrive(0);                       // Set interrupt drive mode (0=push-pull, 1=open-drain)
    mpu.setInterruptMode(1);                        // Set interrupt mode (0=active-high, 1=active-low)
    mpu.setInterruptLatch(0);                       // Interrupt pulses when triggered instead of remaining on until cleared
    mpu.setDHPFMode(MPU6050_DHPF_5);
    mpu.setMotionDetectionThreshold(2);             // Motion detection acceleration threshold. 1LSB = 2mg.
    mpu.setMotionDetectionDuration(50);             // Motion detection event duration in ms
    mpu.setFreefallDetectionCounterDecrement(1);    // Set free fall detection counter decrement
    mpu.setMotionDetectionCounterDecrement(1);      // Set motion detection counter decrement
    mpu.setIntMotionEnabled(false);                 // Set interrupt enabled status

    checkInputInterval = INPUT_INTERVAL;
    keyMax = 50;
    keyCount = checkInputCount = shakeCount = 0; 
    az = avg_az = 0;
    min_ax = min_ay = min_az = 5000;
    max_ax = max_ay = max_az = -5000;
    checkInput();

    // Initialize TFT display
    tft.begin();  
    tft.fillScreen(TFT_BLACK);
    turnDisplayOn;

    // Calculate triangle points
    int w = tft.width();
    float tr_h = (tr_side * 1.732) / 2.0;
    d_y = (w - tr_h) / 2; 
    d_x = (w - tr_side) / 2;
    
    // Set pivot to the middle of the screen
    tft.setPivot(w/2, w/2);

    // Create sprite
    mainSprite.createSprite(w, w);
    mainSprite.setTextColor(TFT_WHITE);
#ifdef FONT_14    
    mainSprite.setFreeFont(&FreeSans14pt8b);
#else
    mainSprite.setFreeFont(&FreeSans12pt8b);
#endif    
    mainSprite.setAttribute(UTF8_SWITCH, false);

    // We should start from "intro" message
    phraseIdx = 0;
    angle = 0; scale = 0.1;    
    drawSprite(&mainSprite);

#ifdef USE_SOUND
    // Initialize software serial port (speed, type, RX & TX pins)
    softSerial.begin(9600, SERIAL_8N1, 16, 17);  
    delay(100);  

    // Initialize DFPlayerMini
    if (player.begin(softSerial, false, true)) 
    {  
        playerReady = true;
        Serial.println("Player is ready!");
        // Set serial communictaion time out 500ms
        player.setTimeOut(500);
        player.outputDevice(DFPLAYER_DEVICE_SD);
        player.volume(30);
    }
#endif    

    prepareAnimation(true);
    gameState = APPEARANCE;
    inputState = NONE;
}

void loop() 
{
    if (checkInputCount++ > checkInputInterval)
    {
        checkInputCount = 0;
        checkInput();

        if (ax-avg_ax > 10 || ay-avg_ay > 10 || az-avg_az > 10)
        {
            if (!displayIsOn)
            {
                turnDisplayOn;
                inputState = NONE;
            }
            else startHideTime = millis();
        }

        // Check triple shake
        if (displayIsOn && inputState == SHAKE)
        {
            if (shakeCount >= 4)
            {
                shakeCount = 0;
                gameState = SETTINGS;
                inputState = NONE;
                drawSettings();
                delay(500);
                startHideTime = millis();
            }
        }
    }

    // Handle game states only when display is on
    if (displayIsOn)
    {
        // Reset multiple shake counter
        if (millis()-measTime > 1500) shakeCount = 0;
        
        switch (gameState)
        {
            case SETTINGS:
                if (millis()-startHideTime > SCREEN_TIMEOUT) turnDisplayOff;
                if (inputState != NONE)
                {
                    if (inputState == SHAKE)
                    {
                        phraseIdx = 0; 
                        drawSprite(&mainSprite);
                        mainSprite.pushSprite(0, 0);
                        gameState = SHOW;
                        delay(1000);
                    }
                    else
                    {
                        if (inputState == UP) lang--; else if (inputState == DOWN) lang++;
                        if (lang < 0) lang = NUM_LANGUAGES-1; else if (lang >= NUM_LANGUAGES) lang = 0;
    
                        drawSettings();
    
                        // Save new language setting
                        EEPROM.write(0, lang);
                        EEPROM.commit();
                        delay(500);                    
                    }
                    shakeCount = 0;
                    startHideTime = millis();                
                }
                break;
            
            case APPEARANCE:
                if (millis()-startHideTime > 1000)
                {
#ifdef USE_SOUND
                    if (playerReady && scale <= MIN_SCALE + 0.1)
                    {
                        player.play(lang*NUM_PHRASES + phraseIdx + 1);
                    }
#endif
                    scaleSprite(&mainSprite, &scaledSprite, scale, scale * 210);
                    scaledSprite.pushRotated(rotate_dir > 0 ? angle : 360-angle);
                    angle -= d_angle;
                    scale += d_scale;
                    if (scale > 0.98)
                    {
                        mainSprite.pushSprite(0, 0);
                        startShowTime = millis();
                        gameState = SHOW;
                    }
                }
                break;
    
            case SHOW:
                // Provide new "divination" on shake
                if (inputState == SHAKE)
                {
                    // select random prediction
                    phraseIdx = random(1, 14);
                    prepareAnimation(false);
                    gameState = DISAPPEARANCE;
                }
                // Turn off display after 25 seconds
                else if (millis() - startHideTime > SCREEN_TIMEOUT) turnDisplayOff;
                break;
    
            case DISAPPEARANCE:
                scaleSprite(&mainSprite, &scaledSprite, scale, scale * 180);
                scale -= d_scale;
                if (scale < 0.95) angle += d_angle;            
                scaledSprite.pushRotated(rotate_dir > 0 ? angle : 360-angle);
    
                if (scale < 0.05)
                {
                    // Clear screen
                    tft.fillScreen(TFT_BLACK);
                    
                    // Draw sprite
                    drawSprite(&mainSprite);
    
                    gameState = APPEARANCE;
                    prepareAnimation(true);
                    startHideTime = millis();
                }
                break;    
        }
    
        // Always reset input state to NONE
        inputState = NONE;            
    }
    // We can't use light or deep sleep modes now, let's use delay()
    else
    {
        delay(10);
#ifdef USE_DEEP_SLEEP
        if (millis()-displayOffTime > SLEEP_TIMEOUT)
        {
            // Put ST7789 TFT controller to sleep (dispaly is already off)
            tft.writecommand(0x10);
            // Enable motion interrupt on MPU6050
            mpu.setIntMotionEnabled(true);
            // Enable external source wake up for ESP32 dev. board
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_14, 0);
            delay(100);
            esp_deep_sleep_start();
        }
#endif        
    }
}

// Toggle display, backlight and CPU clock
void turnDisplay(bool isOn)
{
    displayIsOn = isOn;

    // Turn on/off display
    tft.writecommand(isOn ? 0x29 : 0x28);
    delay(10);
    
    // Turn on/off backlight
    if (isOn)
    {
        // Disable holding pin state for backlight pin
        gpio_hold_dis(GPIO_NUM_13);
        pinMode(TFT_BKLIGHT_PIN, INPUT); 
    }
    else
    {
        pinMode(TFT_BKLIGHT_PIN, OUTPUT);
        // Enable holding pin state during sleep for backlight pin
        gpio_hold_en(GPIO_NUM_13);
    }
    // Set clock
    rtc_clk_cpu_freq_set(isOn ? RTC_CPU_FREQ_240M : RTC_CPU_FREQ_80M);
    checkInputInterval = isOn ? INPUT_INTERVAL : 2;

    if (isOn) startHideTime = millis();
    else displayOffTime = millis();
}

// Draw settings menu
void drawSettings()
{
    int w = mainSprite.width();
    int lineHeight = 32;
    int startY = lineHeight*2;
    mainSprite.fillSprite(TFT_BLACK);
    mainSprite.drawString("Choose language:", 4, 2);
    for (int i=0; i<NUM_LANGUAGES; i++)
    {
        if (i == lang) mainSprite.fillRoundRect(2, startY + (i*lineHeight), w-2, lineHeight, 2, TFT_BLUE);
        mainSprite.drawString(langNames[i], 20, startY + (i*lineHeight));
    }
    mainSprite.pushSprite(0, 0);
}

// Prepare animation
void prepareAnimation(bool isAppear)
{
    // Rotation direction 
    rotate_dir = (random(0,2) == 1 ? 1 : -1);
    angle = isAppear ? random(45, 165) : 0;
    d_angle = isAppear ? angle / animSteps : (random(35, 155) / animSteps);

    scale = isAppear ? MIN_SCALE : MAX_SCALE;
    d_scale = 0.98 / (animSteps + 3);
    startHideTime = startShowTime = millis();
}

// Draws "prediction" triangle with text
void drawSprite(TFT_eSprite *sprite) 
{
    char phrases[3][50];
    int numPhrases = 0;

    int w = sprite->width();
    int h = w;
    int c = w/2;
    sprite->setPivot(c, c);

    for (int i = 0; i < 3; i++)
    {
        strcpy(phrases[i], utf8rus2(languages[lang][phraseIdx][i].c_str()));
        if (strlen(phrases[i]) > 0) numPhrases++;
    }  

    sprite->fillSprite(TFT_BLACK);

    int y = c-d_x;

    // Long first word and more than one word in sentence
    if (strlen(phrases[0]) >= 5 && numPhrases > 1)
    {
        sprite->fillTriangle(d_x, d_y, w-d_x, d_y, c, h-d_y, TFT_BLUE);
        if (numPhrases == 3) y = h/5-d_y; else y = h/4-d_y;
    }
    // Short first word or only one word
    else 
    {
        sprite->fillTriangle(d_x, h-d_y, c, d_y, w-d_x, h-d_y, TFT_BLUE);
        if (numPhrases == 2) y = h/3+d_x; else if (numPhrases == 3) y = h/4+d_x;
    }

    for (int i = 0; i < numPhrases; i++)
        sprite->drawString(phrases[i], c - (sprite->textWidth(phrases[i]) / 2), i * sprite->fontHeight()+y+d_x);
}

// Sprite scaling with linear interpolation
// TODO: refactor for more efficient, elegant and faster code
#define RGB(r,g,b) (uint16_t) ( ((r << 11) & 0xF800) | ((g << 5) & 0x7E0) |  (b & 0x1F) )
#define Rc(px) (uint16_t)  (px >> 11)
#define Gc(px) (uint16_t) ((px & 0x7E0) >> 5)
#define Bc(px) (uint16_t)  (px & 0x1F)
void scaleSprite(TFT_eSprite *src, TFT_eSprite *dst, float scale, uint8_t alpha = 255)
{
    uint16_t x, y, w, h, r, g, b;
    uint16_t A, B, C, D;

    uint16_t newWidth = (uint16_t)(src->width() * scale);
    uint16_t newHeight = (uint16_t)(src->height() * scale);
    float ratio = ((float)(src->width() - 1)) / newWidth;

    dst->deleteSprite();
    dst->createSprite(newWidth, newHeight);

    for (int i = 0; i < newHeight; i++)
    {
        for (int j = 0; j < newWidth; j++)
        {
            x = (uint16_t)(ratio * j);
            y = (uint16_t)(ratio * i);
            w = (uint16_t)(ratio * j) - x;
            h = (uint16_t)(ratio * i) - y;

            A = src->readPixel(x, y);
            B = src->readPixel(x + 1, y);
            C = src->readPixel(x, y + 1);
            D = src->readPixel(x + 1, y + 1);

            r = (uint16_t)(Rc(A) * (1 - w) * (1 - h) + Rc(B) * (w) * (1 - h) + Rc(C) * (h) * (1 - w) + Rc(D) * (w * h));
            g = (uint16_t)(Gc(A) * (1 - w) * (1 - h) + Gc(B) * (w) * (1 - h) + Gc(C) * (h) * (1 - w) + Gc(D) * (w * h));
            b = (uint16_t)(Bc(A) * (1 - w) * (1 - h) + Bc(B) * (w) * (1 - h) + Bc(C) * (h) * (1 - w) + Bc(D) * (w * h));

            // If no alpha blending, just draw the pixel
            if (alpha == 255) dst->drawPixel(j, i, RGB(r, g, b));
            // Otherwise, draw with alpha blending
            else dst->drawPixel(j, i, dst->alphaBlend(alpha, RGB(r, g, b), TFT_BLACK));
        }
    }
}

#define MAX_STR_LEN 30
char target[MAX_STR_LEN + 1] = "";
char *utf8rus2(const char *source)
{
    int i, j, k;
    unsigned char n;
    char m[2] = { '0', '\0' };
    strcpy(target, ""); k = strlen(source); i = j = 0;
    while (i < k) 
    {
        n = source[i++];
        if (n >= 127) 
        {
            switch (n) 
            {
                case 208: 
                    n = source[i]; i++;
                    break;
            
                case 209: 
                    n = source[i]; i++;
                    break;
            } 
        } 

        m[0] = n; strcat(target, m);
        j++; if (j >= MAX_STR_LEN) break;
    }
    return target;
}

#define WINDOW_SIZE 25
float approxAverage (float avg, float input) 
{
    avg -= avg / WINDOW_SIZE;
    avg += input / WINDOW_SIZE;
    return avg;
}

void checkInput()
{
    mpu.getAcceleration(&x, &y, &z);

    if (doRandSeed)
    {
        randomSeed(x+y+z);
        doRandSeed = false;
    }

    ax = x / 100;
    ay = y / 100;
    az = z / 100;

    avg_ax = approxAverage(avg_ax, ax);
    avg_ay = approxAverage(avg_ay, ay);
    avg_az = approxAverage(avg_az, az);

    // Detect max & min values
    if (ax > max_ax) max_ax = ax; else if (ax < min_ax) min_ax = ax;
    if (ay > max_ay) max_ay = ay; else if (ay < min_ay) min_ay = ay;
    if (az > max_az) max_az = az; else if (az < min_az) min_az = az;

    // Shake detection
    if (max_ax-min_ax > 500 || max_ay-min_ay > 500 || max_az-min_az > 500)
    {
        min_ax = min_ay = min_az = 5000;
        max_ax = max_ay = max_az = -5000;
        measTime = millis();
        shakeCount++;
        inputState = SHAKE;        
    }
    // Up/down, left/right detection (for menu control)
    else if (keyCount++ > keyMax)
    {
        keyCount = 0;
        keyMax = 50;
        float dz = (az-avg_az) / 8;

        if (abs(dz) < 30 && abs(dz) > 4)
        {
            if (dz < -5) inputState = UP; 
            else if (dz > 5) inputState = DOWN;
            
            if (inputState != NONE)
            {
                keyMax = 200;
                measTime = millis();
            }
        }
    }
}
