#include "tuneinapi.h"
// #include <base64.h>
// #include <EEPROM.h>
// #include <SPIFFS.h>
// #include <ArduinoJson.h>

TuneinApi::TuneinApi()
{
    // this->server = new AsyncWebServer(port);
    // this->events = new AsyncEventSource("/events");
    // this->ui = _ui;
    // this->client.setInsecure(); // shouldn't do this
}

// void TuneinApi::Init()
// {
//     this->ui->SetProgressBar(40, "loading root");
//     uint16_t count = 0;
//     ESP_LOGD(TAG, "Loading category by id: %d", API_ROOT_ID);
//     UIMenuItem *results = TuneinApi::LoadItems(API_ROOT_ID, &count);

//     if (!results)
//     {
//         ESP_LOGE(TAG, "Error loading categories: %d", results);
//     }
//     else
//     {
//         this->ui->SetProgressBar(60, "rendering root");
//         ESP_LOGD(TAG, "returned %d categories", count);

//         this->ui->UpdateMenu(count, results);
//         this->ui->SetProgressBar(80, "rendering menu");
//         this->ui->SetState(UIState::Menu);

//         // delete results;
//     }
// }

UIMenuItem *TuneinApi::LoadItems(String categoryId, uint16_t *length)
{
    char url[API_MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/Browse.ashx?id=%s", API_HOST, categoryId);
    OPMLDocument doc;
    auto err = TuneinApi::LoadOpml(url, &doc);
    if (err != OPML_OK)
    {
        ESP_LOGE(TAG, "Error loading categories: %d", err);
        return NULL;
    }
    else
    {
        OPMLNode *root = doc.FirstChildElement("opml");
        if (root == NULL)
        {
            ESP_LOGE(TAG, "Err: root is null");
            return NULL;
        }

        OPMLNode *body = root->FirstChildElement("body");
        if (body == NULL)
        {
            ESP_LOGE(TAG, "Err: body is null");
            return NULL;
        }

        uint16_t count = 0;
        for (OPMLElement *cat = body->FirstChildElement("outline"); cat; cat = cat->NextSiblingElement("outline"))
        {
            count++;
        }

        ESP_LOGD(TAG, "%d elements found in the document", count);

        UIMenuItem *result = (UIMenuItem *)alloc(sizeof(UIMenuItem) * count);

        count = 0;
        for (OPMLElement *cat = body->FirstChildElement("outline"); cat; count++, cat = cat->NextSiblingElement("outline"))
        {
            auto type_s = cat->Attribute("type");
            auto type = (strcmp(type_s, "link") == 0) ? LINK : ((strcmp(type_s, "audio") == 0) ? AUDIO : UNKNOWN);
            result[count] = (UIMenuItem){
                .type = type
            };
            strcpy(result[count].id, cat->Attribute("guide_id"));
            strcpy(result[count].text, cat->Attribute("text"));
            strcpy(result[count].url, cat->Attribute("URL"));
        }

        *length = count;

        return result;
    }
}

client_result TuneinApi::LoadOpml(const char *url, OPMLDocument *doc)
{
    ESP_LOGD(TAG, "get %s", url);

    client.begin(url);
    int httpCode = client.GET();
    if (httpCode > 0)
    {
        ESP_LOGD(TAG, "[HTTP] GET... code: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK)
        {
            String payload = client.getString();
            // ESP_LOGD(TAG, "<- %s", payload.c_str());

            auto err = doc->Parse(payload.c_str());
            if (err != OPML_SUCCESS)
            {
                ESP_LOGE(TAG, "Error parsing opml: %d", err);
                return OPML_PARSE_ERR;
            }

            return OPML_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Response code is not successful: %d", httpCode);
            return HTTP_UNSUCCESSFUL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "[HTTP] GET... failed, error: %s\n", client.errorToString(httpCode).c_str());
        return HTTP_FAILED;
    }

    client.end();
    return UNDEFINED;
}

// /**
//  * Base 64 encode
//  *
//  * @param str
//  * @return
//  */
// String b64Encode(String str)
// {
//     String encodedStr = base64::encode(str);
//     // Remove unnecessary linefeeds
//     int idx = -1;
//     while ((idx = encodedStr.indexOf('\n')) != -1)
//     {
//         encodedStr.remove(idx, 1);
//     }
//     return encodedStr;
// }

// void generateRandomString(char* str, uint8_t length)
// {
//     for (uint8_t i = 0; i < length; i++)
//     {
//         uint8_t ix = random(25);
//         str[i] = (char)('a' + ix);
//     }

//     str[length] = '\0';
// }

// void TuneinApi::RegisterPaths()
// {
//     //-----------------------------------------------
//     // Initialize HTTP server handlers
//     //-----------------------------------------------
//     events->onConnect([](AsyncEventSourceClient *client)
//                       { ESP_LOGD(TAG, "\n> [%d] events->onConnect\n", micros()); });

//     server->addHandler(events);

//     server->serveStatic("/favicon.ico", SPIFFS, "/favicon.ico");

//     server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         ESP_LOGD(TAG, "server->on /");

//         if (this->access_token == "" && !this->getting_token)
//         {
//             char auth_url[300] = "";
//             getting_token = true;

//             //generateRandomString(&auth_state[0], 16);
//             snprintf(auth_url, sizeof(auth_url),
//                     "https://accounts.spotify.com/authorize/"
//                     "?response_type=code"
//                     "&scope=user-read-private+user-read-currently-playing+user-read-playback-state+user-modify-playback-state"
//                     "&redirect_uri=http%%3A%%2F%%2F" CONFIG_DEVICE_NAME ".lan%%2Fcallback%%2F"
//                     "&client_id=%s",
//                     //"&spotify_auth_state=%s",
//                     SPTF_CLIENT_ID/*, auth_state*/);
//             ESP_LOGD(TAG, "Redirect to: %s", auth_url);
//             request->redirect(auth_url);
//         }
//         else
//         {
//             request->send(SPIFFS, "/index.html");
//         }
//     });

//     server->on("/callback", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         auth_code = "";
//         auth_state_returned = "";
//         uint8_t paramsNr = request->params();
//         for (uint8_t i = 0; i < paramsNr; i++)
//         {
//             AsyncWebParameter *p = request->getParam(i);
//             // if (p->name() == "state")
//             // {
//             //     auth_state_returned = p->value();
//             // }
//             if (p->name() == "code")
//             {
//                 auth_code = p->value();
//                 this->SetState(SptfActions::GetToken);
//             }
//         }

//         // if (auth_state_returned != auth_state) {
//         //     ESP_LOGE(TAG, "State mismatch, sent: %s, returned: %s", auth_state, auth_state_returned);
//         // }

//         if (this->State == SptfActions::GetToken)
//         {
//             request->redirect("/");
//         }
//         else
//         {
//             request->send(204);
//         }
//     });

//     server->on("/next", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         this->SetState(SptfActions::Next);
//         request->send(204);
//     });

//     server->on("/previous", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         this->SetState(SptfActions::Previous);
//         request->send(204);
//     });

//     server->on("/toggle", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         this->SetState(SptfActions::Toggle);
//         request->send(204);
//     });

//     server->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request)
//     {
//         request->send(200, "text/plain", String(ESP.getFreeHeap()));
//     });

//     server->on("/resettoken", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         this->access_token = "";
//         this->refresh_token = "";
//         this->deleteRefreshToken();
//         api->SetState(SptfActions::Idle);
//         request->send(200, "text/plain", "Tokens deleted, M5Spot will restart");
//         uint32_t start = millis();
//         while (true)
//         {
//             if (millis() - start > 5000)
//             {
//                 ESP.restart();
//             }
//             yield();
//         }
//     });

//     server->on("/resetwifi", HTTP_GET, [](AsyncWebServerRequest *request)
//     {
//         WiFi.disconnect(true);
//         request->send(200, "text/plain", "WiFi credentials deleted, M5Spot will restart");
//         uint32_t start = millis();
//         while (true)
//         {
//             if (millis() - start > 5000)
//             {
//                 ESP.restart();
//             }
//             yield();
//         }
//     });

//     server->on("/toggleevents", HTTP_GET, [this](AsyncWebServerRequest *request)
//     {
//         this->send_events = !this->send_events;
//         request->send(200, "text/plain", this->send_events ? "1" : "0");
//     });

//     server->onNotFound([](AsyncWebServerRequest *request)
//     {
//         request->send(404);
//     });

//     server->begin();
// }

// /**
//  * Send log to browser
//  *
//  * @param logData
//  * @param event_type
//  */
// void TuneinApi::eventsSendLog(const char *logData, EventsLogTypes type)
// {
//     if (!send_events)
//         return;
//     events->send(logData, type == log_line ? "line" : "raw");
// }

// /**
//  * Send infos to browser
//  *
//  * @param msg
//  * @param payload
//  */
// void TuneinApi::eventsSendInfo(const char *msg, const char *payload)
// {
//     if (!send_events)
//         return;

//     DynamicJsonBuffer jsonBuffer(256);
//     JsonObject &json = jsonBuffer.createObject();
//     json["msg"] = msg;
//     if (strlen(payload))
//     {
//         json["payload"] = payload;
//     }

//     String info;
//     json.printTo(info);
//     events->send(info.c_str(), "info");
// }

// /**
//  * Send errors to browser
//  *
//  * @param errCode
//  * @param errMsg
//  * @param payload
//  */
// void TuneinApi::eventsSendError(int code, const char *msg, const char *payload)
// {
//     if (!send_events)
//         return;

//     DynamicJsonBuffer jsonBuffer(256);
//     JsonObject &json = jsonBuffer.createObject();
//     json["code"] = code;
//     json["msg"] = msg;
//     if (strlen(payload))
//     {
//         json["payload"] = payload;
//     }

//     String error;
//     json.printTo(error);
//     events->send(error.c_str(), "error");
// }

// bool TuneinApi::HaveRefreshToken()
// {
//     return refresh_token != "";
// }

// void TuneinApi::SetState(SptfActions newstate)
// {
//     this->State = newstate;
// }

// void TuneinApi::Loop()
// {
//     uint32_t cur_millis = millis();

//     // Refreh Spotify access token either on M5Spot startup or at token expiration delay
//     // The number of requests is limited to 1 every 5 seconds
//     if (refresh_token != "" && (token_millis == 0 || (cur_millis - token_millis >= token_lifetime_ms)))
//     {
//         static uint32_t gettoken_millis = 0;
//         if (cur_millis - gettoken_millis >= 5000)
//         {
//             this->GetToken(refresh_token);
//             gettoken_millis = cur_millis;
//         }
//     }

//     // Spotify action handler
//     switch (State)
//     {
//     case SptfActions::Idle:
//         break;
//     case SptfActions::GetToken:
//         this->GetToken(auth_code, gt_authorization_code);
//         break;
//     case SptfActions::CurrentlyPlaying:
//         if (next_curplay_millis && (cur_millis >= next_curplay_millis))
//         {
//             this->CurrentlyPlaying();
//         }
//         else if (cur_millis - last_curplay_millis >= SPTF_POLLING_DELAY)
//         {
//             this->CurrentlyPlaying();
//         }
//         break;
//     case SptfActions::Next:
//         this->Next();
//         break;
//     case SptfActions::Previous:
//         this->Previous();
//         break;
//     case SptfActions::Toggle:
//         this->Toggle();
//         break;
//     }
// }

// /**
//  * Spotify toggle pause/play
//  */
// void TuneinApi::Toggle()
// {
//     HTTP_response_t response = ApiRequest("PUT", sptf_is_playing ? "/pause" : "/play");
//     if (response.httpCode == 204)
//     {
//         sptf_is_playing = !sptf_is_playing;
//         next_curplay_millis = millis() + 200;
//     }
//     else
//     {
//         eventsSendError(response.httpCode, "tunein error", response.payload.c_str());
//     }
//     this->State = SptfActions::CurrentlyPlaying;
// };

// /**
//  * Spotify previous track
//  */
// void TuneinApi::Previous()
// {
//     HTTP_response_t response = ApiRequest("POST", "/previous");
//     if (response.httpCode == 204)
//     {
//         next_curplay_millis = millis() + 200;
//     }
//     else
//     {
//         eventsSendError(response.httpCode, "tunein error", response.payload.c_str());
//     }
//     this->State = SptfActions::CurrentlyPlaying;
// };

// /**
//  * Spotify next track
//  */
// void TuneinApi::Next()
// {
//     HTTP_response_t response = ApiRequest("POST", "/next");
//     if (response.httpCode == 204)
//     {
//         next_curplay_millis = millis() + 200;
//     }
//     else
//     {
//         eventsSendError(response.httpCode, "tunein error", response.payload.c_str());
//     }
//     this->State = SptfActions::CurrentlyPlaying;
// };

// /**
//  * Get information about the Spotify user's current playback
//  */
// void TuneinApi::CurrentlyPlaying()
// {
//     ESP_LOGD(TAG, "CurrentlyPlaying()");

//     last_curplay_millis = millis();
//     next_curplay_millis = 0;

//     HTTP_response_t response = ApiRequest("GET", "/currently-playing");

//     if (response.httpCode == 200)
//     {

//         DynamicJsonBuffer jsonBuffer(5120);

//         JsonObject &json = jsonBuffer.parse(response.payload);

//         if (json.success())
//         {
//             sptf_is_playing = json["is_playing"];
//             uint32_t progress_ms = json["progress_ms"];
//             uint32_t duration_ms = json["item"]["duration_ms"];

//             // Check if current song is about to end
//             if (sptf_is_playing)
//             {
//                 uint32_t remaining_ms = duration_ms - progress_ms;
//                 if (remaining_ms < SPTF_POLLING_DELAY)
//                 {
//                     // Refresh at the end of current song,
//                     // without considering remaining polling delay
//                     next_curplay_millis = millis() + remaining_ms + 200;
//                 }
//             }

//             // Get song ID
//             const char *id = json["item"]["id"];
//             static char previousId[32] = {0};

//             // If song has changed, refresh display
//             if (strcmp(id, previousId) != 0)
//             {
//                 strncpy(previousId, id, sizeof(previousId));

//                 // Display album art
//                 this->DisplayAlbumArt(json["item"]["album"]["images"][1]["url"]);

//                 this->ui->SetSongName(json["item"]["name"].as<String>());

//                 JsonArray &arr = json["item"]["artists"];
//                 String artists;
//                 artists.reserve(150);
//                 bool first = true;
//                 for (auto &a : arr)
//                 {
//                     if (first)
//                     {
//                         artists += a["name"].as<String>();
//                         first = false;
//                     }
//                     else
//                     {
//                         artists += ", ";
//                         artists += a["name"].as<String>();
//                     }
//                 }
//                 this->ui->SetArtistName(artists);
//             }

//             this->ui->SetProgress(progress_ms, duration_ms);
//         }
//         else
//         {
//             ESP_LOGD(TAG, "Unable to parse response payload:\n %s", response.payload.c_str());
//             eventsSendError(500, "Unable to parse response payload", response.payload.c_str());
//         }
//     }
//     else if (response.httpCode == 204)
//     {
//         // No content
//     }
//     else
//     {
//         ESP_LOGD(TAG, "%d - %s", response.httpCode, response.payload.c_str());
//         eventsSendError(response.httpCode, "tunein error", response.payload.c_str());
//     }

//     ESP_LOGD(TAG, "HEAP: %d", ESP.getFreeHeap());
// }

// /**
//  * Get Spotify token
//  *
//  * @param code          Either an authorization code or a refresh token
//  * @param grant_type    [gt_authorization_code|gt_refresh_token]
//  */
// void TuneinApi::GetToken(const String &code, GrantTypes grant_type)
// {
//     ESP_LOGD(TAG, "GetToken(%s, %s)", code.c_str(), grant_type == gt_authorization_code ? "authorization" : "refresh");

//     bool success = false;

//     char requestContent[512];
//     if (grant_type == gt_authorization_code)
//     {
//         snprintf(requestContent, sizeof(requestContent),
//                  "grant_type=authorization_code"
//                  "&redirect_uri=http%%3A%%2F%%2F" CONFIG_DEVICE_NAME ".lan%%2Fcallback%%2F"
//                  "&code=%s",
//                  code.c_str());
//     }
//     else
//     {
//         snprintf(requestContent, sizeof(requestContent),
//                  "grant_type=refresh_token&refresh_token=%s",
//                  code.c_str());
//     }

//     uint8_t basicAuthSize = sizeof(SPTF_CLIENT_ID) + sizeof(SPTF_CLIENT_SECRET);
//     char basicAuth[basicAuthSize];
//     snprintf(basicAuth, basicAuthSize, "%s:%s", SPTF_CLIENT_ID, SPTF_CLIENT_SECRET);

//     char requestHeaders[768];
//     snprintf(requestHeaders, sizeof(requestHeaders),
//              "POST /api/token HTTP/1.1\r\n"
//              "Host: accounts.spotify.com\r\n"
//              "Authorization: Basic %s\r\n"
//              "Content-Length: %d\r\n"
//              "Content-Type: application/x-www-form-urlencoded\r\n"
//              "Connection: close\r\n\r\n",
//              b64Encode(basicAuth).c_str(), strlen(requestContent));

//     HTTP_response_t response = httpRequest("accounts.spotify.com", 443, requestHeaders, requestContent);

//     if (response.httpCode == 200)
//     {
//         DynamicJsonBuffer jsonInBuffer(572);
//         //StaticJsonBuffer<572> jsonInBuffer;
//         JsonObject &json = jsonInBuffer.parseObject(response.payload);

//         if (json.success())
//         {
//             access_token = json["access_token"].as<String>();
//             if (access_token != "")
//             {
//                 token_lifetime_ms = (json["expires_in"].as<uint32_t>() - 300) * 1000;
//                 token_millis = millis();
//                 success = true;
//                 if (json.containsKey("refresh_token"))
//                 {
//                     refresh_token = json["refresh_token"].as<String>();
//                     writeRefreshToken();
//                 }
//             }
//         }
//         else
//         {
//             ESP_LOGD(TAG, "Unable to parse response payload:\n %s", response.payload.c_str());
//             eventsSendError(500, "Unable to parse response payload", response.payload.c_str());
//         }
//     }
//     else
//     {
//         ESP_LOGD(TAG, "%d - %s", response.httpCode, response.payload.c_str());
//         eventsSendError(response.httpCode, "tunein error", response.payload.c_str());

//     }

//     if (success)
//     {
//         this->State = SptfActions::CurrentlyPlaying;
//     } else {
//         this->State = SptfActions::TokenRequestFailed;
//     }

//     getting_token = false;
// }

// /**
//  * Call Spotify API
//  *
//  * @param method
//  * @param endpoint
//  * @return
//  */
// HTTP_response_t TuneinApi::ApiRequest(const char *method, const char *endpoint, const char *content)
// {
//     ESP_LOGD(TAG, "ApiRequest(%s, %s, %s)", method, endpoint, content);

//     char headers[512];
//     snprintf(headers, sizeof(headers),
//              "%s /v1/me/player%s HTTP/1.1\r\n"
//              "Host: api.spotify.com\r\n"
//              "Authorization: Bearer %s\r\n"
//              "Content-Length: %d\r\n"
//              "Connection: close\r\n\r\n",
//              method, endpoint, access_token.c_str(), strlen(content));

//     return httpRequest("api.spotify.com", 443, headers, content);
// }

// /**
//  * Display album art
//  *
//  * @param url
//  */
// void TuneinApi::DisplayAlbumArt(String url)
// {
//     ESP_LOGD(TAG, "DisplayAlbumArt(%s)", url.c_str());

//     HTTPClient http;

//     http.begin(url);
//     int httpCode = http.GET();

//     if (httpCode > 0)
//     {
//         if (httpCode == HTTP_CODE_OK)
//         {

//             const char filename[] = "/albart.jpg";

//             SPIFFS.remove(filename);

//             File file = SPIFFS.open(filename, FILE_APPEND);
//             if (!file)
//             {
//                 ESP_LOGD(TAG, "File open failed");
//                 eventsSendError(500, "File open failed");
//                 http.end();
//                 return;
//             }

//             WiFiClient *stream = http.getStreamPtr();
//             int jpgSize = http.getSize();
//             if (jpgSize < 0)
//             {
//                 ESP_LOGD(TAG, "Unable to get JPEG size");
//                 eventsSendError(500, "Unable to get JPEG size");
//                 http.end();
//                 return;
//             }
//             uint16_t buffSize = 1024;
//             uint8_t buff[buffSize] = {0};

//             while (http.connected() && jpgSize > 0)
//             {
//                 int availableSize = stream->available();
//                 if (availableSize > 0)
//                 {

//                     // Read data from stream into buffer
//                     int B = stream->readBytes(buff, (size_t)min(availableSize, buffSize));

//                     // Write buffer to file
//                     file.write(buff, (size_t)B);

//                     jpgSize -= B;
//                 }
//                 delay(10);
//             }
//             file.close();

//             this->ui->SetAlbumArt(filename);
//         }
//         else
//         {
//             ESP_LOGD(TAG, "Unable to get album art: %s", http.errorToString(httpCode).c_str());
//             eventsSendError(httpCode, "Unable to get album art", http.errorToString(httpCode).c_str());
//         }
//     }
//     else
//     {
//         ESP_LOGD(TAG, "Unable to get album art: %s", http.errorToString(httpCode).c_str());
//         eventsSendError(httpCode, "Unable to get album art", http.errorToString(httpCode).c_str());
//     }

//     http.end();
// }

// /**
//  * Delete refresh token from EEPROM
//  */
// void TuneinApi::deleteRefreshToken()
// {
//     ESP_LOGD(TAG, "\n> [%d] deleteRefreshToken()\n", micros());

//     EEPROM.begin(256);
//     EEPROM.write(0, '\0'); // Deleting only the first character is enough to make the key invalid
//     EEPROM.end();
// }

// /**
//  * Read refresh token from EEPROM
//  *
//  * @return String
//  */
// String TuneinApi::readRefreshToken()
// {
//     String tok;
//     tok.reserve(150);

//     EEPROM.begin(1024);

//     for (int i = 0; i < 5; i++)
//     {
//         tok += String(char(EEPROM.read(i)));
//     }

//     if (tok == "rtok:")
//     {
//         tok = "";
//         char c;
//         uint16_t addr = 5;
//         while (addr < 256)
//         {
//             c = char(EEPROM.read(addr++));
//             if (c == '\0')
//             {
//                 break;
//             }
//             else
//             {
//                 tok += c;
//             }
//             yield();
//         }
//     }
//     else
//     {
//         tok = "";
//     }

//     EEPROM.end();

//     return tok;
// }

// /**
//  * Write refresh token to EEPROM
//  */
// void TuneinApi::writeRefreshToken()
// {
//     ESP_LOGD(TAG, "\n> [%d] writeRefreshToken()\n", micros());

//     EEPROM.begin(256);

//     String tmpTok;
//     tmpTok.reserve(refresh_token.length() + 6);
//     tmpTok = "rtok:";
//     tmpTok += refresh_token;

//     uint16_t i = 0, addr = 0, tt_len = tmpTok.length();
//     for (i = 0; addr < 256 && i < tt_len; i++, addr++)
//     {
//         EEPROM.write(addr, (uint8_t)tmpTok.charAt(i));
//     }

//     EEPROM.write(addr, '\0');
//     EEPROM.end();
// }

// /**
//  * HTTP request
//  *
//  * @param host
//  * @param port
//  * @param headers
//  * @param content
//  * @return
//  */
// HTTP_response_t TuneinApi::httpRequest(const char *host, uint16_t port, const char *headers, const char *content)
// {
//     ESP_LOGD(TAG, "httpRequest(%s, %d, ...)", host, port);

//     WiFiClientSecure client;

//     client.setInsecure(); // shouldn't do this
//     if (!client.connect(host, port))
//     {
//         return {503, "Service unavailable (unable to connect)"};
//     }

//     /*
//      * Send HTTP request
//      */

//     ESP_LOGD(TAG, "Request:\n%s%s", headers, content);
//     eventsSendLog(">>>> REQUEST");
//     eventsSendLog(headers);
//     eventsSendLog(content);

//     client.print(headers);
//     if (strlen(content))
//     {
//         client.print(content);
//     }

//     /*
//      * Get HTTP response
//      */

//     uint32_t timeout = millis();
//     while (!client.available())
//     {
//         if (millis() - timeout > 5000)
//         {
//             client.stop();
//             return {503, "Service unavailable (timeout)"};
//         }
//         yield();
//     }

//     ESP_LOGD(TAG, "Response: ");
//     eventsSendLog("<<<< RESPONSE");

//     HTTP_response_t response = {0, ""};
//     boolean EOH = false;
//     uint32_t contentLength = 0;
//     uint16_t buffSize = 1024;
//     uint32_t readSize = 0;
//     uint32_t totatlReadSize = 0;
//     uint32_t lastAvailableMillis = millis();
//     char buff[buffSize];

//     // !HERE
//     // client.setNoDelay(false);

//     while (client.connected())
//     {
//         int availableSize = client.available();
//         if (availableSize)
//         {
//             lastAvailableMillis = millis();

//             if (!EOH)
//             {
//                 // Read response headers
//                 readSize = client.readBytesUntil('\n', buff, buffSize);
//                 buff[readSize - 1] = '\0'; // replace /r by \0
//                 ESP_LOGD(TAG, "%s\n", buff);
//                 eventsSendLog(buff);
//                 if (startsWith(buff, "HTTP/1."))
//                 {
//                     buff[12] = '\0';
//                     response.httpCode = atoi(&buff[9]);
//                     if (response.httpCode == 204)
//                     {
//                         break;
//                     }
//                 }
//                 else if (startsWith(buff, "Content-Length:"))
//                 {
//                     contentLength = atoi(&buff[16]);
//                     if (contentLength == 0)
//                     {
//                         break;
//                     }
//                     response.payload.reserve(contentLength + 1);
//                 }
//                 else if (buff[0] == '\0')
//                 {
//                     // End of headers
//                     EOH = true;
//                     ESP_LOGD(TAG, "<EOH>\n");
//                     eventsSendLog("");
//                 }
//             }
//             else
//             {
//                 // Read response content
//                 readSize = client.readBytes(buff, min(buffSize - 1, availableSize));
//                 buff[readSize] = '\0';
//                 // ESP_LOGD(TAG, buff);
//                 eventsSendLog(buff, log_raw);
//                 response.payload += buff;
//                 totatlReadSize += readSize;
//                 if (totatlReadSize >= contentLength)
//                 {
//                     break;
//                 }
//             }
//         }
//         else
//         {
//             if ((millis() - lastAvailableMillis) > 5000)
//             {
//                 response = {504, "Response timeout"};
//                 break;
//             }
//             delay(100);
//         }
//     }
//     client.stop();

//     ESP_LOGD(TAG, "HEAP: %d", ESP.getFreeHeap());

//     return response;
// }