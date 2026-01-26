#include "ConfigManager.h"
#include "Logger.h"
#include <iostream>

int main() {
    // Test basic compilation
    ConfigManager::initialize("test.json");
    Logger::getInstance().info("Test compilation");
    return 0;
}
