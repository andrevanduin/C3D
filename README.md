# C3DEngine
Fully featured game engine written from scratch. Using only a few simple libraries from other sources.
Currently the rendering is done in Vulkan but the ambition is to support multiple rendering backends.

Current focus is on improving the Vulkan renderer to be super fast and support the latest features.

## Custom standard library implementations written from scratch
- String
    - Supports small string optimization (SSO)
    - Automatically handles dynamic memory allocation for you
    - Supports the use of custom alloctors (dynamic, linear, malloc etc.)
    - Lot's of convience functions (Split, Join, Substring etc.)
- Vector/dynamic array
    - Automatically handles dynamic memory allocation for you
    - Supports the use of custom alloctors (dynamic, linear, malloc etc.)
- Hashmap
    - Open-Adressing using Robin Hood probing and backshift deletion.
    - Optimized for stable performance and small memory footprint
- Circular buffer
- Queue
- Stack
- Ring queue

## Custom memory allocators
- The engine preallocates memory at the start of the application
    - We use this buffer to do all our internal allocations
- Memory metric system to keep track of all your allocations
    - This allows the user to see exactly how many allocations are done
    - Allows the user to be aware when they are leaking memory
    - Supports stacktracing per allocation to show exactly where memory is being leaked
- Linear allocator for fastest possible allocations and scratch buffers / memory arena's
- Dynamic allocator with freelist for more fine-tuned allocations
- Malloc allocator for when the default allocator is needed

## Platform layer
Interface that Abstracts away the platform-specific methods. Currently supports Windows and Linux (X11).
- Supports file watching that sends you an event whenever a watched file is changed
- Dynamic loading of plugins (for example the renderer)
- Abstraction over the file system to make file IO easier
- Multi-window support
- Abstraction over platform-specific input (see Input system)

## General systems
- Event system that supports sending out events to multiple listeners. 
- Job system that supports scheduling jobs on multiple threads
- Input system to handle keyboard and mouse inputs

## Custom testing framework
A testing framework written from scratch to make sure that custom implementations of, for example, containers are working as expected.
Runs all provided tests automatically and provides an overview of the results (pass, fail, not ran) with corresponding error messages.

## Asset loading features
- Loading of .OBJ files
- Loading and saving of custom CSON format (JSON-like format written from scratch)
- Loading of GLSL shader files
    - Includes support for automatically parsing #include statements (without GLSL extensions)

## Vulkan Renderer features
- Custom vulkan allocator written from scratch
- Automatic detection of best GPU based on the required functionality
- Runtime shader compilation from glsl to SPIR-V
- Double or Triple buffering support for smooth image output
- Mesh and Task Shaders
    - Used for cone culling to get rid of meshlets that are back-facing before rasterization

