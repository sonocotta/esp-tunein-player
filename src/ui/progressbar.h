#ifndef UI_PROGRESSBAR_H
#define UI_PROGRESSBAR_H

#ifdef TFT_ENABLED
#include "TFT_eSPI.h"
#endif

class ProgressBar
{
public:
#ifdef TFT_ENABLED
    ProgressBar(TFT_eSPI *);
#else
    ProgressBar();
#endif
    void setColor(uint16_t, uint16_t);
    void setTextColor(uint16_t);
    void setTextSize(uint8_t);
    void setText(const char *);
    void setProgress(uint8_t);
    void setFont(const GFXfont *);

    uint8_t padding = 16;
    uint8_t height = 20;

private:
    const char *TAG = "progress";
    TFT_eSPI *tft;
    GFXfont * font;
    uint16_t fgcolor = TFT_WHITE, bgcolor = TFT_BLACK;
    uint16_t tcolor = TFT_WHITE;
    uint8_t tsize = 1;
    const char* text = NULL;
};

#endif