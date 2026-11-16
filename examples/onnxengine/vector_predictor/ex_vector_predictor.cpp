#include <cstddef>
#include <filesystem>
#include <iostream>
#include <vector>

#include "onnxengine/onnxengine.hpp"

#ifndef ONNXENGINE_EXAMPLE_RESOURCE_DIRECTORY
#error "ONNXENGINE_EXAMPLE_RESOURCE_DIRECTORY is not configured"
#endif

int main() {
    const std::filesystem::path resource_directory =
        ONNXENGINE_EXAMPLE_RESOURCE_DIRECTORY;
    if (!std::filesystem::is_directory(resource_directory)) {
        std::cerr << "resource directory does not exist: "
                  << resource_directory.string() << '\n';
        return 1;
    }

    const std::filesystem::path library_path =
        onnxengine::locateLibrary(resource_directory);
    const std::filesystem::path model_path =
        onnxengine::locateModel(resource_directory);
    std::cout << "resource directory: " << resource_directory.string() << '\n';
    std::cout << "library path: " << library_path.string() << '\n';
    std::cout << "model path: " << model_path.string() << '\n';

    onnxengine::VectorPredictor predictor;
    if (!predictor.loadLibrary(library_path)) {
        std::cerr << "load library failed: " << predictor.lastError() << '\n';
        return 1;
    }
    if (!predictor.loadModel(model_path)) {
        std::cerr << "load model failed: " << predictor.lastError() << '\n';
        return 1;
    }

    // * Same values as export_model.py so the two outputs can be compared.
    std::vector<double> input(predictor.inputSize(), 0.0);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = 1.0 + 0.1 * static_cast<double>(index + 1);
    }
    std::vector<double> output(predictor.outputSize(), 0.0);
    if (!predictor.predict(input.data(), output.data())) {
        std::cerr << "prediction failed: " << predictor.lastError() << '\n';
        return 1;
    }

    std::cout << "input size: " << predictor.inputSize() << '\n';
    std::cout << "output size: " << predictor.outputSize() << '\n';
    std::cout << "input:";
    for (const double value : input) std::cout << ' ' << value;
    std::cout << '\n';
    std::cout << "output:";
    for (const double value : output) std::cout << ' ' << value;
    std::cout << '\n';
    return 0;
}
