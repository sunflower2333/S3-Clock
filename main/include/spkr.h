#pragma once

#include <stdio.h>
#include <cstring>

// Speaker
#include <driver/i2s_std.h>
#include <driver/i2s_pdm.h>
// SD
#include <esp_spiffs.h>
// Log
#include <esp_err.h>
#include <esp_log.h>
// Gpio
#include <driver/gpio.h>
// RTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"

typedef struct 
{
    //   RIFF Section    
    char RIFFSectionID[4];      // Letters RIFF
    uint32_t Size;              // Size of entire file less 8
    char RiffFormat[4];         // Letters WAVE
    
    //   Format Section    
    char FormatSectionID[4];    // letters fmt
    uint32_t FormatSize;        // Size of format section less 8
    uint16_t FormatID;          // 1=uncompressed PCM
    uint16_t NumChannels;       // 1=mono,2=stereo
    uint32_t SampleRate;        // 44100, 16000, 8000 etc.
    uint32_t ByteRate;          // =SampleRate * Channels * (BitsPerSample/8)
    uint16_t BlockAlign;        // =Channels * (BitsPerSample/8)
    uint16_t BitsPerSample;     // 8,16,24 or 32
    // // List Section
    // char ListSectionID[4]; // LIST
    // uint32_t unkown_list;
    // char InfoSectionID[8]; // INFOISFT
    // uint8_t InfoSection[18];
    // Data Section
    char DataSectionID[4];      // The letters data
    uint32_t DataSize;          // Size of the data that follows
}__attribute__ ((packed)) WavHeader_Struct;

void start_spkr(void *pvParameters);