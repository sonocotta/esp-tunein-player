#ifndef SPOTIFY_API_H
#define SPOTIFY_API_H

#include <tinyopml.h>
#include <WString.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

using namespace tinyopml;

#ifdef BOARD_HAS_PSRAM
#include "esp32-hal-psram.h"
#define alloc ps_malloc
#else
#define alloc malloc
#endif

#define API_HOST "http://opml.radiotime.com"
#define API_PORT 80
#define API_ROOT_ID "r0"
#define API_MAX_URL_LEN 128
// #define min(X, Y) (((X)<(Y))?(X):(Y))
// #define startsWith(STR, SEARCH) (strncmp(STR, SEARCH, strlen(SEARCH)) == 0)

// typedef struct
// {
//     int httpCode;
//     String payload;
// } HTTP_response_t;

enum UIMenuItemType
{
    LINK,
    AUDIO,
    UNKNOWN
};

struct UIMenuItem
{
    UIMenuItemType type;
    char id[8];
    char text[32];
    char url[64];
};

enum client_result
{
    UNDEFINED,
    OPML_OK,
    HTTP_UNSUCCESSFUL,
    HTTP_FAILED,
    OPML_PARSE_ERR,
    OPML_UNEXPECTED_STRUCTURE,
};

class TuneinApi
{
public:
    TuneinApi(/*TuneinUI *ui*/);

    // // webserver part
    // void RegisterPaths();

    // void eventsSendLog(const char *logData, EventsLogTypes type = log_line);
    // void eventsSendInfo(const char *msg, const char *payload = "");
    // void eventsSendError(int code, const char *msg, const char *payload = "");

    // API part
    // void Init();
    // bool HaveRefreshToken();
    // void SetState(SptfActions);
    // void Loop();

    // void Previous();
    // void Next();
    // void Toggle();
    // void CurrentlyPlaying();
    // void DisplayAlbumArt(String);
    UIMenuItem *LoadItems(String, uint16_t *);
    client_result LoadOpml(const char *, OPMLDocument *);

private:
    const char *TAG = "api";
    // bool send_events = true;

    // TuneinUI *ui;
    HTTPClient client;
    // TuneinApi *api;
    // AsyncWebServer *server;
    // AsyncEventSource *events;

    // char auth_state[17];
    // String auth_state_returned;
    // String auth_code;
    // String access_token;
    // String refresh_token;

    // uint32_t token_lifetime_ms = 0;
    // uint32_t token_millis = 0;
    // uint32_t last_curplay_millis = 0;
    // uint32_t next_curplay_millis = 0;

    // bool getting_token = false;
    // bool ota_in_progress = false;
    // bool sptf_is_playing = true;

    // SptfActions State = Idle;

    // void GetToken(const String &code, GrantTypes grant_type = GrantTypes::gt_refresh_token);
    // HTTP_response_t ApiRequest(const char *method, const char *endpoint, const char *content = "");

    // void deleteRefreshToken();
    // String readRefreshToken();
    // void writeRefreshToken();
    // HTTP_response_t httpRequest(const char *host, uint16_t port, const char *headers, const char *content = "");
};

#endif