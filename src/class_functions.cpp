#include <main.h>

///Netværkspakker
bool network_package::parse_header(Package_queue_item* original_buffer) {
    constexpr size_t header_size = offsetof(network_package, payload);

    if (original_buffer->length < header_size) {
        return false;
    }

    memcpy(this, original_buffer->data, header_size);
    return true;
}

bool network_package::parse_payload(Package_queue_item* original_buffer) {
    constexpr size_t header_size = offsetof(network_package, payload);
    constexpr size_t len_field_size = sizeof(uint16_t);

    if (original_buffer->length < header_size + len_field_size) {
        return false;
    }

    this->payload.len = *(uint16_t*)&original_buffer->data[header_size];

    if (original_buffer->length < header_size + len_field_size + this->payload.len) {
        return false;
    }

    memcpy(this->payload.data, &original_buffer->data[header_size + len_field_size], this->payload.len);
    return true;
}

bool network_package::add_hop(uint8_t node_id){
    if(this->hops >= 16){
        return false;
    }
    this->hop_list[this->hops] = node_id;
    this->hops++;
}



bool network_package::serialize(Package_queue_item* dest) {
    constexpr size_t header_size = offsetof(network_package, payload);
    constexpr size_t len_field_size = sizeof(uint16_t);
    size_t total_size = header_size + len_field_size + this->payload.len;
    // Tjek at det hele kan være i destination bufferen
    if (total_size > sizeof(dest->data)) {
        return false;
    }
    // Kopier header
    memcpy(dest->data, this, header_size);

    // Skriv payload længde
    *(uint16_t*)&dest->data[header_size] = this->payload.len;

    // Kopier payload data
    memcpy(&dest->data[header_size + len_field_size], this->payload.data, this->payload.len);

    // Sæt længden så modtageren ved hvor meget data der er
    dest->length = total_size;

    return true;
}


void block_item::print_payload()
{
    Serial.printf("Block payload [32 bytes]:\n");

    for (int i = 0; i < 32; i++)
    {
        if (i % 8 == 0)
            Serial.printf("  %02X: ", i);

        Serial.printf("%02X ", block_payload[i]);

        if (i % 8 == 7)
        {
            Serial.print(" |");
            for (int j = i - 7; j <= i; j++)
            {
                char c = block_payload[j];
                Serial.print((c >= 32 && c < 127) ? c : '.');
            }
            Serial.println("|");
        }
    }
    Serial.println();
}

void Package_queue_item::debug_msg()
{
    Serial.printf("Package [%u bytes]:\n", this->length);

    for (uint16_t i = 0; i < this->length; i++)
    {
        // Adresse i starten af hver række
        if (i % 16 == 0)
            Serial.printf("  %04X: ", i);

        Serial.printf("%02X ", this->data[i]);

        // ASCII-kolonne + linjeskift i slutningen af hver række
        if ((i % 16 == 15) || (i == this->length - 1))
        {
            // Pad hvis sidste række ikke er fuld
            uint16_t pad = 15 - (i % 16);
            for (uint16_t p = 0; p < pad; p++)
                Serial.print("   ");

            Serial.print(" |");
            uint16_t row_start = i - (i % 16);
            for (uint16_t j = row_start; j <= i; j++)
            {
                char c = this->data[j];
                Serial.print((c >= 32 && c < 127) ? c : '.');
            }
            Serial.println("|");
        }
    }
    Serial.println();
}
