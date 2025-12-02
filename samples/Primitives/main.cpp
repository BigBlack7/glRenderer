#include <iostream>
#include <logger.hpp>
int main()
{
    core::Logger::Init();
    GL_INFO("Hello, GLRenderer!");

    return 0;
}