#include <main.h>
#include <Arduino.h>


#define Recieve_Table_size 32



/// @brief Table der holder hvilke network pakker der er kommet i seneste tid.
class recv_table_t {
private:
    uint8_t Nonce_list[Recieve_Table_size] = {0};
    uint16_t  UID_list[Recieve_Table_size] = {0};
    uint16_t index_pointer = 0;
    uint16_t size = Recieve_Table_size;
public:
    /// @brief Creates a table to hold entries of previous known transmissions on the network
    /// @param num_entries The length of said table der må MAX være 255 idecies som det er nu...
    recv_table_t() {

    }
    void add_entry(uint16_t UID, uint8_t Nonce){
        UID_list[index_pointer] = UID;
        Nonce_list[index_pointer] = Nonce;
        index_pointer = (index_pointer + 1) % size;
    }

    bool exists(uint16_t UID, uint8_t Nonce){
        for (size_t i = 0; i < this->size; i++)
        {
            if((this->UID_list[i] == UID) && (this->Nonce_list[i] == Nonce)){
                //If this nonce for the user exists we return true
                return true;
            }
        }
        return false;
    }
};








extern QueueHandle_t rx_package_queue;
extern QueueHandle_t tx_package_queue;

size_t current_listener = 0;

#define max_number_listenings 10
uint16_t listening_UIDS[max_number_listenings] = {0xFFFF}; //De UID'er vi lytter på
QueueHandle_t Recv_queues[max_number_listenings] = {NULL}; //Tilhørende Queues til disse UID'er

bool check_user_listerning(network_package* rx_network_package, size_t* rx_userchannel){
    for (size_t i = 0; i < max_number_listenings; i++)
    {
        if(rx_network_package->source_UID == listening_UIDS[i]){
            *rx_userchannel = i;
            return true;
        }
    }
    return false;
}

bool check_TTL(network_package* package){
    return package->hops < network_params.TTL;
}




void task_router(void *pvParameters){
    //* Vores Stucts brugt i dne her fil
    recv_table_t recieved_message_table = recv_table_t(); //Creates 32 indecies of existing users to remember for the current node.
    while (true)
    {   
        Package_queue_item rx_package;
        if(!xQueueReceive(rx_package_queue, &rx_package, portMAX_DELAY)){
            continue;
        }
        //If we got a package.
        network_package rx_network_package;
        rx_network_package.parse_header(&rx_package);
        
        //Tjekker om det en pakke vi har arbejdet med før
        if(recieved_message_table.exists(rx_network_package.source_UID, rx_network_package.nonce)){
            continue;
        }
        recieved_message_table.add_entry(rx_network_package.source_UID, rx_network_package.nonce);

        size_t possible_channel = 0; 
        //Checker om det var en pakke der skulle ud til en userQueue
        if(check_user_listerning(&rx_network_package, &possible_channel)){
            rx_network_package.parse_payload(&rx_package);
            xQueueSend(Recv_queues[possible_channel], &rx_network_package, 0);
            if(rx_network_package.msg_type == Broadcast && check_TTL(&rx_network_package)){ //Så hvis det nu var en broadcast og vi kan retrans så gør vi det
                rx_network_package.add_hop(network_params.node_id); //Tilføjer os som hop
                rx_network_package.serialize(&rx_package); //Serializer tilbage ind i vores rx_package
                xQueueSend(tx_package_queue, &rx_package, portMAX_DELAY); //Sender ham videre ud i verden
            }
            continue; //Videre i livet næste pakke
        }

        if(!check_TTL(&rx_network_package)){
            continue;
        }
        //Hvis vi er her så det en pakke vi nok burde transmitte
        if(rx_network_package.msg_type == P2P || rx_network_package.msg_type == Broadcast){
            rx_network_package.add_hop(network_params.node_id); //Tilføjer os som hop
            rx_network_package.parse_payload(&rx_package);
            rx_network_package.serialize(&rx_package); //Serializer tilbage ind i vores rx_package
            xQueueSend(tx_package_queue, &rx_package, portMAX_DELAY); //Sender ham videre ud i verden
        }
    }
}




void setup_router(){
    xTaskCreatePinnedToCore(
        task_router,        // Task function
        "Router-core",     // Name
        8196,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        NULL,             // Task handle
        1                 // Core ID (0 or 1)
    );
}

bool setup_listener(uint16_t UID){
    
}



void Send_data(uint16_t UID,uint8_t* buffer, size_t length){

}