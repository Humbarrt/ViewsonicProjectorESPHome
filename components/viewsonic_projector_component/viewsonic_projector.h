#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include <map>

namespace esphome
{
    namespace viewsonic_projector
    {
        enum PowerState
        {
            ON,
            OFF,
            WARM_UP,
            COOL_DOWN
        };

        class ViewsonicProjectorComponent : public uart::UARTDevice, public Component
        {
        public:
            void setup() override;
            void loop() override;
            void dump_config() override;
            void power_on();
            void power_off();
            bool is_powered_on();
            std::string get_power_state();
            int get_light_source_usage_time();

        private:
            esphome::Mutex uart_mutex_;
            int _last_light_source_usage_time = -1;
            PowerState _last_power_state = PowerState::OFF;
            int _payload_length(const std::vector<uint8_t> &header);
            uint8_t _checksum(const std::vector<uint8_t> &data);
            std::vector<uint8_t> _send_packet(const std::string &query, bool &error);
            bool _send_write(const std::string &query);
            std::vector<uint8_t> _send_read(const std::string &query);
            void _reset_input_buffer();
            bool _read_array(size_t num_bytes, uint8_t *data);
            std::map<std::string, std::vector<uint8_t>> read_mapping = {
                {"OFF", {0x05, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x17}},
                {"ON", {0x05, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x18}},
                {"WARM_UP", {0x05, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x19}},
                {"COOL_DOWN", {0x05, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x1a}}};
        };
    }
}