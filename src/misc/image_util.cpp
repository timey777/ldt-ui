#include "image_util.h"
#include "misc/stb_image.h"
#include <iostream>

namespace ldt {

bool ImageUtil::getImageSize(const std::string& path, int& width, int& height) {
    int comp;
    int ok = stbi_info(path.c_str(), &width, &height, &comp);
    if (ok) {
        return true;
    } else {
        // std::cerr << "[ImageUtil] Failed to get image info for: " << path << std::endl;
        return false;
    }
}

unsigned char* ImageUtil::loadImage(const std::string& path, int& width, int& height, int& channels) {
    // Force 4 channels (RGBA)
    return stbi_load(path.c_str(), &width, &height, &channels, 4);
}

void ImageUtil::freeImage(unsigned char* data) {
    if (data) {
        stbi_image_free(data);
    }
}

} // namespace ldt
