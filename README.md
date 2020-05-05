# Vulkan Testing

This is a Vulkan Testing application that I created to learn the [Vulkan](https://www.khronos.org/vulkan/) API

## Prerequisites

It requires [Visual Studio](https://visualstudio.microsoft.com/vs/) for the development environment, [vcpkg](https://github.com/microsoft/vcpkg) for package management, and the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)

## Installing

Run vcpkg to install
- glm:x64-windows
- sdl2[vulkan]:x64-windows
- stb:x64-windows
- tinyobjloader:x64-windows
- imgui:x64-windows

Run the compile.ps1 script in the shaders subfolder to build the shader binaries

NOTE:  The project is setup up for [user-wide MSBuild integration](https://github.com/microsoft/vcpkg/blob/master/docs/users/integration.md)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
