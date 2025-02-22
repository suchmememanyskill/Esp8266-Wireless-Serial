#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>
#include "web.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool isSerialActive = false;
unsigned int baudrate = 0;

void handle_connect(AsyncWebSocketClient * client, uint8_t *payload, size_t length)
{
    
}

void handle_disconnect(AsyncWebSocketClient * client, uint8_t *payload, size_t length)
{
    
}

#define EQ(str1, str2) str1 != NULL && str2 != NULL && !strcmp(str1, str2)

void send_info(AsyncWebSocketClient * client)
{
    JsonDocument out;
    char buff[128];
    out["action"] = "info";
    out["baud"] = baudrate;
    out["active"] = isSerialActive;
    serializeJson(out, buff);
    client->text(buff);
}

void handle_text(AsyncWebSocketClient * client, uint8_t *payload, size_t length)
{
    JsonDocument doc;
    
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok)
    {
        return;
    }

    const char *func = doc["action"];

    if (EQ(func, "connect") && !isSerialActive)
    {
        baudrate = doc["baud"];

        if (baudrate <= 0)
        {
            baudrate = Serial.detectBaudrate(1000);
        }

        if (baudrate <= 0)
        {
            return;
        }

        isSerialActive = true;
        Serial.begin(baudrate);
        send_info(client);
    }
    else if (EQ(func, "disconnect") && isSerialActive)
    {
        isSerialActive = false;
        Serial.end();
        send_info(client);
    }
    else if (EQ(func, "info"))
    {
        send_info(client);
    }
    else if (EQ(func, "send") && isSerialActive)
    {
        String data = doc["data"];
        Serial.println(data);
    }
}

typedef struct {
    void (*ptr)(AsyncWebSocketClient * client, uint8_t *payload, size_t length);
    AwsEventType type;
} WebSocketHandlers_t;

WebSocketHandlers_t handlers[] = {
    { handle_connect, WS_EVT_CONNECT },
    { handle_disconnect, WS_EVT_DISCONNECT },
    { handle_text, WS_EVT_DATA },
};

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
    const int array_size = sizeof(handlers)/sizeof((handlers)[0]);

    for (int i = 0; i < array_size; i++)
    {
        if (handlers[i].type == type)
        {
            handlers[i].ptr(client, data, len);
        }
    }
}

void setup()
{
    WiFi.softAP("WirelessSerial");
    delay(500);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        //Serial.println("Request received (/)");
        request->send(200, "text/html", HTML_CONTENT);
    });

    server.on("/index", HTTP_GET, [](AsyncWebServerRequest *request){
        //Serial.println("Request received (/index)");
        request->send(200, "text/html", HTML_CONTENT);
    });
    
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();
}

char buff[512];
char data[257];
unsigned int data_length = 0;
unsigned long last_fetched = 0;
const int max_wait_between_messages_ms = 400;

void loop()
{
    if (!isSerialActive)
    {
        return;
    }

    unsigned int old_data_length = data_length;

    while (Serial.available() && data_length < 256)
    {
        data[data_length++] =  Serial.read();
    }

    if (data_length > 0 && old_data_length <= 0)
    {
        last_fetched = millis();
    }

    if (data_length > 0 && ((last_fetched + max_wait_between_messages_ms) < millis() || data_length >= 256))
    {
        data[data_length] = '\0';
        JsonDocument out;
        out["action"] = "data";
        out["data"] = data;
        serializeJson(out, buff);
        ws.textAll(buff);
        data_length = 0;
    }
}