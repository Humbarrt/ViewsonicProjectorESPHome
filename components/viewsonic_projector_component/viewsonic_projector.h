#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <mutex>
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
            std::mutex uart_mutex_;
            int _last_light_source_usage_time = -1;
            PowerState _last_power_state = PowerState::OFF;
            int _payload_length(const std::vector<uint8_t> &header);
            uint8_t _checksum(const std::vector<uint8_t> &data);
            std::vector<uint8_t> _send_packet(const std::string &query, bool &error);
            bool _send_write(const std::string &query);
            std::vector<uint8_t> _send_read(const std::string &query);
            std::map<std::string, std::string> read_mapping = {
                {"ON", "051400030000000017"},
                {"OFF", "051400030000000118"},
                {"WARM_UP", "051400030000000219"},
                {"COOL_DOWN", "05140003000000031a"}};
        };

    } // namespace viewsonic_projector
} // namespace esphome