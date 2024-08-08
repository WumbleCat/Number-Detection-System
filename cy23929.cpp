#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iterator>
#include <algorithm>
#include <unordered_map>
#include <winsock2.h>  // For ntohl on Windows
#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib

// Function to read IDX files
std::vector<uint8_t> readIDXFile(const std::string& filename, std::vector<int>& dimensions) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    // Read the magic number
    uint8_t magicNumber[4];
    file.read(reinterpret_cast<char*>(&magicNumber), 4);

    // Check the data type
    uint8_t dataType = magicNumber[2];
    uint8_t numDimensions = magicNumber[3];

    // Read dimensions
    dimensions.resize(numDimensions);
    for (int i = 0; i < numDimensions; ++i) {
        uint32_t dim;
        file.read(reinterpret_cast<char*>(&dim), sizeof(dim));
        dim = ntohl(dim);  // Convert to host byte order
        dimensions[i] = dim;
    }

    // Read the data
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    return data;
}

// Function to read label files
std::vector<uint8_t> readLabelFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open label file");
    }

    // Read the magic number and number of items
    uint8_t magicNumber[4];
    file.read(reinterpret_cast<char*>(&magicNumber), 4);

    uint32_t numItems;
    file.read(reinterpret_cast<char*>(&numItems), 4);
    numItems = ntohl(numItems);  // Convert to host byte order

    // Read the labels
    std::vector<uint8_t> labels(numItems);
    file.read(reinterpret_cast<char*>(&labels[0]), numItems);

    return labels;
}

// Function to calculate the pairwise distance between two 28x28 images
int pairwise_distance(const std::vector<uint8_t>& train_data, const std::vector<uint8_t>& test_data, int train_index, int test_index) {
    int imageSize = 28 * 28;
    int train_offset = train_index * imageSize;
    int test_offset = test_index * imageSize;

    int total = 0;

    for (int i = 0; i < imageSize; ++i) {
        int diff = static_cast<int>(train_data[train_offset + i]) - static_cast<int>(test_data[test_offset + i]);
        total += diff * diff;
    }

    return total;
}

// Function to print a 28x28 image at a given index from the data
void printImage(const std::vector<uint8_t>& data, const std::vector<int>& dimensions, int index) {
    if (dimensions.size() == 3 && dimensions[1] == 28 && dimensions[2] == 28) {
        int imageSize = 28 * 28;
        int offset = index * imageSize;
        if (offset + imageSize > data.size()) {
            std::cerr << "Index out of bounds" << std::endl;
            return;
        }
        for (int i = 0; i < 28; ++i) {
            for (int j = 0; j < 28; ++j) {
                std::cout << (data[offset + i * 28 + j] > 128 ? "#" : "."); // Thresholding for visualization
            }
            std::cout << std::endl;
        }
    } else {
        std::cerr << "Unexpected dimensions: ";
        for (int dim : dimensions) {
            std::cerr << dim << " ";
        }
        std::cerr << std::endl;
    }
}

int main() {
    std::string train_images_filename = "train-images.idx3-ubyte";
    std::string test_images_filename = "t10k-images.idx3-ubyte";
    std::string train_labels_filename = "train-labels.idx1-ubyte";
    std::string test_labels_filename = "t10k-labels.idx1-ubyte";
    std::vector<int> train_dimensions, test_dimensions;
    std::vector<uint8_t> train_data, test_data;
    std::vector<uint8_t> train_labels, test_labels;

    try {
        train_data = readIDXFile(train_images_filename, train_dimensions);
        test_data = readIDXFile(test_images_filename, test_dimensions);
        train_labels = readLabelFile(train_labels_filename);
        test_labels = readLabelFile(test_labels_filename);

        // Iterate over each test image and find the 10 closest train images
        int num_test_images = test_dimensions[0];
        int num_train_images = train_dimensions[0];
        int k = 5;
        int correct_predictions = 0;

        for (int test_index = 0; test_index < num_test_images; ++test_index) {
            std::vector<std::pair<int, int>> distances;  // Pair of (distance, train_index)

            for (int train_index = 0; train_index < num_train_images; ++train_index) {
                int distance = pairwise_distance(train_data, test_data, train_index, test_index);
                distances.push_back(std::make_pair(distance, train_index));
            }

            // Sort distances and get the 10 smallest
            std::sort(distances.begin(), distances.end());

            // Count the frequency of each label in the 10 closest train images
            std::unordered_map<uint8_t, int> label_counts;
            for (int i = 0; i < k; ++i) {
                uint8_t label = train_labels[distances[i].second];
                label_counts[label]++;
            }

            // Find the label with the highest frequency
            uint8_t predicted_label = 0;
            int max_count = 0;
            for (const auto& pair : label_counts) {
                if (pair.second > max_count) {
                    max_count = pair.second;
                    predicted_label = pair.first;
                }
            }

            // // Output the classification result
            std::cout << "Test Image Index: " << test_index << " classified as: " << static_cast<int>(predicted_label) << std::endl;


            // Check if the prediction is correct
            if (predicted_label == test_labels[test_index]) {
                correct_predictions++;
            }
        }

        // Calculate and print the accuracy
        double accuracy = static_cast<double>(correct_predictions) / num_test_images;
        std::cout << "Classification Accuracy: " << accuracy * 100 << "%" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
