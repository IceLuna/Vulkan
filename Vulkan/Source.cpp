#include <iostream>
#include "Core/Application.h"

int main() 
{
    std::cout << "Creating application...\n";
    Application app(800, 600, "Hello, Vulkan!");

    std::cout << "Running application...\n";
    app.Run();
    std::cout << "Shutting down application...\n";

    return 0;
}