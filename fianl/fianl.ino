#include "src/OV2640.h"
#include <WiFi.h>
#include <WiFiClient.h>

#include "src/SimStreamer.h"
#include "src/OV2640Streamer.h"
#include "src/CRtspSession.h"

#define SOFTAP_MODE // If you want to run our own softap turn this on
#define ENABLE_RTSPSERVER

// Select camera model
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

OV2640 cam;

#ifdef ENABLE_RTSPSERVER
WiFiServer rtspServer(8554);
#endif


#ifdef SOFTAP_MODE
IPAddress apIP = IPAddress(192, 168, 4, 1);
#else
#include "wifikeys.h"
#endif


void lcdMessage(String msg)
{
  #ifdef ENABLE_OLED
    if(hasDisplay) {
        display.clear();
        display.drawString(128 / 2, 32 / 2, msg);
        display.display();
    }
  #endif
}

void setup()
{

    Serial.begin(115200);
    //while (!Serial);            //wait for serial connection. 

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12; 
    config.fb_count = 2;       
  
    #if defined(CAMERA_MODEL_ESP_EYE)
      pinMode(13, INPUT_PULLUP);
      pinMode(14, INPUT_PULLUP);
    #endif
  
    cam.init(config);
    
    IPAddress ip;

#ifdef SOFTAP_MODE
    const char *hostname = "BIDULGI";
   // WiFi.hostname(hostname); // FIXME - find out why undefined
    WiFi.mode(WIFI_AP);
    
    bool result = WiFi.softAP(hostname, "12345678", 1, 0);
    delay(2000);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    if (!result)
    {
        Serial.println("AP Config failed.");
        return;
    }
    else
    {
        Serial.println("AP Config Success.");
        Serial.print("AP MAC: ");
        Serial.println(WiFi.softAPmacAddress());

        ip = WiFi.softAPIP();

        Serial.print("Stream Link: rtsp://");
        Serial.print(ip);
        Serial.println(":8554/mjpeg/1");
    }
#else
    lcdMessage(String("join ") + ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }
    ip = WiFi.localIP();
    Serial.println(F("WiFi connected"));
    Serial.println("");
    Serial.println(ip);
    Serial.print("Stream Link: rtsp://");
    Serial.print(ip);
    Serial.println(":8554/mjpeg/1");
#endif

    lcdMessage(ip.toString());
#ifdef ENABLE_RTSPSERVER
    rtspServer.begin();
#endif
}

CStreamer *streamer;
CRtspSession *session;
WiFiClient client; // FIXME, support multiple clients

void loop()
{
#ifdef ENABLE_RTSPSERVER
    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    // (FIXME - support multiple simultaneous clients)
    if(session) {
        session->handleRequests(0); // we don't use a timeout here,
        // instead we send only if we have new enough frames

        uint32_t now = millis();
        if(now > lastimage + msecPerFrame || now < lastimage) { // handle clock rollover
            session->broadcastCurrentFrame(now);
            lastimage = now;

            // check if we are overrunning our max frame rate
            now = millis();
            if(now > lastimage + msecPerFrame)
                printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
        }

        if(session->m_stopped) {
            delete session;
            delete streamer;
            session = NULL;
            streamer = NULL;
        }
    }
    else {
        client = rtspServer.accept();

        if(client) {
            //streamer = new SimStreamer(&client, true);             // our streamer for UDP/TCP based RTP transport
            streamer = new OV2640Streamer(&client, cam);             // our streamer for UDP/TCP based RTP transport

            session = new CRtspSession(&client, streamer); // our threads RTSP session and state
        }
    }
#endif
}
