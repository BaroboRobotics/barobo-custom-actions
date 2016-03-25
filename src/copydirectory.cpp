#include "copydirectory.hpp"

#include <string>
#include <stdexcept>

namespace fs = boost::filesystem;

// Modified from http://www.technical-recipes.com/2014/using-boostfilesystem/#CopyingDirectory
void copyDirectory (const fs::path& source, const fs::path& destination) {
    // Check whether the function call is valid
    if (!fs::exists(source) || !fs::is_directory(source)) {
        throw std::runtime_error(std::string("Source directory ")
                + source.string() + " does not exist or is not a directory.");
    }

    if (fs::exists(destination)) {
        throw std::runtime_error(std::string("Destination directory ")
                + destination.string() + " already exists.");
    }

    // Create the destination directory
    if (!fs::create_directory(destination)) {
        throw std::runtime_error(std::string("Unable to create destination directory")
                + destination.string());
    }

    // Iterate through the source directory
    for (auto&& file : fs::directory_iterator(source)) {
        auto current = file.path();
        if (fs::is_directory(current)) {
            // Found directory: Recursion
            copyDirectory(current, destination / current.filename());
        }
        else {
            // Found file: Copy
            fs::copy_file(current, destination / current.filename());
        }
    }
}
