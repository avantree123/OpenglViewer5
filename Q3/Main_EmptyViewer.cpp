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

const int screenWidth = 512;
const int screenHeight = 512;

std::vector<unsigned char> frameBuffer(screenWidth* screenHeight * 3);
std::vector<float> depthBuffer(screenWidth* screenHeight);


const glm::vec3 mat_ka = glm::vec3(0.0f, 1.0f, 0.0f);
const glm::vec3 mat_kd = glm::vec3(0.0f, 0.5f, 0.0f);
const glm::vec3 mat_ks = glm::vec3(0.5f, 0.5f, 0.5f);
const float mat_p_shininess = 32.0f;

const float light_Ia_intensity = 0.2f;
const glm::vec3 light_pos_world = glm::vec3(-4.0f, 4.0f, -3.0f);
const glm::vec3 light_Il_intensity = glm::vec3(1.0f, 1.0f, 1.0f); // White light

const glm::vec3 eye_pos_world = glm::vec3(0.0f, 0.0f, 0.0f);
const float gamma_val = 2.2f;
// --- 전역 변수로 선언된 모델 행렬 및 구 중심 ---
glm::mat4 g_modelMatrix;
glm::vec3 g_sphere_center_world;


float edgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

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


glm::vec3 calculate_phong_pixel_color(const glm::vec3& pixel_world_pos, const glm::vec3& pixel_world_normal_normalized) {
    // Ambient
    glm::vec3 ambient_color = light_Ia_intensity * mat_ka;

    // Diffuse
    glm::vec3 light_dir = glm::normalize(light_pos_world - pixel_world_pos);
    float diff_factor = std::max(0.0f, glm::dot(pixel_world_normal_normalized, light_dir));
    glm::vec3 diffuse_color = light_Il_intensity * mat_kd * diff_factor;

    // Specular
    glm::vec3 view_dir = glm::normalize(eye_pos_world - pixel_world_pos);
    glm::vec3 reflect_dir = glm::reflect(-light_dir, pixel_world_normal_normalized);
   
    float spec_factor = std::pow(std::max(0.0f, glm::dot(view_dir, reflect_dir)), mat_p_shininess);
    glm::vec3 specular_color = light_Il_intensity * mat_ks * spec_factor;

    glm::vec3 final_color_linear = ambient_color + diffuse_color + specular_color;
    final_color_linear = glm::clamp(final_color_linear, 0.0f, 1.0f);

    // Gamma Correction
    glm::vec3 final_color_gamma_corrected = glm::vec3(
        std::pow(final_color_linear.r, 1.0f / gamma_val),
        std::pow(final_color_linear.g, 1.0f / gamma_val),
        std::pow(final_color_linear.b, 1.0f / gamma_val)
    );
    return glm::clamp(final_color_gamma_corrected, 0.0f, 1.0f);
}


void rasterizeTriangle(
    const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip,
    const glm::vec3& v0_world, const glm::vec3& v1_world, const glm::vec3& v2_world,
    const glm::vec3& n0_world_norm, const glm::vec3& n1_world_norm, const glm::vec3& n2_world_norm,
    bool print_debug = false) {

    float epsilon_w = 1e-5f;
    if (v0_clip.w < epsilon_w && v1_clip.w < epsilon_w && v2_clip.w < epsilon_w) {
        // if (print_debug) std::cout << "Triangle culled: all w < epsilon" << std::endl;
        return;
    }

    glm::vec3 v0_ndc, v1_ndc, v2_ndc;
    if (std::abs(v0_clip.w) < epsilon_w || std::abs(v1_clip.w) < epsilon_w || std::abs(v2_clip.w) < epsilon_w) {
     
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
        // if (print_debug) std::cout << "Triangle culled: zero area" << std::endl;
        return;
    }

    bool first_pixel_debug_printed = !print_debug;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            glm::vec2 p = { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };

            float w0_edge = edgeFunction(v1_screen, v2_screen, p);
            float w1_edge = edgeFunction(v2_screen, v0_screen, p);
            float w2_edge = edgeFunction(v0_screen, v1_screen, p);

            if (w0_edge >= 0 && w1_edge >= 0 && w2_edge >= 0) {
                glm::vec3 lambda = glm::vec3(w0_edge / area, w1_edge / area, w2_edge / area);
                float z_ndc_interpolated = interpolateDepth(lambda, v0_clip, v1_clip, v2_clip);

                if (!first_pixel_debug_printed) {
                    std::cout << "  Inside rasterizeTriangle (Phong, Tri0, Pixel0): z_ndc_interpolated = " << z_ndc_interpolated << std::endl;
             
                }

                if (z_ndc_interpolated < -1.0f - 1e-5f || z_ndc_interpolated > 1.0f + 1e-5f) {
                    continue;
                }

                float z_screen = (z_ndc_interpolated + 1.0f) * 0.5f;

                int index = y * screenWidth + x;
                if (z_screen < depthBuffer[index]) {
                    depthBuffer[index] = z_screen;

                    // Perspective-correct interpolation for world position and normal
                    float inv_w0_clip = 1.0f / v0_clip.w;
                    float inv_w1_clip = 1.0f / v1_clip.w;
                    float inv_w2_clip = 1.0f / v2_clip.w;

                    float interpolated_inv_w_clip = lambda.x * inv_w0_clip + lambda.y * inv_w1_clip + lambda.z * inv_w2_clip;
                    if (std::abs(interpolated_inv_w_clip) < std::numeric_limits<float>::epsilon()) continue;


                    // Interpolate World Position (P_world / w_clip)
                    glm::vec3 world_pos_over_w = lambda.x * (v0_world * inv_w0_clip) +
                        lambda.y * (v1_world * inv_w1_clip) +
                        lambda.z * (v2_world * inv_w2_clip);
                    glm::vec3 pixel_world_pos = world_pos_over_w / interpolated_inv_w_clip;

                    // Interpolate World Normal (N_world / w_clip)
                    glm::vec3 world_normal_over_w = lambda.x * (n0_world_norm * inv_w0_clip) +
                        lambda.y * (n1_world_norm * inv_w1_clip) +
                        lambda.z * (n2_world_norm * inv_w2_clip);
                    glm::vec3 pixel_world_normal_unnormalized = world_normal_over_w / interpolated_inv_w_clip;
                    glm::vec3 pixel_world_normal_normalized = glm::normalize(pixel_world_normal_unnormalized);


                    // Calculate pixel color using Phong shading
                    glm::vec3 pixel_color = calculate_phong_pixel_color(pixel_world_pos, pixel_world_normal_normalized);

                    frameBuffer[index * 3 + 0] = static_cast<unsigned char>(pixel_color.r * 255.0f);
                    frameBuffer[index * 3 + 1] = static_cast<unsigned char>(pixel_color.g * 255.0f);
                    frameBuffer[index * 3 + 2] = static_cast<unsigned char>(pixel_color.b * 255.0f);

                    if (!first_pixel_debug_printed && print_debug) { 
                        std::cout << "    Pixel(" << x << "," << y << "): world_pos(" << pixel_world_pos.x << "," << pixel_world_pos.y << "," << pixel_world_pos.z << ")" << std::endl;
                        std::cout << "    Pixel(" << x << "," << y << "): world_normal(" << pixel_world_normal_normalized.x << "," << pixel_world_normal_normalized.y << "," << pixel_world_normal_normalized.z << ")" << std::endl;
                        std::cout << "    Pixel(" << x << "," << y << "): color(" << pixel_color.r << "," << pixel_color.g << "," << pixel_color.b << ")" << std::endl;
                        first_pixel_debug_printed = true;
                    }
                }
            }
        }
    }
}


int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Software Rasterizer - Q3 Phong", NULL, NULL);
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

    g_modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -7.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
    g_sphere_center_world = glm::vec3(g_modelMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));


    glm::mat4 viewMatrix = glm::lookAt(eye_pos_world,
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    float nearVal = 0.1f;
    float farVal = 100.0f;
    glm::mat4 projectionMatrix = glm::frustum(-0.1f, 0.1f, -0.1f, 0.1f, nearVal, farVal);

    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * g_modelMatrix;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(g_modelMatrix))); // For transforming normals (if model normals were used)


    std::cout << "Rasterizing with Phong Shading..." << std::endl;
    bool first_triangle_main_debug_printed = false;

    for (int i = 0; i < gNumTriangles; ++i) {
        int k0 = gIndexBuffer[3 * i + 0];
        int k1 = gIndexBuffer[3 * i + 1];
        int k2 = gIndexBuffer[3 * i + 2];

        glm::vec3 v0_model = gVertexBuffer[k0];
        glm::vec3 v1_model = gVertexBuffer[k1];
        glm::vec3 v2_model = gVertexBuffer[k2];

        // Calculate World Positions
        glm::vec3 v0_world = glm::vec3(g_modelMatrix * glm::vec4(v0_model, 1.0f));
        glm::vec3 v1_world = glm::vec3(g_modelMatrix * glm::vec4(v1_model, 1.0f));
        glm::vec3 v2_world = glm::vec3(g_modelMatrix * glm::vec4(v2_model, 1.0f));

       
        glm::vec3 n0_world_norm = glm::normalize(v0_world - g_sphere_center_world);
        glm::vec3 n1_world_norm = glm::normalize(v1_world - g_sphere_center_world);
        glm::vec3 n2_world_norm = glm::normalize(v2_world - g_sphere_center_world);
       

        glm::vec4 v0_clip = mvpMatrix * glm::vec4(v0_model, 1.0f);
        glm::vec4 v1_clip = mvpMatrix * glm::vec4(v1_model, 1.0f);
        glm::vec4 v2_clip = mvpMatrix * glm::vec4(v2_model, 1.0f);

        bool current_triangle_print_debug = false;
        if (!first_triangle_main_debug_printed) {
            std::cout << "Triangle " << i << " Clip Coords (x,y,z,w):" << std::endl;
            std::cout << "  V0_clip: (" << v0_clip.x << ", " << v0_clip.y << ", " << v0_clip.z << ", " << v0_clip.w << ")" << std::endl;
       
            current_triangle_print_debug = true;
            first_triangle_main_debug_printed = true;
        }

        rasterizeTriangle(
            v0_clip, v1_clip, v2_clip,
            v0_world, v1_world, v2_world,
            n0_world_norm, n1_world_norm, n2_world_norm,
            current_triangle_print_debug
        );
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