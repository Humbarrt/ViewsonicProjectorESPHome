#include "esphome.h"
#include "esphome/core/helpers.h"
#include "viewsonic_projector.h"

namespace esphome
{
    namespace viewsonic_projector
    {
        static const char *TAG = "empty_uart_component.component";
        void ViewsonicProjectorComponent::setup()
        {
        }

        void ViewsonicProjectorComponent::loop()
        {
            // This will be called by ESPHome every loop iteration
        }

        void ViewsonicProjectorComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Empty UART component");
        }

        void ViewsonicProjectorComponent::power_on()
        {
            if (!_send_write("0614000400341100005d"))
            {
                ESP_LOGE(TAG, "Error powering on");
                return;
            }
            ESP_LOGI(TAG, "Powered on successfully");
        }

        void ViewsonicProjectorComponent::power_off()
        {
            if (!_send_write("0614000400341101005e"))
            {
                ESP_LOGE(TAG, "Error powering off");
                return;
            }
            ESP_LOGI(TAG, "Powered off successfully");
        }

        bool ViewsonicProjectorComponent::is_powered_on()
        {
            get_power_state();
            return _last_power_state != PowerState::OFF;
        }

        std::string ViewsonicProjectorComponent::get_power_state()
        {
            PowerState state = _last_power_state;
            std::vector<uint8_t> response = _send_read("071400050034000011005e");
            if (response.empty())
            {
                ESP_LOGE(TAG, "Error reading power state");
                return "UNKNOWN";
            }

            std::string response_str(response.begin(), response.end());

            if (response == read_mapping["ON"])
            {
                _last_power_state = PowerState::ON;
                return "ON";
            }
            else if (response == read_mapping["OFF"])
            {
                _last_power_state = PowerState::OFF;
                return "OFF";
            }
            else if (response == read_mapping["WARM_UP"])
            {
                _last_power_state = PowerState::WARM_UP;
                return "WARM_UP";
            }
            else if (response == read_mapping["COOL_DOWN"])
            {
                _last_power_state = PowerState::COOL_DOWN;
                return "COOL_DOWN";
            }

            ESP_LOGE("Unknown power state: %s", response_str.c_str());
            return "UNKNOWN";
        }

        int ViewsonicProjectorComponent::get_light_source_usage_time()
        {
            std::vector<uint8_t> response = _send_read("0714000500340000150163");
            if (response.empty())
            {
                ESP_LOGE(TAG, "Error reading light source usage time");
                return _last_light_source_usage_time;
            }

            int stat = static_cast<int>(response[7]) |
                       (static_cast<int>(response[8]) << 8) |
                       (static_cast<int>(response[9]) << 16) |
                       (static_cast<int>(response[10]) << 24);
            _last_light_source_usage_time = stat;
            return stat;
        }

        int ViewsonicProjectorComponent::_payload_length(const std::vector<uint8_t> &header)
        {
            uint8_t lsb = header[3];
            uint8_t msb = header[4];
            uint8_t ck = 1; // 1-byte checksum at the end
            return lsb + (msb << 8) + ck;
        }

        uint8_t ViewsonicProjectorComponent::_checksum(const std::vector<uint8_t> &packet)
        {
            uint8_t sum = 0;
            for (size_t i = 1; i < packet.size() - 1; ++i)
            {
                sum += packet[i];
            }
            return sum % 256;
        }

        void ViewsonicProjectorComponent::_reset_input_buffer()
        {
            ESP_LOGI(TAG, "Resetting input buffer");
            int bytes_trashed = 0;
            while (this->available() > 0)
            {
                this->read();
                bytes_trashed++;
            }
            ESP_LOGI(TAG, "Trashed %d bytes", bytes_trashed);
        }

        bool ViewsonicProjectorComponent::_read_array(size_t num_bytes, uint8_t *data)
        {
            uint32_t start = millis();
            size_t data_size = 0;
            while (data_size < num_bytes)
            {
                delay(10);
                if (millis() - start > 1000)
                {
                    ESP_LOGE(TAG, "Timeout reading response");
                    return false;
                }

                if (this->available() == 0)
                {
                    continue;
                }

                data[data_size] = this->read();
                data_size++;
                ESP_LOGI(TAG, "Read %d byte: %02x", data_size, data[data_size - 1]);
            }

            ESP_LOGI(TAG, "Read %d bytes", data_size);

            return true;
        }

        std::vector<uint8_t> ViewsonicProjectorComponent::_send_packet(const std::string &query, bool &error)
        {
            uart_mutex_.lock();
            std::vector<uint8_t> query_bytes;
            for (size_t i = 0; i < query.length(); i += 2)
            {
                std::string byteString = query.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
                query_bytes.push_back(byte);
            }
            ESP_LOGI(TAG, "Query: %s -> %s", query.c_str(), format_hex_pretty(query_bytes).c_str());

            _reset_input_buffer();
            delay(50);

            for (auto byte : query_bytes)
            {
                this->write_byte(byte);
            }
            this->flush();

            std::vector<uint8_t> response_header(5);
            // this->read_array(response_header.data(), response_header.size());
            bool ok = _read_array(response_header.size(), response_header.data());
            if (!ok)
            {
                ESP_LOGE(TAG, "Error reading response header");
                error = true;
                uart_mutex_.unlock();
                return {};
            }
            ESP_LOGI(TAG, "HEADER: %s", format_hex_pretty(response_header).c_str());

            int payload_num_bytes = _payload_length(response_header);
            std::vector<uint8_t> response_payload(payload_num_bytes);
            // this->read_array(response_payload.data(), response_payload.size());
            ok = _read_array(response_payload.size(), response_payload.data());
            if (!ok)
            {
                ESP_LOGE(TAG, "Error reading response payload");
                error = true;
                uart_mutex_.unlock();
                return {};
            }
            ESP_LOGI(TAG, "Payload: %s", format_hex_pretty(response_payload).c_str());

            std::vector<uint8_t> response = response_header;
            response.insert(response.end(), response_payload.begin(), response_payload.end());

            if (_checksum(response) != response.back())
            {
                ESP_LOGE(TAG, "Invalid checksum");
                error = true;
                uart_mutex_.unlock();
                return {};
            }

            std::vector<uint8_t> HEADER_DISABLED = {0x00, 0x14, 0x00, 0x00, 0x00, 0x14};
            std::vector<uint8_t> HEADER_PROJ_OFF = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

            if (response == HEADER_DISABLED)
            {
                ESP_LOGE(TAG, "Function is disabled");
                error = true;
                uart_mutex_.unlock();
                return {};
            }

            if (response == HEADER_PROJ_OFF)
            {
                ESP_LOGE(TAG, "Projector is off");
                error = true;
                uart_mutex_.unlock();
                return {};
            }

            error = false;
            uart_mutex_.unlock();
            return response;
        }

        bool ViewsonicProjectorComponent::_send_write(const std::string &query)
        {
            bool error;
            std::vector<uint8_t> response = _send_packet(query, error);
            if (error || response != std::vector<uint8_t>{0x03, 0x14, 0x00, 0x00, 0x00, 0x14})
            {
                ESP_LOGE(TAG, "Command failed");
                return false;
            }
            return true;
        }

        std::vector<uint8_t> ViewsonicProjectorComponent::_send_read(const std::string &query)
        {
            bool error;
            std::vector<uint8_t> response = _send_packet(query, error);
            if (error)
            {
                ESP_LOGE(TAG, "Error sending read command");
                return {};
            }
            return response;
        }
    };
};