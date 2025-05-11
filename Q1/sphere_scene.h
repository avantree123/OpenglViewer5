#pragma once
#ifndef SPHERE_SCENE_H
#define SPHERE_SCENE_H

#include <glm/vec3.hpp> 


extern int gNumVertices;
extern int gNumTriangles;
extern int* gIndexBuffer;
extern glm::vec3* gVertexBuffer;


void create_scene();
void delete_scene();

#endif // SPHERE_SCENE_H