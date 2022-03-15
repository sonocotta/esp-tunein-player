#include "progressbar.h"

#ifdef TFT_ENABLED
ProgressBar::ProgressBar(TFT_eSPI *_tft)
{
    this->tft = _tft;
}
#else
ProgressBar::ProgressBar(TFT_eSPI *_tft)
{
}
#endif

void ProgressBar::setColor(uint16_t _fgcolor, uint16_t _bgcolor)
{
    this->fgcolor = _fgcolor;
    this->bgcolor = _bgcolor;
}

void ProgressBar::setText(const char *_text)
{
    this->text = _text;
}

void ProgressBar::setTextColor(uint16_t _tcolor)
{
    this->tcolor = _tcolor;
}

void ProgressBar::setTextSize(uint8_t _tsize)
{
    this->tsize = _tsize;
}

void ProgressBar::setFont(const GFXfont *_font)
{
    this->font = (GFXfont *)_font;
}

void ProgressBar::setProgress(uint8_t perc)
{
    if (perc > 100)
        perc = 100;

    uint16_t barwidth = tft->width() - (padding * 2);
    uint16_t fillwidth = (barwidth - 4) * perc / 100;
    // uint16_t x0 =
    uint16_t y0 = tft->height() - padding - height;

    tft->drawRect(padding, y0, barwidth, height, this->fgcolor);
    // tft->fillRect(padding + 1, y0 + 1 , barwidth - 2, height - 2, this->bgcolor);
    tft->fillRect(padding + 2, y0 + 2, fillwidth, height - 4, this->fgcolor);
    tft->fillRect(padding + 2 + fillwidth, y0 + 2, barwidth - fillwidth - 4, height - 4, this->bgcolor);

    if (this->text != NULL)
    {
        tft->setFreeFont(this->font);
        tft->setTextSize(this->tsize);
        tft->setTextColor(this->tcolor);
        tft->setTextDatum(BC_DATUM);
        tft->drawString(this->text, tft->width() >> 1, y0 + font->yAdvance);
    }
}