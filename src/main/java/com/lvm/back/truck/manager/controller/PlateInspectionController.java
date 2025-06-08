
package com.lvm.back.truck.manager.controller;

import io.micronaut.http.HttpResponse;
import io.micronaut.http.MediaType;
import io.micronaut.http.annotation.Controller;
import io.micronaut.http.annotation.Post;
import io.micronaut.http.multipart.CompletedFileUpload;
import jakarta.inject.Singleton;

import java.io.IOException;
import java.util.Set;

@Singleton
@Controller("/inspect-plate")
public class PlateInspectionController {

    private static final Set<String> ALLOWED_IMAGE_TYPES = Set.of(
            "image/jpeg",
            "image/png",
            "image/gif",
            "image/webp"
    );

    private static boolean libraryLoaded = false;

    static {
        try {
            System.loadLibrary("PlateProcessor");
            libraryLoaded = true;
            System.out.println("✅ PlateProcessor library loaded");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("❌ Failed to load PlateProcessor: " + e.getMessage());
        }
    }

    @Post(consumes = MediaType.MULTIPART_FORM_DATA, produces = MediaType.APPLICATION_JSON)
    public HttpResponse<InspectionResponse> inspectPlate(CompletedFileUpload file) {
        System.out.println("🚀 Request received");

        try {
            if (!libraryLoaded) {
                return HttpResponse.serverError(new InspectionResponse(false, 0.0f, "", "Native library not loaded"));
            }

            if (file == null || file.getSize() == 0) {
                return HttpResponse.badRequest(new InspectionResponse(false, 0.0f, "", "No file provided or file is empty"));
            }

            String contentType = file.getContentType().map(MediaType::toString).orElse("");
            boolean isImage = ALLOWED_IMAGE_TYPES.contains(contentType);

            if (!isImage) {
                return HttpResponse.badRequest(new InspectionResponse(false, 0.0f, "", "File must be an image"));
            }

            byte[] fileBytes = file.getBytes();
            if (fileBytes.length == 0) {
                return HttpResponse.badRequest(new InspectionResponse(false, 0.0f, "", "Empty file data"));
            }

            System.out.println("📁 Processing " + fileBytes.length + " bytes");

            // Use separate native methods for each value
            try {
                String plateNumber = extractPlateNumber(fileBytes);
                boolean isClean = isPlateClean(fileBytes);
                float confidence = getConfidence(fileBytes);

                System.out.println("✅ Success: clean=" + isClean + ", confidence=" + confidence + ", plate=" + plateNumber);
                return HttpResponse.ok(new InspectionResponse(isClean, confidence, plateNumber, null));

            } catch (UnsatisfiedLinkError e) {
                System.err.println("❌ UnsatisfiedLinkError: " + e.getMessage());
                return HttpResponse.serverError(new InspectionResponse(false, 0.0f, "", "Native method signature mismatch: " + e.getMessage()));
            }

        } catch (IOException e) {
            System.err.println("❌ IOException: " + e.getMessage());
            return HttpResponse.serverError(new InspectionResponse(false, 0.0f, "", "Error reading file: " + e.getMessage()));
        } catch (Exception e) {
            System.err.println("❌ Exception: " + e.getMessage());
            e.printStackTrace();
            return HttpResponse.serverError(new InspectionResponse(false, 0.0f, "", "Unexpected error: " + e.getMessage()));
        }
    }

    // Simple native methods with primitive types
    private native String extractPlateNumber(byte[] imageData);
    private native boolean isPlateClean(byte[] imageData);
    private native float getConfidence(byte[] imageData);

    public static class InspectionResponse {
        private final boolean isClean;
        private final float confidence;
        private final String plateNumber;
        private final String error;

        public InspectionResponse(boolean isClean, float confidence, String plateNumber, String error) {
            this.isClean = isClean;
            this.confidence = confidence;
            this.plateNumber = plateNumber;
            this.error = error;
        }

        public boolean isIsClean() { return isClean; }
        public float getConfidence() { return confidence; }
        public String getPlateNumber() { return plateNumber; }
        public String getError() { return error; }
    }
}