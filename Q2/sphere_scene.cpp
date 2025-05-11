//
//  sphere_scene.cpp
//  Rasterizer based on the provided sphere_scene.c
//
//

#include <stdio.h>
#include <math.h>
#include <glm/vec3.hpp> // Include GLM for vec3 type
#include "../Q1/sphere_scene.h"

// Global variables
int         gNumVertices = 0;        // Number of 3D vertices.
int         gNumTriangles = 0;        // Number of triangles.
int* gIndexBuffer = nullptr;  // Vertex indices for the triangles.
glm::vec3* gVertexBuffer = nullptr;  // Vertex coordinates array (using glm::vec3)

// Function to create the sphere geometry
void create_scene()
{
    // Define sphere parameters (resolution)
    int width = 32; // Number of divisions around the equator
    int height = 16; // Number of divisions from pole to pole

    float theta, phi;
    int t; // Vertex counter

    // Calculate total number of vertices and triangles
    gNumVertices = (height - 2) * width + 2; // vertices in the middle + 2 poles
    gNumTriangles = (height - 2) * (width - 1) * 2; // triangles in the middle strips

    // 1. Allocate the vertex buffer array
    gVertexBuffer = new glm::vec3[gNumVertices];
    if (!gVertexBuffer) {
        fprintf(stderr, "Error allocating memory for vertex buffer\n");
        return;
    }

    // Allocate the index buffer array
    // Note: 3 indices per triangle
    gIndexBuffer = new int[3 * gNumTriangles];
    if (!gIndexBuffer) {
        fprintf(stderr, "Error allocating memory for index buffer\n");
        delete[] gVertexBuffer; // Clean up previously allocated memory
        gVertexBuffer = nullptr;
        return;
    }


    t = 0; // Initialize vertex index counter

    // Generate vertices for the main body of the sphere (excluding poles)
    for (int j = 1; j < height - 1; ++j) // Iterate through latitude lines
    {
        for (int i = 0; i < width; ++i) // Iterate through longitude lines
        {
            // Calculate spherical coordinates (theta: polar angle, phi: azimuthal angle)
            // Note: M_PI might require defining _USE_MATH_DEFINES before including math.h/cmath on some compilers
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
            theta = (float)j / (height - 1) * M_PI;        // Angle from the positive Y axis
            phi = (float)i / (width - 1) * M_PI * 2;    // Angle around the Y axis

            // Convert spherical coordinates to Cartesian coordinates (unit sphere)
            float   x = sinf(theta) * cosf(phi);
            float   y = cosf(theta);
            float   z = -sinf(theta) * sinf(phi); // Negative Z to match common conventions if needed

            // 2. Set vertex t in the vertex array to {x, y, z}.
            gVertexBuffer[t] = glm::vec3(x, y, z);
            t++; // Increment vertex index
        }
    }

    // Add the North Pole vertex
    // 3. Set vertex t in the vertex array to {0, 1, 0}.
    gVertexBuffer[t] = glm::vec3(0.0f, 1.0f, 0.0f);
    t++; // Increment vertex index

    // Add the South Pole vertex
    // 4. Set vertex t in the vertex array to {0, -1, 0}.
    gVertexBuffer[t] = glm::vec3(0.0f, -1.0f, 0.0f);
    t++; // Increment vertex index

    // --- Generate the index buffer ---
    t = 0; // Reset counter for index buffer population

    // Generate indices for the main body triangles (quad strips)
    for (int j = 0; j < height - 3; ++j) // Iterate through latitude strips
    {
        for (int i = 0; i < width - 1; ++i) // Iterate through longitude quads
        {
            // Triangle 1 of the quad
            gIndexBuffer[t++] = j * width + i;              // Bottom-left vertex
            gIndexBuffer[t++] = (j + 1) * width + (i + 1);  // Top-right vertex
            gIndexBuffer[t++] = j * width + (i + 1);        // Bottom-right vertex

            // Triangle 2 of the quad
            gIndexBuffer[t++] = j * width + i;              // Bottom-left vertex
            gIndexBuffer[t++] = (j + 1) * width + i;        // Top-left vertex
            gIndexBuffer[t++] = (j + 1) * width + (i + 1);  // Top-right vertex
        }
    }

    // Generate indices for the triangles connecting to the poles (fans)
    int northPoleIndex = (height - 2) * width; // Index of the North Pole vertex
    int southPoleIndex = northPoleIndex + 1; // Index of the South Pole vertex

    // Triangles connecting the top strip to the North Pole
    for (int i = 0; i < width - 1; ++i)
    {
        gIndexBuffer[t++] = northPoleIndex; // North Pole
        gIndexBuffer[t++] = i;              // Vertex on the top strip
        gIndexBuffer[t++] = i + 1;          // Next vertex on the top strip
    }

    // Triangles connecting the bottom strip to the South Pole
    int bottomStripStart = (height - 3) * width;
    for (int i = 0; i < width - 1; ++i)
    {
        gIndexBuffer[t++] = southPoleIndex;             // South Pole
        gIndexBuffer[t++] = bottomStripStart + (i + 1); // Next vertex on the bottom strip
        gIndexBuffer[t++] = bottomStripStart + i;       // Vertex on the bottom strip
    }

    // The index buffer has now been generated.
    // To determine the vertices of triangle i (0 <= i < gNumTriangles):
    // k0 = gIndexBuffer[3*i + 0];
    // k1 = gIndexBuffer[3*i + 1];
    // k2 = gIndexBuffer[3*i + 2];
    // The vertices are gVertexBuffer[k0], gVertexBuffer[k1], and gVertexBuffer[k2].
}

