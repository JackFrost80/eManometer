#include "Globals.h"

const std::vector<String> TempLabelsShort = {
    "C",
    "F",
    "K"
};

const std::vector<String> TempLabels = {
    "Celsius",
    "Fahrenheit",
    "Kelvin"
};

std::vector<String> RemoteAPILabels = {
    "Off",
    "Ubidots",
    "CraftBeerPi",
    "HTTP",
    "TCP",
    "InfluxDB",
    "Prometheus",
    "MQTT",
    "ThingSpeak"
};

controller_t Controller_;
p_controller_t p_Controller_ = &Controller_;
statistics_t Statistic_;
p_statistics_t p_Statistic_ = &Statistic_;
basic_config_t Basic_config_;
p_basic_config_t p_Basic_config_= &Basic_config_; 