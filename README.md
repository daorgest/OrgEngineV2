# OrgEngine V2 (?)

## This is my 3rd attempt on making a game engine from scratch this time with some extra features such as:
 - RHI Support
    - This was a choice made by me since doing raw Vulkan calls was making me go insane.
    - All classes that inherits from the RHI base class is marked as final to help with devirtualization
    - Future graphics apis can be implemented easily without changing the Engine code....as much
    - A learning experience for getting the grasp of OOP.
 
 - Entity Components (without the System)
 - Slang Support
 - SDL Support (other platforms not tested yet)
 - Model Support
    - fbx (with ufbx)
    - obj (with tinyobj) **(soon to be deprecated)**
    - gltf (with fastgltf)

## Want to try it out urself?
 1. Pull this repo with the submodules:
    ```bash
    git clone --recurse-submodules (this repo url is private lol)
    ```
 
    - if you already cloned this repo without the --recursive flag you can do this:
        ```bash
        git submodule update --init --recursive
        ```
 2. Create a build directory
    ```bash
    mkdir build
    cd build
    ``` 
 3. Build
    ```bash
    cmake --build . --config Release
    ```
## Some notes for myself and TODOS
 - I'm aware that the platform layer is NOT OOP as its 1 header, n amount of implementation files
 - Audio implementation is still yet to be started on
 - GameInput is a nice API for windows but the documentation is so ASS I had to look at SDL for ref
 - Order of destruction is still a WIP
## Requirements to Run
 - Windows 10 64-Bit (22H2) or newer
 - CMake 4.0 or newer
 - Vulkan with these extensions supported
   - VK_EXT_scalar_block_layout
   - VK_KHR_synchronization_2
   - VK_KHR_buffer_device_address
   - VK_KHR_unified_image_layouts
   - VK_EXT_descriptor_indexing
   - VK_EXT_present_mode_fifo_latest_ready (Optional)
 