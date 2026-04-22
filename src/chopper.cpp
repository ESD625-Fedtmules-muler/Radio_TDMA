#include <Arduino.h>
#include <Hamming.h>
#include <main.h>
#include "esp_rom_crc.h"





#define num_package_queue 10
#define MAGIC_NUMBER 0x88221030
const int32_t w_parity  =7; //størrelse efter parity
const int32_t wo_parity =  4; //Størrselse før parity
const size_t bytes_pr_frame = 18; //Hvor mange bytes vi forventer at klemme ind i et frame


HammingCodec codec(w_parity, wo_parity); 

//De her 2 de findes ude i verden et sted
extern QueueHandle_t tx_blockqueue;
extern QueueHandle_t rx_blockqueue[MAX_NODES];

QueueHandle_t rx_package_queue; //QUeues for de packages after the chopper
QueueHandle_t tx_package_queue;

void print_pointer(const char* msg,void* object){
    Serial.print(msg);
    Serial.print("0x");
    Serial.println((size_t)object, HEX);
}



struct  __attribute__((packed)) chopper_header
{
    uint64_t ID = MAGIC_NUMBER; 
    uint8_t number_of_frames = 0;
    uint8_t tail_length = 0; //How long the last block is
    uint32_t payload_crc = 0;
    

    void enqueue(QueueHandle_t* queue_to_send_to){
        block_item header_item;
        uint8_t buf[32] = {0};
        
        memcpy(buf, this, sizeof(chopper_header));

        codec.encode(buf, bytes_pr_frame, header_item.block_payload, 32);        
        xQueueSend(*queue_to_send_to, &header_item, portMAX_DELAY);
    }



    bool decode_from_block(block_item* block)
    {
        uint8_t buf[32] = {0};

        codec.decode(
            block->block_payload,
            codec.encodedBits(bytes_pr_frame),
            buf,
            bytes_pr_frame
        );

        // Kopiér direkte ind i struct
        memcpy(this, buf, sizeof(chopper_header));

        // Validér
        if (ID != MAGIC_NUMBER)
            return false;

        return true;
    }
};




struct param_dechopper{
    QueueHandle_t ListenerQueue;
    uint16_t Timeslot;
};

void task_dechopper(void *pvParameters)
{
    param_dechopper params;
    memcpy(&params, pvParameters, sizeof(param_dechopper));
    QueueHandle_t listener_queue = params.ListenerQueue; 


    while (true)
    {        
        block_item incoming_block;

        // Vent på første blok - den skal være en header
        if (!xQueueReceive(listener_queue, &incoming_block, portMAX_DELAY))
            continue;
        // Forsøg at decode headeren
        chopper_header header;
        if (!header.decode_from_block(&incoming_block))
        {
            // Ikke en gyldig header - skip og prøv igen
            //Serial.println("Header error\n");
            //Serial.print("content:");
            //incoming_block.print_payload();
            //Serial.printf("&rx_blockqueue[%d] = ", params.Timeslot);
            //print_pointer(" \t ", listener_queue);
            continue;
        }


        // Nu ved vi hvor mange frames der følger
        Package_queue_item output_package;
        output_package.length = 0;
        bool assembly_ok = true;

        for (uint8_t frame = 0; frame < header.number_of_frames; frame++)
        {
            block_item data_block;
            if (!xQueueReceive(listener_queue, &data_block, portMAX_DELAY))
            {
                assembly_ok = false;
                break;
            }
            //data_block.print_payload();
            uint8_t decoded_buf[bytes_pr_frame] = {0};

            uint16_t frame_len = (frame == (header.number_of_frames-1)) ? header.tail_length : bytes_pr_frame; //SÅdan fixer hale problemet så den sidste byte ikke bliver kopirereet helt over  
            int decoded_len = codec.decode(data_block.block_payload, codec.encodedBits(bytes_pr_frame), decoded_buf, frame_len);

            // decoded_len < 0 kan bruges til at detektere ukorrigerbare fejl
            if (decoded_len < 0)
            {
                Serial.print("Hard bitflip error");
                assembly_ok = false;
                break;
            }
            memcpy(&output_package.data[output_package.length], decoded_buf, frame_len);
            output_package.length += frame_len;
        }
        

        if (assembly_ok)
        {
            //Serial.println("Assembly okay so far checking CRC");
            //output_package.debug_msg();
            uint32_t crc = esp_rom_crc32_le(0, output_package.data, sizeof(output_package.length));
            if(header.payload_crc != crc){
                Serial.printf("CRC fail");
                continue;
            }
            xQueueSend(rx_package_queue, &output_package, portMAX_DELAY);
        }
    }
}







void task_chopper(void *pvParameters){
    
    
    uint32_t loop_counter = 0;


    while (true)
    {
        Package_queue_item input_package;
        if(xQueueReceive(tx_package_queue, &input_package, portMAX_DELAY)){
            uint8_t number_of_frames = input_package.length / bytes_pr_frame;
            uint16_t tail_length     = input_package.length % bytes_pr_frame;
            if (tail_length > 0) number_of_frames++;

            //input_package.debug_msg();

            chopper_header header_content;
            header_content.number_of_frames = number_of_frames;
            header_content.tail_length      = tail_length;
            header_content.payload_crc = esp_rom_crc32_le(0, input_package.data, sizeof(input_package.length));
            //Serial.println("Constructing header");
            //Serial.println(header_content.payload_crc);

            header_content.enqueue(&tx_blockqueue);

            // Loop over fulde frames
            for (uint8_t f = 0; f < number_of_frames; f++)
            {
                bool is_last   = (f == number_of_frames - 1);
                uint16_t flen  = (is_last && tail_length > 0) ? tail_length : bytes_pr_frame;

                block_item outputbuffer;
                int len = codec.encode(&input_package.data[f * bytes_pr_frame], flen,
                            outputbuffer.block_payload, 32);
                
                xQueueSend(tx_blockqueue, &outputbuffer, portMAX_DELAY);
            }


        }

    }
}




void setup_chopper(){
    codec.begin();
    tx_package_queue = xQueueCreate(10, sizeof(Package_queue_item));
    
    rx_package_queue = xQueueCreate(5, sizeof(Package_queue_item));
    

    xTaskCreatePinnedToCore(
    task_chopper,        // Task function
    "Chopper",     // Name
    4096,             // Stack size
    NULL,             // Parameters
    6,                // Priority
    NULL,             // Task handle
    1                 // Core ID (0 or 1)
    );

    //*Så skal vi lige have spawnet 10 dechoppers
    for (size_t i = 0; i < MAX_NODES; i++)
    {
        Serial.printf("&rx_blockqueue[%d] = ", i);
        print_pointer(" \t ", rx_blockqueue[i]);

        param_dechopper params;
        params.ListenerQueue = rx_blockqueue[i];
        params.Timeslot = i;
        
        xTaskCreatePinnedToCore(
            task_dechopper,        // Task function
            "De-chopper",     // Name
            4096,             // Stack size
            &params,             // Parameters
            6,                // Priority
            NULL,             // Task handle
            1                 // Core ID (0 or 1)
        );
    }
}








