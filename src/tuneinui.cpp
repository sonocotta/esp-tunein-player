#include "tuneinui.h"

#include <SPI.h>
#include <SPIFFS.h>
#include <WiFi.h>

void setLed() {}
void op1Func() {}

TuneinUI::TuneinUI(TuneinApi *_api)
{
    this->api = _api;
#ifdef TFT_ENABLED
    this->tft = new TFT_eSPI();
    this->progress = new ProgressBar(this->tft);
    this->progress->setColor(TFT_TN_GREEN, TFT_BLACK);
    this->progress->setFont(&FreeMono9pt7b);
    this->progress->setTextColor(TFT_TEXT_COLOR);
    this->progress->setTextSize(1);
#else
    this->progress = new ProgressBar();
#endif

    // input
    serial_in = new serialIn(Serial);
    inputsList[0] = serial_in;
    ins = new chainStream<1>(inputsList);

    // tft output
    // panels[0] = {0, 0, TFT_HEIGHT / FONT_W, TFT_WIDTH / FONT_H};
    pList = new panelsList(panels, nodes, sizeof(panels) / sizeof(panel));

    // idx_t eSpiTops[MENU_MAX_DEPTH] = {0};
    eSpiOut = new TFT_eSPIOut(*(this->tft), colors, eSpiTops, *pList, FONT_W, FONT_H + 1);

    // serial output -------------------------------------
    serial_out = new serialOut(Serial, tops);

    // outputs -------------------------------------
    outList[0] = serial_out;
    outList[1] = eSpiOut;
    outs = new outputsList(outList, sizeof(outList) / sizeof(menuOut *));

    // // choose field and options -------------------------------------
    // int duration = 0; // target var
    // prompt *durData[] = {
    //     new menuValue<int>("Off", 0),
    //     new menuValue<int>("Short", 1),
    //     new menuValue<int>("Medium", 2),
    //     new menuValue<int>("Long", 3)};
    // choose<int> &durMenu = *new choose<int>("Duration", duration, sizeof(durData) / sizeof(prompt *), durData);

    // // select field and options -------------------------------------
    // enum Fxs
    // {
    //     Fx0,
    //     Fx1,
    //     Fx2
    // } selFx; // target var

    // prompt *fxData[] = {
    //     new menuValue<Fxs>("Default", Fx0),
    //     new menuValue<Fxs>("Pop", Fx1),
    //     new menuValue<Fxs>("Rock", Fx2)};

    // Menu::select<Fxs> &fxMenu = *new Menu::select<Fxs>("Fx", selFx, sizeof(fxData) / sizeof(prompt *), fxData);

    // // toggle field and options -------------------------------------
    // bool led = false; // target var

    // prompt *togData[] = {
    //     new menuValue<bool>("On", true),
    //     new menuValue<bool>("Off", false)};
    // toggle<bool> &ledMenu = *new toggle<bool>("LED:", led, sizeof(togData) / sizeof(prompt *), togData, (Menu::callback)setLed, enterEvent);

    // // the submenu -------------------------------------
    // prompt *subData[] = {
    //     new prompt("Sub1"),
    //     new prompt("Sub2"),
    //     new Exit("<Back")};

    // menuNode &subMenu = *new menuNode("sub-menu", sizeof(subData) / sizeof(prompt *), subData);

    // // pad menu --------------------
    // uint16_t year = 2017;
    // uint16_t month = 10;
    // uint16_t day = 7;

    // prompt *padData[] = {
    //     new menuField<typeof(year)>(year, "", "", 1900, 3000, 20, 1, doNothing, noEvent),
    //     new menuField<typeof(month)>(month, "/", "", 1, 12, 1, 0, doNothing, noEvent),
    //     new menuField<typeof(day)>(day, "/", "", 1, 31, 1, 0, doNothing, noEvent)};

    // menuNode &padMenu = *new menuNode(
    //     "Date",
    //     sizeof(padData) / sizeof(prompt *),
    //     padData,
    //     doNothing,
    //     noEvent,
    //     noStyle,
    //     (systemStyles)(_asPad | Menu::_menuData | Menu::_canNav | _parentDraw));

    // // the main menu -------------------------------------

    // uint8_t test = 55; // target var for numerical range field

    // edit text field info
    // const char *hexDigit = "0123456789ABCDEF";            // a text table
    // const char *hexNr[] = {"0", "x", hexDigit, hexDigit}; // text validators
    // char buf1[] = "0x11";

    // // menuNode &mainMenu = *new menuNode("Tune In", doNothing, noEvent, wrapStyle /*,
    // //     OP("Cut!", doRunCuts, enterEvent) */
    // // );
    //mainData[0] = new prompt(F("Op 2"));
    //mainData[1] = new textField("Addr", buf1, 4, (char *const *)hexNr);
    //     &subMenu,
    //     &durMenu,
    //     &fxMenu,
    //     &ledMenu,
    //     &padMenu,
    mainData[0] = new Exit(title_1);

    mainMenu = new menuNode("Main menu", sizeof(mainData) / sizeof(prompt *), mainData /*,doNothing,noEvent,wrapStyle*/);
    nav = new navRoot(*mainMenu, path, MENU_MAX_DEPTH, *ins, *outs);
}

void TuneinUI::Init()
{
#ifdef TFT_ENABLED
    tft->init();
    tft->setRotation(TFT_ROTATION);
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_TN_GREEN);
#endif

    if (!SPIFFS.begin())
    {
        GiveUp("Unable to begin SPIFFS");
    }

    ESP_LOGI(TAG, "initialized");
}

void TuneinUI::SetProgressBar(uint8_t perc, String text)
{
    progress->setText(text.c_str());
    progress->setProgress(perc);
}

void TuneinUI::SetState(UIState newState)
{
    this->state = newState;

    switch (this->state)
    {
    case UIState::WifiConnecting:
    {
#ifdef TFT_ENABLED
        tft->drawBmpFile(SPIFFS, "/logo-color.bmp", 0, 0);

        tft->setFreeFont(&FreeSansBoldOblique12pt7b);
        tft->setTextSize(1);
        tft->setTextDatum(TC_DATUM);
        tft->drawString(CONFIG_DEVICE_NAME, 160, 10);

        tft->setFreeFont(&FreeSans9pt7b);
        tft->setTextSize(1);
        tft->setTextDatum(BC_DATUM);
#endif
        ESP_LOGI(TAG, "Connecting to WiFi...");
    }
    break;

    case InfoScreen:
    {
#ifdef TFT_ENABLED
        tft->fillScreen(TFT_BLACK);
        tft->drawBmpFile(SPIFFS, "/logo-color-d.bmp", 0, 0);

        tft->setFreeFont(&FreeSansBoldOblique12pt7b);
        tft->setTextColor(TFT_TN_GREEN);
        tft->setTextSize(1);
        tft->setTextDatum(TC_DATUM);
        tft->drawString(CONFIG_DEVICE_NAME, 160, 10);

        tft->setFreeFont(&FreeMono9pt7b);
        tft->setTextColor(TFT_TN_GREEN);
        tft->setTextSize(1);
        tft->setCursor(0, 75);
        tft->printf(" SSID:      %s\n", WiFi.SSID().c_str());
        tft->printf(" IP:        %s\n", WiFi.localIP().toString().c_str());
        tft->printf(" STA MAC:   %s\n", WiFi.macAddress().c_str());
        tft->printf(" AP MAC:    %s\n", WiFi.softAPmacAddress().c_str());
        tft->printf(" Chip size: %s\n", prettyBytes(ESP.getFlashChipSize()).c_str());
        tft->printf(" Free heap: %s\n", prettyBytes(ESP.getFreeHeap()).c_str());
#endif
        ESP_LOGI(TAG, " SSID:      %s", WiFi.SSID().c_str());
        ESP_LOGI(TAG, " IP:        %s", WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, " STA MAC:   %s", WiFi.macAddress().c_str());
        ESP_LOGI(TAG, " AP MAC:    %s", WiFi.softAPmacAddress().c_str());
        ESP_LOGI(TAG, " Chip size: %s", prettyBytes(ESP.getFlashChipSize()).c_str());
        ESP_LOGI(TAG, " Free heap: %s", prettyBytes(ESP.getFreeHeap()).c_str());
    }
    break;

    case Root:
    {
        this->SetProgressBar(40, "loading root");
        // if (!loadItems(API_ROOT_ID))
        // {
        //     ESP_LOGE(TAG, "Error loading categories: %s", API_ROOT_ID);
        // }
        // else
        // {
        //     this->SetProgressBar(60, "rendering root");
        //     ESP_LOGD(TAG, "returned %d items", items_count);

        //     // this->UpdateMenu(count, results);
        //     this->SetProgressBar(80, "rendering menu");
        //     this->SetState(UIState::MainMenu);
        // }
    }
    break;

    case MainMenu:
    {
#ifdef TFT_ENABLED
        tft->fillScreen(TFT_BLACK);
        tft->drawBmpFile(SPIFFS, "/logo-color-d.bmp", 0, 0);
#endif
        // renderMenu();
    }
    break;
    }
}

bool TuneinUI::loadItems(String id)
{
    ESP_LOGD(TAG, "Loading category by id: %s", id);
    if (this->items != NULL)
    {
        free(this->items);
    }

    this->items = api->LoadItems(id, &items_count);
    this->selected_index = 0;

    return true;
}

// void TuneinUI::UpdateMenu(uint16_t count, UIMenuItem *items)
// {
//     if (this->items != NULL)
//         delete this->items;

//     this->items = items;
//     this->items_count = count;
//     this->selected_index = 0;
// }

void TuneinUI::GiveUp(const char *errMsg)
{
    /// ESP_LOGE(TAG, errMsg);

#ifdef TFT_ENABLED
    tft->setFreeFont(&FreeSans12pt7b);
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(1);
    tft->setTextDatum(CC_DATUM);
    tft->fillScreen(TFT_RED);
    tft->drawString(errMsg, tft->width() >> 1, tft->height() >> 1);
#endif

    while (true)
    {
        yield();
    }
}

String TuneinUI::prettyBytes(uint32_t bytes)
{

    const char *suffixes[7] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    uint8_t s = 0;
    double count = bytes;

    while (count >= 1024 && s < 7)
    {
        s++;
        count /= 1024;
    }
    if (count - floor(count) == 0.0)
    {
        return String((int)count) + suffixes[s];
    }
    else
    {
        return String(round(count * 10.0) / 10.0, 1) + suffixes[s];
    };
}

// void TuneinUI::ControlEvent(UIControlEvent evt)
// {
//     bool render = false;

//     switch (evt)
//     {
//     case KEY_UP:
//     {
//         if (selected_index == 0)
//             selected_index = items_count - 1;
//         else
//             selected_index -= 1;
//         render = true;
//     }
//     break;

//     case KEY_DOWN:
//     {
//         if (selected_index == items_count - 1)
//             selected_index = 0;
//         else
//             selected_index += 1;
//         render = true;
//     }
//     break;

//     case KEY_RIGHT:
//     {
//         ESP_LOGD(TAG, "right");
//         auto selected = &(items[selected_index]);
//         if (!loadItems(selected->id))
//         {
//             ESP_LOGE(TAG, "Error loading items: %s", selected->id);
//         }
//         else
//         {
//             // this->SetProgressBar(60, "rendering root");
//             ESP_LOGD(TAG, "returned %d items", items_count);

//             // this->UpdateMenu(count, results);
//             //  this->SetProgressBar(80, "rendering menu");
//             //  this->SetState(UIState::Menu);
//             this->parent = selected;
//         }
//         render = true;
//     }
//     break;

//     case KEY_LEFT:
//     {
//         ESP_LOGD(TAG, "left");
//         auto id = parent->id;
//         if (!loadItems(id))
//         {
//             ESP_LOGE(TAG, "Error loading items: %s", id);
//         }
//         else
//         {
//             // this->SetProgressBar(60, "rendering root");
//             ESP_LOGD(TAG, "returned %d items", items_count);

//             // this->UpdateMenu(count, results);
//             //  this->SetProgressBar(80, "rendering menu");
//             //  this->SetState(UIState::Menu);
//         }
//         render = true;
//     }
//     break;

//     case KEY_RELEASE:
//         break;
//     }

//     if (render)
//         // this->renderMenu();
//         this->SetState(UIState::MainMenu);
// }

void TuneinUI::renderMenu()
{
    // tft->fillScreen(TFT_BLACK);
    // tft->drawBmpFile(SPIFFS, "/logo-color-d.bmp", 0, 0);

#ifdef TFT_ENABLED
    tft->setTextColor(TFT_TN_GREEN);
    tft->setFreeFont(&FreeMono12pt7b);
    tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);

    // Header line
    tft->setCursor(0, FreeMono12pt7b.yAdvance);
    if (parent == NULL)
    {
        // tft->println("\n");
    }
    else
    {
        tft->printf("< %s", parent->text);
    }
    tft->drawLine(0, FreeMono12pt7b.yAdvance + 4, tft->width(), FreeMono12pt7b.yAdvance + 4, TFT_TN_GREEN);
#endif

    for (uint16_t i = 0; i < items_count; i++)
    {
#ifdef TFT_ENABLED
        tft->setCursor(0, FreeMono12pt7b.yAdvance * (i + 2));
        tft->printf("%s %s", items[i].text, (selected_index == i) ? "<" : " ");
#endif
        ESP_LOGI(TAG, "%s %s", items[i].text, (selected_index == i) ? "<" : "");
    }
}

void TuneinUI::Loop(void)
{
    nav->poll();
}

// void TuneinUI::SetSongName(String name)
// {
// // Display song name
// #ifdef TFT_ENABLED
//     tft->fillRect(0, 0, 320, 30, 0xffffff);
//     tft->setTextColor(TFT_BLACK);
//     tft->setTextFont(2);
//     tft->setTextSize(1);
//     tft->setTextDatum(TC_DATUM);
//     tft->drawString(name, 160, 2);
// #endif

//     ESP_LOGI(TAG, "Song name: %s", name);
// }

// void TuneinUI::SetArtistName(String name)
// {
// #ifdef TFT_ENABLED
//     // Display artists names
//     tft->setTextFont(1);
//     tft->setTextSize(1);
//     tft->setTextDatum(BC_DATUM);
//     tft->drawString(name, 160, 28);
// #endif

//     ESP_LOGI(TAG, "Artist name: %s", name);
// }

// void TuneinUI::SetProgress(uint32_t progress, uint32_t duration)
// {
// #ifdef TFT_ENABLED
//     // Display progress bar background
//     tft->fillRect(0, 235, 320, 5, TFT_WHITE); // TODO: only when changed
//     tft->fillRect(0, 235, ceil((float)320 * ((float)progress / duration)), 5, TFT_TN_GREEN);
// #endif

//     ESP_LOGI(TAG, "Progress: %d / %d", progress, duration);
// }

// void TuneinUI::SetAlbumArt(const char *filename)
// {
// #ifdef TFT_ENABLED
//     // Display album art
//     tft->fillScreen(TFT_WHITE);
//     tft->drawJpgFile(SPIFFS, filename, 10, 30);
// #endif

//     ESP_LOGI(TAG, "Album art: %s", filename);
// }