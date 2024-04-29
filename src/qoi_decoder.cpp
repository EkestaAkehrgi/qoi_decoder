#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <cstdint>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

#define TAG_RGB 0xfe
#define TAG_RGBA 0xff
#define TAG_INDEX 0x00
#define TAG_DIFF 0x40
#define TAG_LUMA 0x80
#define TAG_RUN 0xc0

#define TAG_MASK 0xc0
#define MASK_6BIT 0x3f

#define DIFF_RED 0x30
#define DIFF_GREEN 0x0c
#define DIFF_BLUE 0x03

#define LUMA_GREEN MASK_6BIT
#define LUMA_DIFF_R 0xf0
#define LUMA_DIFF_B 0x0f

typedef struct rgba
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} colour;

void insertIntoSeenData(colour *array, const colour val)
{

    int index_position = (val.r * 3 + val.g * 5 + val.b * 7 + val.a * 11) % 64;
    array[index_position] = val;
}

uint32_t read4byte(std::vector<uint8_t>& buffer, int& bufferIndex){

    return (buffer[bufferIndex++] << 24) |
           (buffer[bufferIndex++] << 16) |
           (buffer[bufferIndex++] << 8) |
           (buffer[bufferIndex++]);
}
int main()
{
    const int qoiHeader = 14;
    const int lastInd = 8;

    std::string filePath = "../assets/qoi_test_images/kodim10.qoi";
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Unable to open file '" << filePath << "'\n";
        return 1;
    }

    // Determining the file size
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    if (fileSize < qoiHeader + lastInd)
    {
        std::cerr << "Error: Corrupt qoi file \n";
    }

    // Creating a buffer (for ease) and reading file data into it
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char *>(buffer.data()), fileSize))
    {
        std::cerr << "Error: Failed to read file '" << filePath << "'\n";
        return 1;
    }
    if (file.fail() || file.gcount() != fileSize)
    {
        std::cerr << "Error: Failed to read file '" << filePath << "'\n";
        return 1;
    }

    file.close();

    // -----------------Now buffer vector contains qoi file data

    int bufferIndex{};

    constexpr uint32_t MAGIC_NUM = ((unsigned int)'q') << 24 |
                                   ((unsigned int)'o') << 16 |
                                   ((unsigned int)'i') << 8 |
                                   ((unsigned int)'f');

    uint32_t aux = read4byte(buffer, bufferIndex);

    if (aux != MAGIC_NUM)
    {
        std::cerr << "Error: Qoi header magic number is corrupt\n";
        return 1;
    }

    colour prevPixel{0, 0, 0, 255};
    colour seenData[64]{};

    uint32_t imgWidth = read4byte(buffer, bufferIndex);

    uint32_t imgHeight = read4byte(buffer, bufferIndex);

    int channel = buffer[bufferIndex++];
    int clrspace = buffer[bufferIndex++];

    const int imgSize = imgHeight * imgWidth * channel;
    std::vector<uint8_t> image(imgSize);


    int imgIndex{};

    while (imgIndex < imgSize)
    {

        uint8_t byte = buffer[bufferIndex++];

        if (byte == TAG_RGB)
        {

            prevPixel.r = buffer[bufferIndex++];
            prevPixel.g = buffer[bufferIndex++];
            prevPixel.b = buffer[bufferIndex++];
        }
        else if (byte == TAG_RGBA)
        {

            prevPixel.r = buffer[bufferIndex++];
            prevPixel.g = buffer[bufferIndex++];
            prevPixel.b = buffer[bufferIndex++];
            prevPixel.a = buffer[bufferIndex++];
        }
        else
        {

            if ((byte & TAG_MASK) == TAG_RUN)
            {

                int runLength = (byte & MASK_6BIT);
                for (int i = 0; i < runLength; i++)
                {
                    if (channel == 4)
                    {
                        image[imgIndex++] = prevPixel.r;
                        image[imgIndex++] = prevPixel.g;
                        image[imgIndex++] = prevPixel.b;
                        image[imgIndex++] = prevPixel.a;
                    }
                    else
                    {
                        image[imgIndex++] = prevPixel.r;
                        image[imgIndex++] = prevPixel.g;
                        image[imgIndex++] = prevPixel.b;
                    }
                }
            }
            else if ((byte & TAG_MASK) == TAG_INDEX)
            {

                int hash = byte & MASK_6BIT;
                colour aux = seenData[hash];

                prevPixel.r = aux.r;
                prevPixel.g = aux.g;
                prevPixel.b = aux.b;
                prevPixel.a = aux.a;
            }
            else if ((byte & TAG_MASK) == TAG_DIFF)
            {

                int red = ((byte & DIFF_RED) >> 4) - 2;
                int green = ((byte & DIFF_GREEN) >> 2) - 2;
                int blue = ((byte & DIFF_BLUE) >> 0) - 2;

                prevPixel.r = prevPixel.r + red;
                prevPixel.g = prevPixel.g + green;
                prevPixel.b = prevPixel.b + blue;
            }
            else if ((byte & TAG_MASK) == TAG_LUMA)
            {

                int dg = (byte & LUMA_GREEN) - 32;
                int nextByte = buffer[bufferIndex++];

                int drdg = ((nextByte & LUMA_DIFF_R) >> 4) - 8;
                int dbdg = ((nextByte & LUMA_DIFF_B) >> 0) - 8;

                int dr = drdg + dg;
                int db = dbdg + dg;

                prevPixel.r = prevPixel.r + dr;
                prevPixel.g = prevPixel.g + dg;
                prevPixel.b = prevPixel.b + db;
            }
        }
        insertIntoSeenData(seenData, prevPixel);
        if (channel == 4)
        {
            image[imgIndex++] = prevPixel.r;
            image[imgIndex++] = prevPixel.g;
            image[imgIndex++] = prevPixel.b;
            image[imgIndex++] = prevPixel.a;
        }
        else
        {
            image[imgIndex++] = prevPixel.r;
            image[imgIndex++] = prevPixel.g;
            image[imgIndex++] = prevPixel.b;
        }
    }
    stbi_write_bmp("output.bmp", imgWidth, imgHeight, channel, image.data());
    return 0;
}
