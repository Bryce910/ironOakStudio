#include "engine/world_json_string.h"
#include <cstdio>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char **) {
    if (argc > 1) {
        const std::string value{std::istreambuf_iterator<char>(std::cin),
                                std::istreambuf_iterator<char>()};
        std::cout << world_json::quote(value);
        return 0;
    }
    const std::string path = R"(C:\users\computer\Temp\valwood-studio-ve97zr/0.txt)";
    const std::string controls{"zero\0tab\tline\nreturn\rback\bform\f", 32};
    std::printf("{\"authored_sources\":[{\"path\":%s,\"instances\":283}],"
                "\"driver\":%s,\"scenic\":%s,\"view\":%s,\"id\":%s,"
                "\"controls\":%s,\"utf8\":%s,\"empty\":%s,\"enabled\":true}\n",
                world_json::quote(path).c_str(),
                world_json::quote("audio \\\"driver\"").c_str(),
                world_json::quote("rainforest-milestone").c_str(),
                world_json::quote("arrival").c_str(),
                world_json::quote("silverpine_waystation_return").c_str(),
                world_json::quote(controls).c_str(),
                world_json::quote(u8"C:\\Users\\Élodie\\城\\house.glb").c_str(),
                world_json::quote("").c_str());
}
