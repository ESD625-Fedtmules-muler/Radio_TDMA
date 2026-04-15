#include <Arduino.h>
#include <Hamming.h>
#include <main.h>


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




struct chopper_header
{
    uint64_t ID = MAGIC_NUMBER; 
    uint8_t number_of_frames = 0;
    uint8_t tail_length = 0; //How long the last block is

    void enqueue(QueueHandle_t* queue_to_send_to){
        block_item header_item;
        uint8_t buf[32] = {0};
        memcpy(buf, &ID, 8);
        buf[4] = number_of_frames;
        buf[5] = tail_length;

        codec.encode(buf, bytes_pr_frame, header_item.block_payload, 32);        
        xQueueSend(*queue_to_send_to, &header_item, portMAX_DELAY);
    }
};


struct dechopper_header
{
    static const uint64_t MAGIC = MAGIC_NUMBER;
    uint8_t number_of_frames = 0;
    uint8_t tail_length = 0;

    // Returnerer true hvis headeren er valid
    bool decode_from_block(block_item* block)
    {
        uint8_t buf[bytes_pr_frame] = {0};
        //block->print_payload();
        codec.decode(block->block_payload, codec.encodedBits(bytes_pr_frame), buf, (bytes_pr_frame)); //!Obs Den tager bits som parameter og ikke bytes
        uint64_t received_id = 0;
        memcpy(&received_id, buf, 4);
        if (received_id != MAGIC)
            return false;
        number_of_frames = buf[4];
        tail_length = buf[5];
        return true;
    }
};


struct param_dechopper{
    QueueHandle_t ListenerQueue;
};

void task_dechopper(void *pvParameters)
{
    param_dechopper* params = (param_dechopper*)pvParameters;
    QueueHandle_t listener_queue = params->ListenerQueue; 
    while (true)
    {        
        block_item incoming_block;

        // Vent på første blok - den skal være en header
        if (!xQueueReceive(listener_queue, &incoming_block, portMAX_DELAY))
            continue;
        // Forsøg at decode headeren
        dechopper_header header;
        if (!header.decode_from_block(&incoming_block))
        {
            // Ikke en gyldig header - skip og prøv igen
            Serial.print("Header error\n");
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

            chopper_header header_content;
            header_content.number_of_frames = number_of_frames;
            header_content.tail_length      = tail_length;
            header_content.enqueue(&tx_blockqueue);

            // Loop over fulde frames
            for (uint8_t f = 0; f < number_of_frames; f++)
            {
                bool is_last   = (f == number_of_frames - 1);
                uint16_t flen  = (is_last && tail_length > 0) ? tail_length : bytes_pr_frame;

                block_item outputbuffer;
                codec.encode(&input_package.data[f * bytes_pr_frame], flen,
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

    //Så skal vi lige have spawnet 10 dechoppers

    
    for (size_t i = 0; i < MAX_NODES; i++)
    {
        param_dechopper params;
        params.ListenerQueue = rx_blockqueue[i];

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








