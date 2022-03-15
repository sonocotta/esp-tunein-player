#ifndef SPOTIFY_UI_H
#define SPOTIFY_UI_H

#ifdef TFT_ENABLED
#include "TFT_eSPI.h"
#define TFT_TN_GREEN tft->color565(0x14, 0xd8, 0xcc)
#define TFT_TEXT_COLOR TFT_YELLOW
#include <menuIO/TFT_eSPIOut.h>
#endif

#define MENU_MAX_DEPTH 10
#include <menu.h>
#include <menuIO/serialOut.h>
#include <menuIO/serialIn.h>
#include <menuIO/chainStream.h>

#ifdef CONTROL_ROTARY_ENC
#include <ClickEncoder.h>
#include <menuIO/clickEncoderIn.h>
#endif

#include "ui/progressbar.h"
#include "tuneinapi.h"

#define FONT_SIZE 1
// font No 2
#define FONT_W (8 * FONT_SIZE)
#define FONT_H (16 * FONT_SIZE)

// define menu colors --------------------------------------------------------
#define Black RGB565(0, 0, 0)
#define Red RGB565(255, 0, 0)
#define Green RGB565(0, 255, 0)
#define Blue RGB565(0, 0, 255)
#define Gray RGB565(128, 128, 128)
#define LighterRed RGB565(255, 150, 150)
#define LighterGreen RGB565(150, 255, 150)
#define LighterBlue RGB565(150, 150, 255)
#define LighterGray RGB565(211, 211, 211)
#define DarkerRed RGB565(150, 0, 0)
#define DarkerGreen RGB565(0, 150, 0)
#define DarkerBlue RGB565(0, 0, 150)
#define Cyan RGB565(0, 255, 255)
#define Magenta RGB565(255, 0, 255)
#define Yellow RGB565(255, 255, 0)
#define White RGB565(255, 255, 255)
#define DarkerOrange RGB565(255, 140, 0)

// TFT color table
const colorDef<uint16_t> colors[6] MEMMODE = {
    //{{disabled normal,disabled selected},{enabled normal,enabled selected, enabled editing}}
    {{(uint16_t)Black, (uint16_t)Black}, {(uint16_t)Black, (uint16_t)Red, (uint16_t)Red}},     // bgColor
    {{(uint16_t)White, (uint16_t)White}, {(uint16_t)White, (uint16_t)White, (uint16_t)White}}, // fgColor
    {{(uint16_t)Red, (uint16_t)Red}, {(uint16_t)Yellow, (uint16_t)Yellow, (uint16_t)Yellow}},  // valColor
    {{(uint16_t)White, (uint16_t)White}, {(uint16_t)White, (uint16_t)White, (uint16_t)White}}, // unitColor
    {{(uint16_t)White, (uint16_t)Gray}, {(uint16_t)Black, (uint16_t)Red, (uint16_t)White}},    // cursorColor
    {{(uint16_t)White, (uint16_t)Yellow}, {(uint16_t)Black, (uint16_t)Red, (uint16_t)Red}},    // titleColor
};

enum UIState
{
    WifiConnecting,
    InfoScreen,
    Root,
    MainMenu,
};

// enum UIControlEvent
// {
//     KEY_RELEASE,
//     KEY_UP,
//     KEY_DOWN,
//     KEY_RIGHT,
//     KEY_LEFT,
// };

class TuneinUI
{
public:
    TuneinUI(TuneinApi *);
    void Init();
    void SetState(UIState);
    void GiveUp(const char *);

    // void SetSongName(String);
    // void SetArtistName(String);
    // void SetProgress(uint32_t, uint32_t);
    void SetProgressBar(uint8_t, String);
    // void SetAlbumArt(const char *);

    // void UpdateMenu(uint16_t, UIMenuItem *);
    // void ControlEvent(UIControlEvent);
    void Loop(void);

private:
    TuneinApi *api;
    ProgressBar *progress;
    TFT_eSPI *tft;

    const char *TAG = "ui";
    UIState state;
    UIMenuItem *items = NULL;
    uint16_t items_count = 0;
    uint16_t selected_index = 0;
    UIMenuItem *parent = NULL;

    // navigation inputs
    serialIn *serial_in;
    menuIn *inputsList[1];
    chainStream<1> *ins;

// #ifdef CONTROL_ROTARY_ENC
//     ClickEncoder clickEncoder = ClickEncoder(TEST_ROTARY_ENC_A, TEST_ROTARY_ENC_B, TEST_ROTARY_ENC, 4);
//     ClickEncoderStream encStream(clickEncoder, 1);
//     void IRAM_ATTR onTimer();
// #endif

    // navigation outputs
    const panel panels[1] = {{0, 0, TFT_HEIGHT / FONT_W, TFT_WIDTH / FONT_H}};
    navNode *nodes[sizeof(panels) / sizeof(panel)];
    panelsList* pList;
    
    idx_t eSpiTops[MENU_MAX_DEPTH];
    TFT_eSPIOut *eSpiOut;

    idx_t tops[MENU_MAX_DEPTH];
    serialOut *serial_out;

    // outputs
    menuOut *outList[2];
    outputsList *outs;

    prompt *mainData[1];

    // menu elements
    const char* title_1 = "<Exit.";

    menuNode* mainMenu;
    navNode path[MENU_MAX_DEPTH];
    navRoot* nav;

    String prettyBytes(uint32_t bytes);
    void renderMenu();
    bool loadItems(String);
};

#endif