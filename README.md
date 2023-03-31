# Directory watch media plugin for OBS Studio

Plugin for OBS Studio adding a filter that can watch a directory for media files

# Download

https://obsproject.com/forum/resources/directory-watch-media.801/

# Build
1. In-tree build
    - Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
    - Check out this repository to plugins/dir-watch-media
    - Add `add_subdirectory(dir-watch-media)` to plugins/CMakeLists.txt
    - Rebuild OBS Studio

1. Stand-alone build (Linux only)
    - Verify that you have package with development files for OBS
    - Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

# Donations
https://www.paypal.me/exeldro
