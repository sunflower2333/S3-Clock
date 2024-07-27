#include <spkr.h>

static const char* TAG = "AudSpkr";
WavHeader_Struct WavHeader;

bool ValidWavData(WavHeader_Struct* Wav);
void sound_terminate(void);

void start_spkr(void *pvParameters)
{
    // // 初始化I2S引脚
    // i2s_config_t i2s_config = {
    //     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),              // 使用主模式并设置为发送数据
    //     .sample_rate = 48000,                               // 设置采样率为44100Hz
    //     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,       // 设置每个采样点的位数为16位
    //     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,       // 立体声
    //     .communication_format = I2S_COMM_FORMAT_STAND_I2S,  // I2S通信格式
    //     .intr_alloc_flags = ESP_INTR_FLAG_EDGE,             // 分配中断标志
    //     .dma_buf_count = 16,                                 // 设置DMA缓冲区数量为8
    //     .dma_buf_len = 1024,                                // 每个DMA缓冲区的长度为1024字节
    // };
    
    // i2s_pin_config_t pin_config = {
    //     .bck_io_num = GPIO_NUM_21,            // BCLK引脚号
    //     .ws_io_num = GPIO_NUM_47,             // LRCK引脚号
    //     .data_out_num = GPIO_NUM_14, // DATA引脚号
    //     .data_in_num = -1,           // DATA_IN引脚号
    // };
    // i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    // i2s_set_pin(I2S_NUM_0, &pin_config);

    // New I2S API
    static i2s_chan_handle_t                tx_chan;        // I2S tx channel handler
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
            .bclk = I2S_SCK,
            .ws   = I2S_WS,
            .dout = I2S_SDO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));


    // 使用 SPIFFS 进行文件系统的初始化
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/audio",
        .partition_label = "audio",
        .max_files = 5,                // 包含的最大文件数
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    // Enable Amplifier
    gpio_reset_pin(AMP_EN);
    gpio_set_direction(AMP_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(AMP_EN, 1);
    
    // 打开文件进行读取
    FILE* f = fopen("/audio/fdp2.wav", "rb");

    if (f == NULL) 
    {
        ESP_LOGI(TAG, "Failed to open file for reading");
    }
    else 
    {
        char wavBuffer[1024];   // 接收缓冲区
        // 读取wav文件信息
        fseek(f, 0, SEEK_SET);  // 重新将指针指向文件首部
        fread(wavBuffer, sizeof(char), sizeof(WavHeader_Struct), f);
        memcpy(&WavHeader, wavBuffer, sizeof(WavHeader_Struct));
        if(ValidWavData(&WavHeader))
        {
            printf("WAV Infomation:\n");
            printf("Bit per sample %d \n Block Align %d \n DataSize %ld \n SampleRate %ld \n",
                    WavHeader.BitsPerSample,
                    WavHeader.BlockAlign,
                    WavHeader.DataSize,
                    WavHeader.SampleRate);
            // i2s_channel_reconfig_std_clock(tx_chan, I2S_STD_CLK_DEFAULT_CONFIG(WavHeader.SampleRate));
        }
        else
        {
            ESP_LOGI(TAG, "Failed to read wav file");
        }
        uint32_t wavData_size = WavHeader.DataSize; // 保存文件字符数
        uint16_t readTimes = 0;                     // 需要读的次数
        size_t BytesWritten;

        for (readTimes = 0; readTimes < (wavData_size / 1024); readTimes++)
        {
            fread(wavBuffer, sizeof(char), 1024, f); // 读文件
            i2s_channel_write(tx_chan, wavBuffer, 1024, &BytesWritten, 1000);
        }

        memset(wavBuffer, 0, 1024);                               // 清空&#xff0c;准备读少于1024的字节
        fread(wavBuffer, sizeof(char), (wavData_size % 1024), f); // 读文件

        i2s_channel_write(tx_chan, wavBuffer, (wavData_size % 1024) + 20, &BytesWritten, 1000);

        printf("readtimes=%d,last read len = %ld\r\n", readTimes, (wavData_size % 1024));

        printf("sound play over\r\n");
        gpio_set_level(GPIO_NUM_16, 0);
        sound_terminate(); //清空缓存
    }

    // 关闭文件
    fclose(f);

    // 卸载 SPIFFS 文件系统
    esp_vfs_spiffs_unregister(NULL);
    // 卸载 I2S 驱动
    i2s_channel_disable(tx_chan);
    i2s_del_channel(tx_chan);
    vTaskDelete(NULL);
}

bool ValidWavData(WavHeader_Struct* Wav)
{
    if(memcmp(Wav->RIFFSectionID,"RIFF",4)!=0) 
    {    
        printf("Invlaid data - Not RIFF format");
        return false;        
    }
    if(memcmp(Wav->RiffFormat,"WAVE",4)!=0)
    {
        printf("Invlaid data - Not Wave file");
        return false;           
    }
    if(memcmp(Wav->FormatSectionID,"fmt",3)!=0) 
    {
        printf("Invlaid data - No format section found");
        return false;       
    }
    if(memcmp(Wav->DataSectionID,"data",4)!=0) 
    {
        printf("Invlaid data - data section not found, %x %x %x %x", Wav->DataSectionID[0], Wav->DataSectionID[1], Wav->DataSectionID[2], Wav->DataSectionID[3]);
        return false;      
    }
    if(Wav->FormatID!=1) 
    {
        printf("Invlaid data - format Id must be 1");
        return false;                          
    }
    if(Wav->FormatSize!=16) 
    {
        printf("Invlaid data - format section size must be 16.");
        return false;                          
    }
    if((Wav->NumChannels!=1)&(Wav->NumChannels!=2))
    {
        printf("Invlaid data - only mono or stereo permitted.");
        return false;   
    }
    if(Wav->SampleRate>48000) 
    {
        printf("Invlaid data - Sample rate cannot be greater than 48000");
        return false;                       
    }
    if((Wav->BitsPerSample!=8) | (Wav->BitsPerSample!=16) | (Wav->BitsPerSample!=32)) 
    {
        printf("Invlaid data - Only 8 or 16 bits per sample permitted.");
        return false;                        
    }
    return true;
}

void sound_terminate(void)
{
    vTaskDelay(pdMS_TO_TICKS(200)); //延时200ms
    // i2s_zero_dma_buffer(I2S_NUM_0);   // Clean the DMA buffer
    // i2s_stop(I2S_NUM_0);

    // i2s_start(I2S_NUM_0);//增加测试
}