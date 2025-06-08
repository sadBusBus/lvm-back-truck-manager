#include <jni.h>
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <filesystem> // Para obtener rutas de carpetas actuales

using namespace cv;

namespace fs = std::filesystem;

// Thread-local storage for per-request data
thread_local struct RequestData {
    Mat processedImage;
    std::string plateNumber;
    bool isClean;
    float confidence;
    bool processed;
} g_requestData;

// Helper function to reset data
void resetRequestData() {
    g_requestData.processed = false;
    g_requestData.plateNumber = "";
    g_requestData.isClean = true;
    g_requestData.confidence = 0.0f;
}

// Helper to save the processed image
void saveProcessedImage(const Mat& image) {
    try {
        // Obtener el directorio actual de ejecución
        std::string currentPath = fs::current_path().string();
        std::string outputPath = currentPath + "/processed_image.png";

        // Guardar la imagen procesada
        bool success = imwrite(outputPath, image);

        if (success) {
            printf("✅ Processed image saved at: %s\n", outputPath.c_str());
        } else {
            printf("❌ Failed to save processed image.\n");
        }
    } catch (const std::exception &e) {
        printf("❌ Exception saving processed image: %s\n", e.what());
    }
}

// Process the image
bool processImage(JNIEnv *env, jbyteArray input) {
    // Reset the request-specific state
    resetRequestData();

    printf("🚀 Starting image processing...\n");

    if (!input) {
        printf("❌ Input is null.\n");
        return false;
    }

    jsize dataLength = env->GetArrayLength(input);
    printf("📊 Input size: %d bytes\n", dataLength);

    if (dataLength == 0) {
        printf("❌ Input data length is empty.\n");
        return false;
    }

    // Load input as byte array
    jbyte *fileData = env->GetByteArrayElements(input, nullptr);
    if (!fileData) {
        printf("❌ Failed to retrieve byte array data.\n");
        return false;
    }

    std::vector<uchar> buffer((uchar*)fileData, (uchar*)fileData + dataLength);
    env->ReleaseByteArrayElements(input, fileData, 0);

    // Decode the image
    Mat image = imdecode(buffer, IMREAD_COLOR);
    if (image.empty()) {
        printf("❌ Failed to decode the image.\n");
        return false;
    }

    printf("✅ Image decoded successfully: %dx%d\n", image.cols, image.rows);

    Mat gray;
    cvtColor(image, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, gray, Size(5, 5), 0);
    threshold(gray, gray, 0, 255, THRESH_BINARY + THRESH_OTSU);

    g_requestData.processedImage = gray.clone();

    // Save the processed image
    saveProcessedImage(g_requestData.processedImage);

    // Optional OCR with Tesseract
    try {
        tesseract::TessBaseAPI api;
        if (api.Init(NULL, "eng") == 0) {
            api.SetImage(gray.data, gray.cols, gray.rows, 1, gray.step);

            char* ocrResult = api.GetUTF8Text();
            if (ocrResult) {
                g_requestData.plateNumber = std::string(ocrResult);
                g_requestData.plateNumber.erase(
                    std::remove_if(g_requestData.plateNumber.begin(), g_requestData.plateNumber.end(), ::isspace),
                    g_requestData.plateNumber.end()
                );
                printf("✅ OCR detected plate: %s\n", g_requestData.plateNumber.c_str());
                delete[] ocrResult;
            } else {
                printf("❌ No text detected by OCR.\n");
                g_requestData.plateNumber = "NO_TEXT_DETECTED";
            }
        } else {
            printf("❌ Failed to initialize Tesseract.\n");
        }
    } catch (...) {
        printf("❌ Unexpected OCR processing error.\n");
        return false;
    }

    g_requestData.processed = true;
    return true;
}

// Native methods
extern "C" JNIEXPORT jstring JNICALL
Java_com_lvm_back_truck_manager_controller_PlateInspectionController_extractPlateNumber(
    JNIEnv *env, jobject, jbyteArray input) {

    printf("🔠 Extract plate number invoked.\n");
    if (!processImage(env, input)) {
        return env->NewStringUTF("PROCESSING_ERROR");
    }
    return env->NewStringUTF(g_requestData.plateNumber.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_lvm_back_truck_manager_controller_PlateInspectionController_isPlateClean(
    JNIEnv *env, jobject, jbyteArray input) {

    printf("🧹 Detect plate cleanliness.\n");
    if (!g_requestData.processed && !processImage(env, input)) {
        return JNI_FALSE;
    }
    return g_requestData.isClean ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_lvm_back_truck_manager_controller_PlateInspectionController_getConfidence(
    JNIEnv *env, jobject, jbyteArray input) {

    printf("📊 Get OCR confidence.\n");
    if (!g_requestData.processed && !processImage(env, input)) {
        return 0.0f;
    }
    return g_requestData.confidence;
}