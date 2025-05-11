#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

const glm::vec3 mat_ka(0.0f, 1.0f, 0.0f);
const glm::vec3 mat_kd(0.0f, 0.5f, 0.0f);
const glm::vec3 mat_ks(0.5f, 0.5f, 0.5f);
const float p_shininess = 32.0f;

const float ambient_light_intensity = 0.2f;
const glm::vec3 point_light_pos_world(-4.0f, 4.0f, -3.0f);
const glm::vec3 point_light_color(1.0f, 1.0f, 1.0f);

const glm::vec3 eye_pos_world(0.0f, 0.0f, 0.0f);
const float gamma_value = 2.2f;

float edgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

glm::vec3 calculateBlinnPhongColorAtPoint(
    const glm::vec3& point_world,
    const glm::vec3& normal_world,
    const glm::vec3& view_pos_world,
    const glm::vec3& material_ka,
    const glm::vec3& material_kd,
    const glm::vec3& material_ks,
    float material_p,
    const glm::vec3& light_pos_w,
    const glm::vec3& light_col,
    float global_ambient_intensity)
{
    glm::vec3 ambient = material_ka * global_ambient_intensity;
    glm::vec3 light_dir = glm::normalize(light_pos_w - point_world);
    float diff_intensity = std::max(glm::dot(normal_world, light_dir), 0.0f);
    glm::vec3 diffuse = material_kd * light_col * diff_intensity;

    glm::vec3 view_dir = glm::normalize(view_pos_world - point_world);
    glm::vec3 halfway_dir = glm::normalize(light_dir + view_dir);
    float spec_intensity = std::pow(std::max(glm::dot(normal_world, halfway_dir), 0.0f), material_p);
    glm::vec3 specular = material_ks * light_col * spec_intensity;

    glm::vec3 total_color = ambient + diffuse + specular;
    return total_color;
}

glm::vec3 applyGamma(const glm::vec3& color, float gamma) {
    float inv_gamma = 1.0f / gamma;
    glm::vec3 non_negative_color = glm::max(color, glm::vec3(0.0f));
    return glm::vec3(
        std::pow(non_negative_color.r, inv_gamma),
        std::pow(non_negative_color.g, inv_gamma),
        std::pow(non_negative_color.b, inv_gamma)
    );
}

float interpolateDepth(const glm::vec3& lambda, const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip) {
    float inv_w0 = 1.0f / v0_clip.w;
    float inv_w1 = 1.0f / v1_clip.w;
    float inv_w2 = 1.0f / v2_clip.w;
    float interpolated_inv_w = lambda.x * inv_w0 + lambda.y * inv_w1 + lambda.z * inv_w2;

    if (std::abs(interpolated_inv_w) < std::numeric_limits<float>::epsilon()) {
        return std::numeric_limits<float>::max();
    }

    float z_over_w0 = v0_clip.z * inv_w0;
    float z_over_w1 = v1_clip.z * inv_w1;
    float z_over_w2 = v2_clip.z * inv_w2;
    float interpolated_z_over_w = lambda.x * z_over_w0 + lambda.y * z_over_w1 + lambda.z * z_over_w2;
    float interpolated_z_ndc = interpolated_z_over_w / interpolated_inv_w;
    return interpolated_z_ndc;
}

void rasterizeTriangle(const glm::vec4& v0_clip, const glm::vec4& v1_clip, const glm::vec4& v2_clip,
    unsigned char r_flat, unsigned char g_flat, unsigned char b_flat) {

    if (v0_clip.w <= 0.0f || v1_clip.w <= 0.0f || v2_clip.w <= 0.0f) {
        return;
    }

    glm::vec3 v0_ndc = glm::vec3(v0_clip) / v0_clip.w;
    glm::vec3 v1_ndc = glm::vec3(v1_clip) / v1_clip.w;
    glm::vec3 v2_ndc = glm::vec3(v2_clip) / v2_clip.w;

    glm::vec2 v0_screen = glm::vec2((v0_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v0_ndc.y) * 0.5f * screenHeight);
    glm::vec2 v1_screen = glm::vec2((v1_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v1_ndc.y) * 0.5f * screenHeight);
    glm::vec2 v2_screen = glm::vec2((v2_ndc.x + 1.0f) * 0.5f * screenWidth, (1.0f - v2_ndc.y) * 0.5f * screenHeight);

    int minX = static_cast<int>(std::max(0.0f, std::min({ v0_screen.x, v1_screen.x, v2_screen.x })));
    int maxX = static_cast<int>(std::min(static_cast<float>(screenWidth - 1), std::ceil(std::max({ v0_screen.x, v1_screen.x, v2_screen.x }))));
    int minY = static_cast<int>(std::max(0.0f, std::min({ v0_screen.y, v1_screen.y, v2_screen.y })));
    int maxY = static_cast<int>(std::min(static_cast<float>(screenHeight - 1), std::ceil(std::max({ v0_screen.y, v1_screen.y, v2_screen.y }))));

    float area = edgeFunction(v0_screen, v1_screen, v2_screen);
    if (std::abs(area) < std::numeric_limits<float>::epsilon()) {
        return;
    }

    for (int y_pixel = minY; y_pixel <= maxY; ++y_pixel) {
        for (int x_pixel = minX; x_pixel <= maxX; ++x_pixel) {
            glm::vec2 p = { static_cast<float>(x_pixel) + 0.5f, static_cast<float>(y_pixel) + 0.5f };
            float w0 = edgeFunction(v1_screen, v2_screen, p);
            float w1 = edgeFunction(v2_screen, v0_screen, p);
            float w2 = edgeFunction(v0_screen, v1_screen, p);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                glm::vec3 lambda = glm::vec3(w0 / area, w1 / area, w2 / area);
                float z_ndc = interpolateDepth(lambda, v0_clip, v1_clip, v2_clip);
                float z_screen_depth = (z_ndc + 1.0f) * 0.5f;
                int buffer_idx = y_pixel * screenWidth + x_pixel;

                if (buffer_idx < 0 || buffer_idx >= screenWidth * screenHeight) {
                    continue;
                }

                if (z_screen_depth < depthBuffer[buffer_idx]) {
                    depthBuffer[buffer_idx] = z_screen_depth;
                    frameBuffer[buffer_idx * 3 + 0] = r_flat;
                    frameBuffer[buffer_idx * 3 + 1] = g_flat;
                    frameBuffer[buffer_idx * 3 + 2] = b_flat;
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

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "HW6 - Q1: Flat Shading", NULL, NULL);
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
    glm::mat4 viewMatrix = glm::lookAt(eye_pos_world, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    float nearVal = 0.1f;
    float farVal = 1000.0f;
    glm::mat4 projectionMatrix = glm::frustum(-0.1f, 0.1f, -0.1f, 0.1f, nearVal, farVal);
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;

    std::cout << "Rasterizing with Flat Shading..." << std::endl;
    for (int i = 0; i < gNumTriangles; ++i) {
        int k0 = gIndexBuffer[3 * i + 0];
        int k1 = gIndexBuffer[3 * i + 1];
        int k2 = gIndexBuffer[3 * i + 2];

        glm::vec3 v0_model = gVertexBuffer[k0];
        glm::vec3 v1_model = gVertexBuffer[k1];
        glm::vec3 v2_model = gVertexBuffer[k2];

        glm::vec4 v0_world_h = modelMatrix * glm::vec4(v0_model, 1.0f);
        glm::vec4 v1_world_h = modelMatrix * glm::vec4(v1_model, 1.0f);
        glm::vec4 v2_world_h = modelMatrix * glm::vec4(v2_model, 1.0f);

        glm::vec3 v0_world = glm::vec3(v0_world_h) / v0_world_h.w;
        glm::vec3 v1_world = glm::vec3(v1_world_h) / v1_world_h.w;
        glm::vec3 v2_world = glm::vec3(v2_world_h) / v2_world_h.w;

        glm::vec3 centroid_world = (v0_world + v1_world + v2_world) / 3.0f;
        glm::vec3 edge1_world = v1_world - v0_world;
        glm::vec3 edge2_world = v2_world - v0_world;
        glm::vec3 normal_world = glm::normalize(glm::cross(edge1_world, edge2_world));

        glm::vec3 sphere_center_world(0.0f, 0.0f, -7.0f);
        if (glm::dot(normal_world, centroid_world - sphere_center_world) < 0.0f) {
            normal_world = -normal_world;
        }

        glm::vec3 flat_color_linear = calculateBlinnPhongColorAtPoint(
            centroid_world, normal_world, eye_pos_world,
            mat_ka, mat_kd, mat_ks, p_shininess,
            point_light_pos_world, point_light_color, ambient_light_intensity
        );

        flat_color_linear = glm::clamp(flat_color_linear, 0.0f, 1.0f);
        glm::vec3 flat_color_gamma_corrected = applyGamma(flat_color_linear, gamma_value);
        flat_color_gamma_corrected = glm::clamp(flat_color_gamma_corrected, 0.0f, 1.0f);

        unsigned char r_char = static_cast<unsigned char>(flat_color_gamma_corrected.r * 255.0f);
        unsigned char g_char = static_cast<unsigned char>(flat_color_gamma_corrected.g * 255.0f);
        unsigned char b_char = static_cast<unsigned char>(flat_color_gamma_corrected.b * 255.0f);

        glm::vec4 v0_clip = mvpMatrix * glm::vec4(v0_model, 1.0f);
        glm::vec4 v1_clip = mvpMatrix * glm::vec4(v1_model, 1.0f);
        glm::vec4 v2_clip = mvpMatrix * glm::vec4(v2_model, 1.0f);

        rasterizeTriangle(v0_clip, v1_clip, v2_clip, r_char, g_char, b_char);
    }
    std::cout << "Rasterization complete." << std::endl;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glDrawPixels(screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer.data());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    std::cout << "Cleaning up..." << std::endl;
    // delete_scene(); // 林籍 贸府 肚绰 昏力
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}