// Main_EmptyViewer.cpp

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <limits>
#include <iostream>
#include <algorithm>
#include <cmath>

#include "sphere_scene.h"
#include "../Q1/sphere_scene.h"

const int screenWidth = 512;
const int screenHeight = 512;

std::vector<unsigned char> frameBuffer(screenWidth* screenHeight * 3);
std::vector<float> depthBuffer(screenWidth* screenHeight);

float edgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

// 수정된 interpolateDepth 함수
float interpolateDepth(const glm::vec3& lambda, const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip) {
    float epsilon_w = 1e-6f;
    if (std::abs(v0_clip.w) < epsilon_w || std::abs(v1_clip.w) < epsilon_w || std::abs(v2_clip.w) < epsilon_w) {
        return std::numeric_limits<float>::max();
    }

    float inv_w0 = 1.0f / v0_clip.w;
    float inv_w1 = 1.0f / v1_clip.w;
    float inv_w2 = 1.0f / v2_clip.w;

    float interpolated_inv_w = lambda.x * inv_w0 + lambda.y * inv_w1 + lambda.z * inv_w2;

    if (std::abs(interpolated_inv_w) < std::numeric_limits<float>::epsilon()) {
        return std::numeric_limits<float>::max();
    }

    float z_ndc0 = v0_clip.z * inv_w0;
    float z_ndc1 = v1_clip.z * inv_w1;
    float z_ndc2 = v2_clip.z * inv_w2;

    float interpolated_z_ndc_over_w = lambda.x * z_ndc0 * inv_w0 +
        lambda.y * z_ndc1 * inv_w1 +
        lambda.z * z_ndc2 * inv_w2;

    float final_interpolated_z_ndc = interpolated_z_ndc_over_w / interpolated_inv_w;

    return final_interpolated_z_ndc;
}


void rasterizeTriangle(const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip,
    const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2, bool print_debug = false) {

    float epsilon_w = 1e-5f;
    if (v0_clip.w < epsilon_w && v1_clip.w < epsilon_w && v2_clip.w < epsilon_w) {
        if (print_debug) std::cout << "Triangle culled: all w < epsilon" << std::endl;
        return;
    }

    glm::vec3 v0_ndc, v1_ndc, v2_ndc;
    if (std::abs(v0_clip.w) < epsilon_w || std::abs(v1_clip.w) < epsilon_w || std::abs(v2_clip.w) < epsilon_w) {
        if (print_debug) std::cout << "Warning: Extremely small w for a vertex before division. Skipping triangle." << std::endl;
        return;
    }

    v0_ndc = glm::vec3(v0_clip) / v0_clip.w;
    v1_ndc = glm::vec3(v1_clip) / v1_clip.w;
    v2_ndc = glm::vec3(v2_clip) / v2_clip.w;

    glm::vec2 v0_screen = glm::vec2((v0_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v0_ndc.y) * 0.5f * screenHeight);
    glm::vec2 v1_screen = glm::vec2((v1_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v1_ndc.y) * 0.5f * screenHeight);
    glm::vec2 v2_screen = glm::vec2((v2_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v2_ndc.y) * 0.5f * screenHeight);

    int minX = static_cast<int>(std::max(0.0f, std::min({ v0_screen.x, v1_screen.x, v2_screen.x })));
    int maxX = static_cast<int>(std::min(static_cast<float>(screenWidth - 1), std::ceil(std::max({ v0_screen.x, v1_screen.x, v2_screen.x }))));
    int minY = static_cast<int>(std::max(0.0f, std::min({ v0_screen.y, v1_screen.y, v2_screen.y })));
    int maxY = static_cast<int>(std::min(static_cast<float>(screenHeight - 1), std::ceil(std::max({ v0_screen.y, v1_screen.y, v2_screen.y }))));

    float area = edgeFunction(v0_screen, v1_screen, v2_screen);

    if (std::abs(area) < std::numeric_limits<float>::epsilon()) {
        if (print_debug) std::cout << "Triangle culled: zero area" << std::endl;
        return;
    }

    bool first_pixel_printed = !print_debug;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            glm::vec2 p = { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };

            float w0_edge = edgeFunction(v1_screen, v2_screen, p);
            float w1_edge = edgeFunction(v2_screen, v0_screen, p);
            float w2_edge = edgeFunction(v0_screen, v1_screen, p);

            if (w0_edge >= 0 && w1_edge >= 0 && w2_edge >= 0) {
                glm::vec3 lambda = glm::vec3(w0_edge / area, w1_edge / area, w2_edge / area);
                float z_ndc_interpolated = interpolateDepth(lambda, v0_clip, v1_clip, v2_clip);

                if (!first_pixel_printed) {
                    std::cout << "  Inside rasterizeTriangle (Tri0, Pixel0): z_ndc_interpolated = " << z_ndc_interpolated << std::endl;
                    std::cout << "    v0_clip.w=" << v0_clip.w << ", v1_clip.w=" << v1_clip.w << ", v2_clip.w=" << v2_clip.w << std::endl;
                    std::cout << "    lambda=(" << lambda.x << "," << lambda.y << "," << lambda.z << ")" << std::endl;
                    first_pixel_printed = true;
                }

                if (z_ndc_interpolated < -1.0f - 1e-5f || z_ndc_interpolated > 1.0f + 1e-5f) {
                    continue;
                }

                float z_screen = (z_ndc_interpolated + 1.0f) * 0.5f;

                int index = y * screenWidth + x;
                if (z_screen < depthBuffer[index]) {
                    depthBuffer[index] = z_screen;

                    if (std::abs(v0_clip.w) < epsilon_w || std::abs(v1_clip.w) < epsilon_w || std::abs(v2_clip.w) < epsilon_w) continue;

                    float inv_w0_val = 1.0f / v0_clip.w;
                    float inv_w1_val = 1.0f / v1_clip.w;
                    float inv_w2_val = 1.0f / v2_clip.w;

                    glm::vec3 color_over_w = lambda.x * (c0 * inv_w0_val) +
                        lambda.y * (c1 * inv_w1_val) +
                        lambda.z * (c2 * inv_w2_val);

                    float current_interpolated_inv_w = lambda.x * inv_w0_val + lambda.y * inv_w1_val + lambda.z * inv_w2_val;

                    if (std::abs(current_interpolated_inv_w) < std::numeric_limits<float>::epsilon()) continue;

                    glm::vec3 interpolated_color = color_over_w / current_interpolated_inv_w;
                    interpolated_color = glm::clamp(interpolated_color, 0.0f, 1.0f);

                    frameBuffer[index * 3 + 0] = static_cast<unsigned char>(interpolated_color.r * 255.0f);
                    frameBuffer[index * 3 + 1] = static_cast<unsigned char>(interpolated_color.g * 255.0f);
                    frameBuffer[index * 3 + 2] = static_cast<unsigned char>(interpolated_color.b * 255.0f);
                }
            }
        }
    }
}

glm::vec3 calculate_vertex_color(const glm::vec3& vertex_model, const glm::mat4& modelMatrix, const glm::vec3& sphere_center_world) {
    glm::vec3 ka = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 kd = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::vec3 ks = glm::vec3(0.5f, 0.5f, 0.5f);
    float p_shininess = 32.0f;
    float Ia_intensity = 0.2f;
    glm::vec3 light_pos_world = glm::vec3(-4.0f, 4.0f, -3.0f);
    glm::vec3 Il_intensity = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 eye_pos_world = glm::vec3(0.0f, 0.0f, 0.0f);
    float gamma_val = 2.2f;

    glm::vec4 vertex_world_h = modelMatrix * glm::vec4(vertex_model, 1.0f);
    if (std::abs(vertex_world_h.w) < 1e-5f) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    glm::vec3 vertex_world = glm::vec3(vertex_world_h) / vertex_world_h.w;

    glm::vec3 vertex_normal_world = glm::normalize(vertex_world - sphere_center_world);

    glm::vec3 ambient_color = Ia_intensity * ka;

    glm::vec3 light_dir = glm::normalize(light_pos_world - vertex_world);
    float diff_factor = std::max(0.0f, glm::dot(vertex_normal_world, light_dir));
    glm::vec3 diffuse_color = Il_intensity * kd * diff_factor;

    glm::vec3 view_dir = glm::normalize(eye_pos_world - vertex_world);
    glm::vec3 reflect_dir = glm::reflect(-light_dir, vertex_normal_world);
    float spec_factor = std::pow(std::max(0.0f, glm::dot(view_dir, reflect_dir)), p_shininess);
    glm::vec3 specular_color = Il_intensity * ks * spec_factor;

    glm::vec3 vertex_color_linear = ambient_color + diffuse_color + specular_color;
    vertex_color_linear = glm::clamp(vertex_color_linear, 0.0f, 1.0f);

    glm::vec3 vertex_color_gamma_corrected = glm::vec3(
        std::pow(vertex_color_linear.r, 1.0f / gamma_val),
        std::pow(vertex_color_linear.g, 1.0f / gamma_val),
        std::pow(vertex_color_linear.b, 1.0f / gamma_val)
    );
    return glm::clamp(vertex_color_gamma_corrected, 0.0f, 1.0f);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Software Rasterizer - Q2 Gouraud (Debug Attempt 3)", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

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

    glm::vec3 sphere_center_world = glm::vec3(modelMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    glm::mat4 viewMatrix = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    float nearVal = 0.1f;
    float farVal = 100.0f;
    glm::mat4 projectionMatrix = glm::frustum(-0.1f, 0.1f, -0.1f, 0.1f, nearVal, farVal);

    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;

    std::cout << "Rasterizing with Gouraud Shading..." << std::endl;
    bool first_triangle_debug_printed = false;

    for (int i = 0; i < gNumTriangles; ++i) {
        int k0 = gIndexBuffer[3 * i + 0];
        int k1 = gIndexBuffer[3 * i + 1];
        int k2 = gIndexBuffer[3 * i + 2];

        glm::vec3 v0_model = gVertexBuffer[k0];
        glm::vec3 v1_model = gVertexBuffer[k1];
        glm::vec3 v2_model = gVertexBuffer[k2];

        glm::vec3 c0 = calculate_vertex_color(v0_model, modelMatrix, sphere_center_world);
        glm::vec3 c1 = calculate_vertex_color(v1_model, modelMatrix, sphere_center_world);
        glm::vec3 c2 = calculate_vertex_color(v2_model, modelMatrix, sphere_center_world);

        glm::vec4 v0_clip = mvpMatrix * glm::vec4(v0_model, 1.0f);
        glm::vec4 v1_clip = mvpMatrix * glm::vec4(v1_model, 1.0f);
        glm::vec4 v2_clip = mvpMatrix * glm::vec4(v2_model, 1.0f);

        bool current_triangle_print_debug = false;
        if (!first_triangle_debug_printed) { // 첫 번째 삼각형에 대해서만 디버그 정보 출력
            std::cout << "Triangle " << i << " Vertex Colors (R,G,B):" << std::endl;
            std::cout << "  C0: (" << c0.r << ", " << c0.g << ", " << c0.b << ")" << std::endl;
            std::cout << "  C1: (" << c1.r << ", " << c1.g << ", " << c1.b << ")" << std::endl;
            std::cout << "  C2: (" << c2.r << ", " << c2.g << ", " << c2.b << ")" << std::endl;

            std::cout << "Triangle " << i << " Clip Coords (x,y,z,w):" << std::endl;
            std::cout << "  V0_clip: (" << v0_clip.x << ", " << v0_clip.y << ", " << v0_clip.z << ", " << v0_clip.w << ")" << std::endl;
            std::cout << "  V1_clip: (" << v1_clip.x << ", " << v1_clip.y << ", " << v1_clip.z << ", " << v1_clip.w << ")" << std::endl;
            std::cout << "  V2_clip: (" << v2_clip.x << ", " << v2_clip.y << ", " << v2_clip.z << ", " << v2_clip.w << ")" << std::endl;
            current_triangle_print_debug = true;
            first_triangle_debug_printed = true;
        }

        rasterizeTriangle(v0_clip, v1_clip, v2_clip, c0, c1, c2, current_triangle_print_debug);
    }
    std::cout << "Rasterization complete." << std::endl;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glDrawPixels(screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer.data());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}