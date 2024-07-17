# FlatOut 2 DB Tool

Tool for reading and writing FlatOut 2 database files

## Usage

- Enter a commandline prompt, run `FlatOut2DBExtractor_gcp.exe (filename)`
- There will now be a folder with the extracted contents of the input file
- After making the desired changes, run `FlatOut2DBMaker_gcp.exe (filename)` in a commandline prompt
- The db will now be repacked with your changes
- Enjoy, nya~ :3

## Building

Building is done on an Arch Linux system with CLion and vcpkg being used for the build process.

Required packages: `mingw-w64-gcc`
