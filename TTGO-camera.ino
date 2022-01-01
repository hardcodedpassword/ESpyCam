#include <WiFi.h>
#include <Wire.h>
#include "esp_camera.h"
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "pinout.h"
#include "secrets.h"

String ip; // device its IP address

//============ Define OLED =============================
#include "SSD1306Wire.h"
SSD1306Wire display(0x3c, I2C_SDA, I2C_SCL, GEOMETRY_128_64, I2C_ONE, -1);
//======================================================

//============= Telegram ===============================
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
//======================================================

void startCameraServer();


camera_fb_t *fb = NULL;
bool dataAvailable = false;

bool isMoreDataAvailable()
{
  if (dataAvailable)
  {
    dataAvailable = false;
    return true;
  }
  else
  {
    return false;
  }
}

byte *getNextBuffer()
{
  if (fb)
  {
    return fb->buf;
  }
  else
  {
    return nullptr;
  }
}

int getNextBufferLen()
{
  if (fb)
  {
    return fb->len;
  }
  else
  {
    return 0;
  }
}

void photo()
{
    Serial.println("Photo!");

    fb = NULL;
    // Take Picture with Camera
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      bot.sendMessage(CHAT_ID, "Camera capture failed", "");
      return;
    }
    dataAvailable = true;
    Serial.println("Sending");
    bot.sendPhotoByBinary(CHAT_ID, "image/jpeg", fb->len,
                          isMoreDataAvailable, nullptr,
                          getNextBuffer, getNextBufferLen);

    Serial.println("done!");

    esp_camera_fb_return(fb);  
}

// Setup the required IO
bool setupIO()
{
    Wire.begin(I2C_SDA, I2C_SCL);
    pinMode(AS312_PIN, INPUT); // motion sensor
    pinMode(BUTTON_1, INPUT);  // push button
    return true;
}


void detectMotion()
{
  static bool motionState = false;
  bool motion = digitalRead( AS312_PIN );
  if ( motion != motionState )
  {
    if (motion == true)
    {
      display.displayOn();
      Serial.println("motion");
      bot.sendMessage(CHAT_ID, "Motion", "");
      photo();
      display.displayOff();
    }
    motionState = motion;
  }  
}


bool setupCamera()
{
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
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
  s->set_brightness(s, 1);//up the blightness just a bit
  s->set_saturation(s, -2);//lower the saturation

  return true;
}

bool setupDisplay()
{
  display.init();
  display.displayOn();
  Serial.println("oled init done" );
  display.clear();
  display.invertDisplay();
  display.display();
  //display.setBrightness(255); // causes a crash?
  display.displayOff();
  
  return true;
}

bool setupNetwork()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  int n = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
 
    if ( n > 20 )
    {
      return false;
    }
    n++;
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.println("");
  Serial.println("WiFi connected");
  ip = WiFi.localIP().toString();
  Serial.println(ip);
 
  return true;
}

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();


    Serial.println("Setting up camera...");
    if ( !setupCamera() )
    {
      Serial.println("Failed to setup Camera");
      ESP.restart();
    }

    Serial.println("Setting up IO...");
    if ( !setupIO() )
    {
      Serial.println("Failed to setup IO");
      ESP.restart();
    }

    if (psramFound()) 
    {
      Serial.println("PSRAM Found");
    }

    Serial.println("Setting up display...");
    if ( !setupDisplay() )
    {
      Serial.println("Failed to setup Display");
      ESP.restart();
    }
    
    Serial.println("Setting up network...");
    if ( !setupNetwork() )
    {
      Serial.println("Failed to setup Network");
      ESP.restart();
    }

    secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);


    Serial.println("Starting server...");
    delay(500);    
    startCameraServer();
    delay(50);

}

void loop()
{
  detectMotion();

  if ( WiFi.status() != WL_CONNECTED )
  {
    ESP.restart();
  }
}
