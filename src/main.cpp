#include <Arduino.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include "main.h"
#include "tuneinapi.h"
#include "tuneinui.h"
#include <string>

const char *TAG = "main";

WiFiMulti wifiMulti;
TuneinApi *api = new TuneinApi();
TuneinUI *ui = new TuneinUI(api);

// #ifdef CONTROL_JOYSTICK
// #include "controls/joystick.h"
// TuneinJoystick *joystick = new TuneinJoystick(ui, CONTROL_JOYSTICK_A, CONTROL_JOYSTICK_B);
// #endif

uint8_t temp = 0;

/**
 * Setup
 */
void setup()
{
    Serial.begin(SERIAL_BAUD);
    while (!Serial)
        ;
    ESP_LOGI(TAG, "Starting...");

    ui->Init();
    ui->SetState(UIState::WifiConnecting);
    ui->SetProgressBar(20, "connecting to wifi");

    WiFi.setHostname(CONFIG_DEVICE_NAME);
    WiFi.mode(WIFI_STA);
    for (auto i : AP_LIST)
    {
        wifiMulti.addAP(i.ssid, i.passphrase);
    }

    uint8_t count = 20;
    while (count-- && (wifiMulti.run() != WL_CONNECTED))
    {
        ui->SetProgressBar(100 - count * 4, "connecting...");
        delay(250);
    }

    if (!WiFi.isConnected())
        ui->GiveUp("Unable to connect to WiFi");

    // MDNS.addService("http", "tcp", 80);

    ui->SetState(UIState::InfoScreen);

    ui->SetState(UIState::Root);
    // // if (api->HaveRefreshToken())
    // // {
    // //     ui->SetState(UIState::Ready);
    // //     api->SetState(SptfActions::CurrentlyPlaying);
    // // }
    // // else
    // // {
    // //     ui->SetState(UIState::AuthRequired);
    // // }
}

uint8_t count = 0;

/**
 * Main loop
 */
void loop()
{
    // #ifdef CONTROL_JOYSTICK
    //     joystick->Poll();
    // #endif
    // // api->Loop();
    // char* val = std::to_string(temp);//itoa(temp, );
    // ui->tft->setTextColor(TFT_YELLOW);
    // ui->progress->setText("balh");
    // ui->progress->setProgress(temp % 110);

    ui->Loop();

    // temp++;

    if (count++ % 50 == 0)
        ESP_LOGI(TAG, "Free heap: %d", ESP.getFreeHeap());

    delay(200);
}