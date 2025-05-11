#pragma once // 헤더 파일 중복 포함 방지

#include <vector>      // std::vector 사용
#include <glm/glm.hpp> // glm::vec3, glm::vec2 사용

// 구체 정점 데이터 및 인덱스를 저장하는 구조체
struct Sphere {
    std::vector<float> vertices;        // 정점 데이터: x, y, z, nx, ny, nz, u, v 순서로 저장
    std::vector<unsigned int> indices;  // 삼각형을 구성하는 인덱스 데이터
    int stride_;                        // 각 정점당 float 개수 (예: 위치 3 + 법선 3 + UV 2 = 8)

    Sphere() : stride_(8) {} // 생성자에서 stride 초기화
};

Sphere generate_sphere(float radius, int sectors, int stacks);
