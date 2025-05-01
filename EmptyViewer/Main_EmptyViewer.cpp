// Main_EmptyViewer.cpp
// Rasterizer main application based on the EmptyViewer template.

#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <limits>
#include <iostream>
#include <algorithm> 


#include "sphere_scene.h"

const int screenWidth = 512;
const int screenHeight = 512;

std::vector<unsigned char> frameBuffer(screenWidth* screenHeight * 3); // RGB 
std::vector<float> depthBuffer(screenWidth* screenHeight);


float edgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}


float interpolateDepth(const glm::vec3& lambda, const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip) {
    // Interpolate 1/w
    float inv_w0 = 1.0f / v0_clip.w;
    float inv_w1 = 1.0f / v1_clip.w;
    float inv_w2 = 1.0f / v2_clip.w;
    float interpolated_inv_w = lambda.x * inv_w0 + lambda.y * inv_w1 + lambda.z * inv_w2;

   
    if (std::abs(interpolated_inv_w) < std::numeric_limits<float>::epsilon()) {
        return std::numeric_limits<float>::max(); // Or some other indicator of invalid depth
    }

    // Interpolate z/w
    float z_over_w0 = v0_clip.z * inv_w0;
    float z_over_w1 = v1_clip.z * inv_w1;
    float z_over_w2 = v2_clip.z * inv_w2;
    float interpolated_z_over_w = lambda.x * z_over_w0 + lambda.y * z_over_w1 + lambda.z * z_over_w2;

    // Calculate interpolated z_ndc
    float interpolated_z_ndc = interpolated_z_over_w / interpolated_inv_w;
    return interpolated_z_ndc;
}


// Rasterizes a single triangle
void rasterizeTriangle(const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip) {

    
    if (v0_clip.w <= 0 || v1_clip.w <= 0 || v2_clip.w <= 0) {
        
        return;
    }
    glm::vec3 v0_ndc = glm::vec3(v0_clip) / v0_clip.w;
    glm::vec3 v1_ndc = glm::vec3(v1_clip) / v1_clip.w;
    glm::vec3 v2_ndc = glm::vec3(v2_clip) / v2_clip.w;

   
    glm::vec2 v0_screen = glm::vec2((v0_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v0_ndc.y) * 0.5f * screenHeight); // Flip Y
    glm::vec2 v1_screen = glm::vec2((v1_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v1_ndc.y) * 0.5f * screenHeight);
    glm::vec2 v2_screen = glm::vec2((v2_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v2_ndc.y) * 0.5f * screenHeight);

    
    int minX = static_cast<int>(std::max(0.0f, std::min({ v0_screen.x, v1_screen.x, v2_screen.x })));
    int maxX = static_cast<int>(std::min(static_cast<float>(screenWidth - 1), std::ceil(std::max({ v0_screen.x, v1_screen.x, v2_screen.x }))));
    int minY = static_cast<int>(std::max(0.0f, std::min({ v0_screen.y, v1_screen.y, v2_screen.y })));
    int maxY = static_cast<int>(std::min(static_cast<float>(screenHeight - 1), std::ceil(std::max({ v0_screen.y, v1_screen.y, v2_screen.y }))));

    // Triangle area for barycentric coordinate calculation
    float area = edgeFunction(v0_screen, v1_screen, v2_screen);

   
    if (std::abs(area) < std::numeric_limits<float>::epsilon()) {
        return;
    }

    // --- Pixel Loop ---
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            glm::vec2 p = { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f }; // Pixel center

            
            float w0 = edgeFunction(v1_screen, v2_screen, p);
            float w1 = edgeFunction(v2_screen, v0_screen, p);
            float w2 = edgeFunction(v0_screen, v1_screen, p);

            
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                
                glm::vec3 lambda = glm::vec3(w0 / area, w1 / area, w2 / area);

                
                float z_ndc = interpolateDepth(lambda, v0_clip, v1_clip, v2_clip);

           
                float z_screen = (z_ndc + 1.0f) * 0.5f;

                // --- Depth Test ---
                int index = y * screenWidth + x;
                if (z_screen < depthBuffer[index]) { // Compare with buffer
                    depthBuffer[index] = z_screen; // Update depth buffer

                    
                    frameBuffer[index * 3 + 0] = 255; // R
                    frameBuffer[index * 3 + 1] = 255; // G
                    frameBuffer[index * 3 + 2] = 255; // B
                }
            }
        }
    }
}


// --- Main Function ---
int main() {
    // --- GLFW Initialization ---
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    
    // --- Create Window ---
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Software Rasterizer - HW5", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // --- GLEW Initialization ---
    glewExperimental = GL_TRUE; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // --- Load Scene Geometry ---
    create_scene();
    if (!gVertexBuffer || !gIndexBuffer) {
        std::cerr << "Failed to create scene geometry" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    std::cout << "Scene created: " << gNumVertices << " vertices, " << gNumTriangles << " triangles." << std::endl;


    
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0);
    std::fill(depthBuffer.begin(), depthBuffer.end(), std::numeric_limits<float>::max()); 

 
    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -7.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));

   
    glm::mat4 viewMatrix = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),  // Eye position
        glm::vec3(0.0f, 0.0f, -1.0f), // Look-at point (along -w)
        glm::vec3(0.0f, 1.0f, 0.0f)); // Up vector (v)
    // This results in an identity matrix for the given parameters.


// glm::frustum expects positive near and far distances.
    float nearVal = 0.1f; // Distance |-0.1|
    float farVal = 1000.0f; // Distance |-1000|
    glm::mat4 projectionMatrix = glm::frustum(-0.1f, 0.1f, -0.1f, 0.1f, nearVal, farVal);

    // Combined MVP Matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;


    
    std::cout << "Rasterizing..." << std::endl;
    for (int i = 0; i < gNumTriangles; ++i) {
        // Get vertex indices for the triangle
        int k0 = gIndexBuffer[3 * i + 0];
        int k1 = gIndexBuffer[3 * i + 1];
        int k2 = gIndexBuffer[3 * i + 2];

        // Get model-space vertex positions
        glm::vec3 v0_model = gVertexBuffer[k0];
        glm::vec3 v1_model = gVertexBuffer[k1];
        glm::vec3 v2_model = gVertexBuffer[k2];

        // Transform vertices to Clip Space
        glm::vec4 v0_clip = mvpMatrix * glm::vec4(v0_model, 1.0f);
        glm::vec4 v1_clip = mvpMatrix * glm::vec4(v1_model, 1.0f);
        glm::vec4 v2_clip = mvpMatrix * glm::vec4(v2_model, 1.0f);

        // Rasterize the triangle (includes perspective divide, viewport transform, depth test)
        rasterizeTriangle(v0_clip, v1_clip, v2_clip);
    }
    std::cout << "Rasterization complete." << std::endl;

    // --- Main Render Loop ---
    while (!glfwWindowShouldClose(window)) {
        // --- Input Handling (Optional) ---
        // processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark grey background
        glClear(GL_COLOR_BUFFER_BIT);

       
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Ensure byte alignment
        glDrawPixels(screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer.data());

        // --- Swap Buffers and Poll Events ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
