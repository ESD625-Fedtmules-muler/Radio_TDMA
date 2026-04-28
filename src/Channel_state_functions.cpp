#include <gps.h>
#include <network_params.h>

extern TinyGPSPlus gps;


extern Channel_state_table channel_state_table;

const char* antennaDirToStr(AntennaDir dir) {
    switch(dir) {
        case ERROR:       return "ERR ";
        case DIR_TX_OMNI: return "TX_O";
        case DIR_RX_OMNI: return "RX  ";
        case DIR_RX_0:    return "RX_0";
        case DIR_RX_1:    return "RX_1";
        case DIR_RX_2:    return "RX_2";
        case DIR_RX_3:    return "RX_3";
        case DIR_RX_4:    return "RX_4";
        case DIR_RX_5:    return "RX_5";
        case DIR_RX_6:    return "RX_6";
        case DIR_RX_7:    return "RX_7";
        default:          return "????";
    }
}


void Channel_state_table::update_entry(size_t ID, float lat, float lng){
    
    entries[ID].latitude = gps.location.lat();
    entries[ID].longitude = gps.location.lng();
    // entries[ID].hasUpdate = true;
    entries[ID].lifetime = 0;
}

void Channel_state_table::begin(){
    table_mutex = xSemaphoreCreateMutex();

    for(int i = 0; i < network_params.number_of_nodes; i++) {
        entries[i].ID = i;
        entries[i].latitude = 0;
        entries[i].longitude = 0;
        // entries[i].rssi = -100;
        switch_states[i] = ERROR; // Se med i næste afsnit...
        entries[i].lifetime = -1;
        // entries[i].hasUpdate = false;
    }
    
}

/// @brief Runs through the entry table to find the number of known positions
/// @return Returns the number of known positions including your own.... If you know that one of course O_o 
uint8_t Channel_state_table::get_known_positions(){
    uint8_t count = 0;
    for (size_t i = 0; i < MAX_NODES; i++) {
        if (entries[i].lifetime >= 0) count++;
    }
    return count;
}


uint16_t Channel_state_table::serialize(uint8_t *data, size_t len){
    if(!xSemaphoreTake(table_mutex, portMAX_DELAY)){
        return 0;
    }

    size_t offset = 1; // reserver plads til count-byte
    uint8_t packed = 0;

    for (size_t i = 0; i < MAX_NODES; i++){
        if(entries[i].lifetime >= 0){
            if(offset + sizeof(Look_up_entry) > len) break;
            memcpy(data + offset, &entries[i], sizeof(Look_up_entry));
            offset += sizeof(Look_up_entry);
            packed++;
        }
    }

    data[0] = packed; // skriv den reelle count til sidst

    xSemaphoreGive(table_mutex);
    return offset;
}

bool Channel_state_table::evaluate_packet(uint8_t *data, size_t len, uint8_t src = -1){
    if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
        return false;
    }
    uint16_t num_entries = data[0];

    if(num_entries == 0){
        Serial.printf("Drone no.: %d does not know any positions, including his own", src);
    }
    if(num_entries > network_params.number_of_nodes){
        Serial.printf("KAT DER SKRIGER, fik %d nodes i gps table", num_entries);
        xSemaphoreGive(table_mutex);
        return false;
    }

    size_t offset = 1;
    for (size_t i = 0; i < num_entries; i++)
    {
        Look_up_entry incoming;
        memcpy(&incoming, data + offset, sizeof(Look_up_entry));
        if((entries[incoming.ID].lifetime > incoming.lifetime) || (entries[incoming.ID].lifetime == -1)){
            
            memcpy(&entries[incoming.ID], &incoming, sizeof(incoming));
        }
        offset += sizeof(Look_up_entry);
    }

    xSemaphoreGive(table_mutex);
    return true;
}

//Updates the lifetime in seconds
bool Channel_state_table :: update_life_times(int8_t delta_time){
    if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
        return false;
    }
    for (size_t i = 0; i < MAX_NODES; i++)
    {
        if(entries[i].lifetime >= 0){
            entries[i].lifetime += delta_time;
        }
        if(entries[i].lifetime > network_params.pos_timeout){
            entries[i].lifetime = -1;
        }
    }
    xSemaphoreGive(table_mutex);
    return true;
}

Look_up_entry Channel_state_table::get_entry(uint8_t node_id) {
    if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
        Look_up_entry ENTRY;
        Serial.println("Kunne ikke få mutex til entry");
        xSemaphoreGive(table_mutex); //Returns the semaphore after use

        return ENTRY;
    }
    xSemaphoreGive(table_mutex); //Returns the semaphore after use
    return entries[node_id];
}


void Channel_state_table::printLookUpTable() {
    Serial.println("---- LOOK UP TABLE ----");
    Serial.println("ID   | ANT  | P_SIG(dBm) | P_CH(dBm) | LAT        | LON        | AGE | DIST     | HDG");
    Serial.println("-----|------|------------|-----------|------------|------------|-----|----------|-----");
    for (int i = 0; i < network_params.number_of_nodes; i++) {
        char line[150];
        
        char p_sig_str[12], p_ch_str[12];
        if(P_Signal[i] <= -200.0f) snprintf(p_sig_str, sizeof(p_sig_str), "    N/A   ");
        else                        snprintf(p_sig_str, sizeof(p_sig_str), "%7.2f dBm", P_Signal[i]);
        
        if(P_Channel[i] <= -200.0f) snprintf(p_ch_str, sizeof(p_ch_str), "   N/A  ");
        else                         snprintf(p_ch_str, sizeof(p_ch_str), "%6.2f dBm", P_Channel[i]);

        if (entries[i].lifetime >= 0) {
            snprintf(line, sizeof(line), "%2d%s | %s | %s | %s | %10.6f | %10.6f | %3ds | %8.2fm | %.1f°",
                i,
                (i == NODE_ID) ? "*" : " ",
                antennaDirToStr(switch_states[i]),
                p_sig_str,
                p_ch_str,
                entries[i].latitude,
                entries[i].longitude,
                entries[i].lifetime,
                calculate_distance(entries[NODE_ID].latitude, entries[NODE_ID].longitude, entries[i].latitude, entries[i].longitude),
                calculate_bearing(entries[NODE_ID].latitude, entries[NODE_ID].longitude, entries[i].latitude, entries[i].longitude)
            );
        } else {
            snprintf(line, sizeof(line), "%2d%s | %s | %s | %s | NO DATA",
                i,
                (i == NODE_ID) ? "*" : " ",
                antennaDirToStr(switch_states[i]),
                p_sig_str,
                p_ch_str
            );
        }
        Serial.println(line);
    }
    Serial.println("-----------------------");
}