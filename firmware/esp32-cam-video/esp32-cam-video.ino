#include <Arduino.h>
#include <SPI.h>
#include "esp_camera.h"


#define SPI_SCK 14
#define SPI_MOSI 13
#define SPI_CS 15

#define BLOCK_SIZE 1024


SPIClass spi(VSPI);


// OV3660

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1

#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y2_GPIO_NUM       35
#define Y3_GPIO_NUM       34
#define Y4_GPIO_NUM       39
#define Y5_GPIO_NUM       36
#define Y6_GPIO_NUM       21
#define Y7_GPIO_NUM       19
#define Y8_GPIO_NUM       18
#define Y9_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22



void initCamera()
{

    camera_config_t config={};

    config.ledc_channel=LEDC_CHANNEL_0;
    config.ledc_timer=LEDC_TIMER_0;

    config.pin_d0=Y2_GPIO_NUM;
    config.pin_d1=Y3_GPIO_NUM;
    config.pin_d2=Y4_GPIO_NUM;
    config.pin_d3=Y5_GPIO_NUM;
    config.pin_d4=Y6_GPIO_NUM;
    config.pin_d5=Y7_GPIO_NUM;
    config.pin_d6=Y8_GPIO_NUM;
    config.pin_d7=Y9_GPIO_NUM;

    config.pin_xclk=XCLK_GPIO_NUM;
    config.pin_pclk=PCLK_GPIO_NUM;
    config.pin_vsync=VSYNC_GPIO_NUM;
    config.pin_href=HREF_GPIO_NUM;

    config.pin_sscb_sda=SIOD_GPIO_NUM;
    config.pin_sscb_scl=SIOC_GPIO_NUM;

    config.pin_pwdn=PWDN_GPIO_NUM;
    config.pin_reset=RESET_GPIO_NUM;

    config.xclk_freq_hz=20000000;

    // JPEG avoids the OV3660 raw RGB565 colour-conversion problem and is
    // substantially smaller to send over SPI.
    config.pixel_format=PIXFORMAT_JPEG;

    config.frame_size=FRAMESIZE_QQVGA;
    config.jpeg_quality=12;
    config.fb_count=2;
    config.grab_mode=CAMERA_GRAB_LATEST;



    if(esp_camera_init(&config)!=ESP_OK)
    {
        Serial.println("CAM FAIL");
        while(1);
    }


    Serial.println("CAM READY");

}



void spiSend(
    uint8_t *data,
    int len
)
{

    digitalWrite(
        SPI_CS,
        LOW
    );


    delayMicroseconds(5);


    spi.transferBytes(
        data,
        NULL,
        len
    );


    delayMicroseconds(5);


    digitalWrite(
        SPI_CS,
        HIGH
    );


}



void sendFrame(
    camera_fb_t *fb
)
{

    uint8_t header[6];


    header[0]=0xAA;
    header[1]=0x55;


    uint32_t size=fb->len;


    memcpy(
        header+2,
        &size,
        4
    );


    // header单独一次

    spiSend(
        header,
        6
    );


    int offset=0;

    int block=0;


    while(offset < (int)fb->len)
    {

        int n=
        min(
            BLOCK_SIZE,
            (int)(fb->len-offset)
        );


        spiSend(
            fb->buf+offset,
            n
        );


        block++;


        offset+=n;


        delay(1);

    }

    Serial.printf("JPEG SENT: %u bytes, %d blocks\n", fb->len, block);

}



void setup()
{

    Serial.begin(115200);

    delay(2000);


    initCamera();



    spi.begin(
        SPI_SCK,
        -1,
        SPI_MOSI
    );


    spi.beginTransaction(
        SPISettings(
            1000000,
            MSBFIRST,
            SPI_MODE1
        )
    );



    pinMode(
        SPI_CS,
        OUTPUT
    );


    digitalWrite(
        SPI_CS,
        HIGH
    );


    Serial.println(
        "SPI READY"
    );

}



void loop()
{

    camera_fb_t *fb=
        esp_camera_fb_get();



    if(!fb)
    {

        Serial.println(
            "CAPTURE FAIL"
        );

        return;

    }

    if(fb->format != PIXFORMAT_JPEG)
    {
        Serial.println("CAM FORMAT ERROR");
        esp_camera_fb_return(fb);
        delay(500);
        return;
    }

    sendFrame(fb);
    esp_camera_fb_return(fb);
    delay(150);

}
