#include <jni.h>
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <filesystem>

using namespace cv;
namespace fs = std::filesystem;

thread_local struct RequestData {
    Mat processedImage;
    std::string plateNumber;
    bool isClean;
    float confidence;
    bool processed;
} g_requestData;

void resetRequestData() {
    g_requestData.processed = false;
    g_requestData.plateNumber = "";
    g_requestData.isClean = true;
    g_requestData.confidence = 0.0f;
}

void saveProcessedImage(const Mat& image) {
    try {
        std::string currentPath = fs::current_path().string();
        std::string outputPath = currentPath + "/processed_image.png";
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

bool processImage(JNIEnv *env, jbyteArray input) {
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

    jbyte *fileData = env->GetByteArrayElements(input, nullptr);
    if (!fileData) {
        printf("❌ Failed to retrieve byte array data.\n");
        return false;
    }

    std::vector<uchar> buffer((uchar*)fileData, (uchar*)fileData + dataLength);
    env->ReleaseByteArrayElements(input, fileData, 0);

    Mat image = imdecode(buffer, IMREAD_COLOR);
    if (image.empty()) {
        printf("❌ Failed to decode the image.\n");
        return false;
    }

    printf("✅ Image decoded successfully: %dx%d\n", image.cols, image.rows);

    // PREPROCESADO MEJORADO PARA OCR DE MATRÍCULA
    Mat gray;
    cvtColor(image, gray, COLOR_BGR2GRAY);

    // Puedes quitar/commentar GaussianBlur si la imagen no tiene demasiado ruido
    // GaussianBlur(gray, gray, Size(3, 3), 0);

    // Umbral adaptativo para mejorar contraste y nitidez de caracteres
    Mat bin;
    adaptiveThreshold(gray, bin, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 31, 15);

    // Invierte para dejar letras en negro sobre fondo blanco si es necesario (ajusta según tu país)
    bitwise_not(bin, bin);

    if (bin.rows < 30 || bin.cols < 100) {
        resize(bin, bin, Size(300, 60));
    }

    g_requestData.processedImage = bin.clone();
    saveProcessedImage(g_requestData.processedImage);

    try {
        tesseract::TessBaseAPI api;
        if (api.Init(NULL, "eng") == 0) {
            api.SetPageSegMode(tesseract::PSM_SINGLE_LINE);
            api.SetVariable("tessedit_char_whitelist", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-");
            api.SetImage(bin.data, bin.cols, bin.rows, 1, bin.step);

            char* ocrResult = api.GetUTF8Text();
            if (ocrResult) {
                g_requestData.plateNumber = std::string(ocrResult);
                g_requestData.plateNumber.erase(
                    std::remove_if(g_requestData.plateNumber.begin(), g_requestData.plateNumber.end(), [](unsigned char c) {
                        return !isalnum(c) && c != '-'; // Eliminar caracteres no válidos
                    }),
                    g_requestData.plateNumber.end()
                );
                g_requestData.confidence = static_cast<float>(api.MeanTextConf()) / 100.0f;
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

extern "C" JNIEXPORT jstring JNICALL
Java_com_lvm_back_truck_manager_controller_PlateInspectionController_extractPlateNumber(JNIEnv *env, jobject, jbyteArray input) {
    printf("🔠 Extract plate number invoked.\n");
    if (!processImage(env, input)) {
        return env->NewStringUTF("PROCESSING_ERROR");
    }
    return env->NewStringUTF(g_requestData.plateNumber.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_lvm_back_truck_manager_controller_PlateInspectionController_isPlateClean(JNIEnv *env, jobject, jbyteArray input) {
    printf("🧹 Detect plate cleanliness.\n");
    if (!g_requestData.processed && !processImage(env, input)) {
        return JNI_FALSE;
    }
    return g_requestData.isClean ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_lvm_back_truck_manager_controller_PlateInspectionController_getConfidence(JNIEnv *env, jobject, jbyteArray input) {
    printf("📊 Get OCR confidence.\n");
    if (!g_requestData.processed && !processImage(env, input)) {
        return 0.0f;
    }
    return g_requestData.confidence;
}