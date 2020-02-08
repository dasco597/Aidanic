#include "Aidanic.h"
#include "tools/Log.h"

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main() {
    Aidanic app;
    std::cout << "I'm Aidanic, nice to meeet you!" << std::endl;
    try {
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        //system("pause");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void Aidanic::Run() {
    init();
    loop();
}

Aidanic::~Aidanic() { cleanUp(); }

void Aidanic::init() {
    Log::Init();
    LOG_INFO("Logger initialized");
}

void Aidanic::loop() {

}

void Aidanic::cleanUp() {

}