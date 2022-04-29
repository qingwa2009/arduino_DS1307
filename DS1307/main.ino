#include <Arduino.h>
#include <math.h>
#include "../mylib/MyTWI.h"
#include "../mylib/MySPI.h"
#include "../mylib/MyPin.h"
#include "../mylib/MyPWM.h"
#include "../mylib/SSD1306/SSD1306.h"
#include "../mylib/DS1307/DS1307.h"

#define PIN_L 4
#define PIN_R 5

#define HOLD_TIME (65535 - 16000000 / 1024 * 3)   /*3s*/
#define HOLD_TIME_S (65535 - 16000000 / 1024 * 1) /*0.3s*/

#define STATE_FREE 0
#define STATE_ENTERING_SETTING 1 /*准备进入设置模式*/
#define STATE_SELECT 2           /*设置模式-选择*/
#define STATE_ENTERING_EDIT 3    /*准备进入修改模式*/
#define STATE_EDITING 4          /*设置模式-修改*/
#define STATE_EXITING_EDIT 5     /*准备退出修改模式*/

static char sDate[] = "2000-01-01";
static char sTime[] = "00:00:00";
static DateTime newDt;
static uint16_t frameCount;

void updateDateTimeStr()
{
    sDate[2] = newDt.year / 10 + 48;
    sDate[3] = newDt.year % 10 + 48;
    sDate[5] = newDt.month / 10 + 48;
    sDate[6] = newDt.month % 10 + 48;
    sDate[8] = newDt.date / 10 + 48;
    sDate[9] = newDt.date % 10 + 48;

    sTime[0] = newDt.hour / 10 + 48;
    sTime[1] = newDt.hour % 10 + 48;
    sTime[3] = newDt.minute / 10 + 48;
    sTime[4] = newDt.minute % 10 + 48;
    sTime[6] = newDt.second / 10 + 48;
    sTime[7] = newDt.second % 10 + 48;
}

void (*drawFunc)();
void drawClock();
void drawSetting();
void drawEdit();
void drawCursor();

volatile bool ticking = 0;
volatile uint8_t state;
uint8_t cursor = 0;
bool isClockFaceDirty = 0;

void enterSetting()
{
    cursor = 6;
    fixMonth2();
    state = STATE_SELECT;
    drawFunc = drawSetting;
    Serial.println("setting!");
}
void exitSetting()
{
    state = STATE_FREE;
    Serial.println("exit setting!");
}
void enterEdit()
{
    switch (cursor)
    {
    case 7:
        /*确认修改*/
        setDateTime(newDt);
    case 6:
        /*取消修改*/
        state = STATE_FREE;
        drawFunc = drawClock;
        return;
    default:
        break;
    }
    state = STATE_EDITING;
    drawFunc = drawEdit;
    Serial.println("editing!");
}
void exitEdit()
{
    state = STATE_SELECT;
    drawFunc = drawSetting;
    Serial.println("exit editing!");
}
void cursorMoveL()
{
    cursor = (cursor - 1) & 0x07;
    isClockFaceDirty = 1;
}
void cursorMoveR()
{
    cursor = (cursor + 1) & 0x07;
    isClockFaceDirty = 1;
}

static uint8_t ds[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/*修正闰月天数*/
void fixMonth2()
{
    if ((newDt.year % 100) == 0) /*整百年*/
        ds[1] = ((newDt.year % 400) != 0) ? 28 : 29;
    else
        ds[1] = ((newDt.year % 4) != 0) ? 28 : 29;
    return;
}
/*修正月份天数*/
void fixMonthDate()
{
    if (newDt.date > ds[newDt.month - 1])
        newDt.date = ds[newDt.month - 1];
}

void changeL()
{
    switch (cursor)
    {
    case 0: /*year*/
        newDt.year++;
        if (newDt.year > 99)
            newDt.year = 0;
        fixMonth2();
        fixMonthDate();
        break;
    case 1: /*month*/
        newDt.month++;
        if (newDt.month > 12)
            newDt.month = 1;
        fixMonthDate();
        break;
    case 2: /*date*/
        newDt.date++;
        if (newDt.date > ds[newDt.month - 1])
            newDt.date = 1;
        break;
    case 3: /*hour*/
        newDt.hour++;
        if (newDt.hour > 23)
            newDt.hour = 0;
        break;
    case 4: /*minute*/
        newDt.minute++;
        if (newDt.minute > 59)
            newDt.minute = 0;
        break;
    case 5: /*second*/
        newDt.second++;
        if (newDt.second > 59)
            newDt.second = 0;
        break;
    case 6:
    case 7:
        break;
    default:
        break;
    }
    updateDateTimeStr();
    isClockFaceDirty = 1;
}
void changeR()
{
    switch (cursor)
    {
    case 0: /*year*/
        newDt.year--;
        if ((int8_t)newDt.year < 0)
            newDt.year = 99;
        fixMonth2();
        fixMonthDate();
        break;
    case 1: /*month*/
        newDt.month--;
        if ((int8_t)newDt.month < 1)
            newDt.month = 12;
        fixMonthDate();
        break;
    case 2: /*date*/
        newDt.date--;
        if ((int8_t)newDt.date < 1)
            newDt.date = ds[newDt.month - 1];
        break;
    case 3: /*hour*/
        newDt.hour--;
        if ((int8_t)newDt.hour < 0)
            newDt.hour = 23;
        break;
    case 4: /*minute*/
        newDt.minute--;
        if ((int8_t)newDt.minute < 0)
            newDt.minute = 59;
        break;
    case 5: /*second*/
        newDt.second--;
        if ((int8_t)newDt.second < 0)
            newDt.second = 59;
        break;
    case 6:
    case 7:
        break;
    default:
        break;
    }
    updateDateTimeStr();
    isClockFaceDirty = 1;
}
static bool preLDown = 0;
static bool preRDown = 0;
static bool curLDown = 0;
static bool curRDown = 0;
static bool hasBtnEvent = 0;
void handleBtnInterrupt()
{
    hasBtnEvent = 1;
    curLDown = !MYPIN_READ_HIGH(PIN_L);
    curRDown = !MYPIN_READ_HIGH(PIN_R);
}

void onBtnClick()
{
    if (!hasBtnEvent)
        return;
    hasBtnEvent = false;
    switch (state)
    {
    case STATE_FREE:
        /*两个按住，准备进入设置模式*/
        if (curLDown && curRDown)
        {
            state = STATE_ENTERING_SETTING;
            ticking = 1;
            TCNT1 = HOLD_TIME; /*计时开始，n秒后触发中断进入设置模式*/
            Serial.println("entering setting!");
        }
        break;
    case STATE_ENTERING_SETTING:
        /*准备进入设置模式中，有按键松开，取消进入设置模式*/
        if (!curLDown || !curRDown)
        {
            state = STATE_FREE;
            ticking = 0;
            Serial.println("cancel enter setting!");
        }
        break;
    case STATE_SELECT:
        /*长按，准备进入修改模式*/
        if (curLDown || curRDown)
        {
            state = STATE_ENTERING_EDIT;
            ticking = 1;
            TCNT1 = HOLD_TIME_S; /*计时开始，n秒后触发中断进入修改模式*/
            Serial.println("entering edit!");
        }
        break;
    case STATE_ENTERING_EDIT:
        /*按下松开，移到光标*/
        state = STATE_SELECT;
        ticking = 0;
        Serial.println("selecting");
        if (preLDown && (!curLDown))
            cursorMoveL();
        else if (preRDown && (!curRDown))
            cursorMoveR();
        preLDown = 0;
        curLDown = 0;
        return;
    case STATE_EDITING:
        /*长按，准备退出修改模式*/
        if (curLDown || curRDown)
        {
            state = STATE_EXITING_EDIT;
            ticking = 1;
            TCNT1 = HOLD_TIME_S; /*计时开始，n秒后触发中断退出修改模式*/
            Serial.println("exiting edit!");
        }
        break;
    case STATE_EXITING_EDIT:
        /*按下松开，修改光标下数字*/
        state = STATE_EDITING;
        ticking = 0;
        Serial.println("editing");
        if (preLDown && (!curLDown))
            changeL();
        else if (preRDown && (!curRDown))
            changeR();
        preLDown = 0;
        curLDown = 0;
        return;
    default:
        break;
    }

    preLDown = curLDown;
    preRDown = curRDown;
}

ISR(TIMER1_OVF_vect)
{
    if (ticking)
    {
        ticking = 0;
        preLDown = 0;
        preRDown = 0;
        switch (state)
        {
        case STATE_ENTERING_SETTING:
            getDateTime(&newDt);
            updateDateTimeStr();
            enterSetting();
            break;
        case STATE_ENTERING_EDIT:
            enterEdit();
            break;
        case STATE_EXITING_EDIT:
            exitEdit();
            break;
        default:
            break;
        }
    }
}

void drawClock()
{
    static uint8_t lastSecond = 0xEE;
    getDateTime(&newDt);
    if (newDt.second != lastSecond)
    {
        lastSecond = newDt.second;
        updateDateTimeStr();
        clearDisplay(0);
        uint8_t scale = 2;
        drawStr(0, 0, (uint8_t *)sDate, sizeof(sDate) - 1, 1, WHITE);
        drawStr((SCREEN_W - 8 * (FONT_W + 1) * scale) / 2, (SCREEN_H - FONT_H * scale) / 2, (uint8_t *)sTime, sizeof(sTime) - 1, scale, WHITE);
        display();
        delay(500);
    }
    delay(80);
}

#define scale 2
#define wDate (((sizeof(sDate) - 1) * (FONT_W + 1) - 1) * scale)
#define hDate (FONT_H * scale)
#define wTime (((sizeof(sTime) - 1) * (FONT_W + 1) - 1) * scale)
#define hTime (FONT_H * scale)

const int8_t xDate = (SCREEN_W - wDate) / 2;
const int8_t yDate = SCREEN_H / 2 - hDate - 4;
const int8_t xTime = (SCREEN_W - wTime) / 2;
const int8_t yTime = SCREEN_H / 2 + 4;

const int8_t xBtnX = 0;
const int8_t yBtnX = SCREEN_H - FONT_H - 4;
const int8_t xBtnOK = SCREEN_W - FONT_W * 4;
const int8_t yBtnOK = SCREEN_H - FONT_H - 4;
const int8_t wBtn = FONT_W * 4;
const int8_t hBtn = FONT_H + 4;

void drawEditClockFace()
{
    clearDisplay(0x00);
    drawStr(xDate, yDate, (uint8_t *)sDate, sizeof(sDate) - 1, scale, WHITE);
    drawStr(xTime, yTime, (uint8_t *)sTime, sizeof(sTime) - 1, scale, WHITE);

    drawRect(xBtnX, yBtnX, wBtn, hBtn, WHITE);
    drawStr((xBtnX + wBtn / 2) - ((FONT_W + 1) * 1 - 1) / 2, yBtnX + 2, (uint8_t *)"X", 1, 1, WHITE);
    drawRect(xBtnOK, yBtnOK, wBtn, hBtn, WHITE);
    drawStr((xBtnOK + wBtn / 2) - ((FONT_W + 1) * 2 - 1) / 2, yBtnOK + 2, (uint8_t *)"OK", 2, 1, WHITE);
    display();
}

void drawSetting()
{
    drawEditClockFace();
    drawFunc = drawCursor;
}
void calcCursor(int16_t *_x, int16_t *_y, int16_t *_w, int16_t *_h)
{
    int16_t x = 0, y = 0, w = 0, h = 0;
    switch (cursor)
    {
    case 0:
        x = xDate + ((FONT_W + 1) * 2) * scale - 1;
        y = yDate - 1;
        w = ((FONT_W + 1) * 2 - 1) * scale + 2;
        h = hDate + 2;
        break;
    case 1:
        x = xDate + ((FONT_W + 1) * 5) * scale - 1;
        y = yDate - 1;
        w = ((FONT_W + 1) * 2 - 1) * scale + 2;
        h = hDate + 2;
        break;
    case 2:
        x = xDate + ((FONT_W + 1) * 8) * scale - 1;
        y = yDate - 1;
        w = ((FONT_W + 1) * 2 - 1) * scale + 2;
        h = hDate + 2;
        break;
    case 3:
        x = xTime + ((FONT_W + 1) * 0) * scale - 1;
        y = yTime - 1;
        w = ((FONT_W + 1) * 2 - 1) * scale + 2;
        h = hTime + 2;
        break;
    case 4:
        x = xTime + ((FONT_W + 1) * 3) * scale - 1;
        y = yTime - 1;
        w = ((FONT_W + 1) * 2 - 1) * scale + 2;
        h = hTime + 2;
        break;
    case 5:
        x = xTime + ((FONT_W + 1) * 6) * scale - 1;
        y = yTime - 1;
        w = ((FONT_W + 1) * 2 - 1) * scale + 2;
        h = hDate + 2;
        break;
    case 6:
        x = xBtnX;
        y = yBtnX;
        w = wBtn;
        h = hBtn;
        break;
    case 7:
        x = xBtnOK;
        y = yBtnOK;
        w = wBtn;
        h = hBtn;
        break;
    default:
        break;
    }
    *_x = x;
    *_y = y;
    *_w = w;
    *_h = h;
}

void drawEdit()
{
    drawEditClockFace();
    drawFunc = drawCursor;
}

void drawCursor()
{
    if (isClockFaceDirty)
    {
        drawEditClockFace();
        isClockFaceDirty = 0;
    }
    int16_t x, y, w, h;
    calcCursor(&x, &y, &w, &h);
    if (frameCount % 15 == 0)
    {
        if (state == STATE_EDITING || state == STATE_EXITING_EDIT)
            fillRect(x, y + h, w, 2, INVERSE);
        else
            fillRect(x, y, w, h, INVERSE);
    }
    display();
}

void setup()
{

    Serial.begin(115200);

    timer1SetClockSource(Timer1_Clock_1_1024);
    timer1SetCountType(Timer1_Count_Normal);
    timer1EnableOverflowInterrupt();

    initMyTWI(100000);
    initSPIMaster(MySPI_Fosc_1_2);

    MYPIN_READ_MODE_PULLUP(PIN_L);
    MYPIN_READ_MODE_PULLUP(PIN_R);

    MYPIN_PCINT_MASK1(PIN_L);
    MYPIN_PCINT_MASK1(PIN_R);
    myPinChangeInterruptEnableUsed();
    myPinChangeInterruptFunction(handleBtnInterrupt, handleBtnInterrupt, handleBtnInterrupt);

    initSSD1306();
    initDS1307();
    setScreenDir(2);

    TWIScanDevices();

    drawFunc = drawClock;
    frameCount = 0;
}

void loop()
{
    frameCount++;
    onBtnClick();
    drawFunc();
    delay(20);
}