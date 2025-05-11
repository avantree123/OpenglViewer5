#pragma once // ��� ���� �ߺ� ���� ����

#include <vector>      // std::vector ���
#include <glm/glm.hpp> // glm::vec3, glm::vec2 ���

// ��ü ���� ������ �� �ε����� �����ϴ� ����ü
struct Sphere {
    std::vector<float> vertices;        // ���� ������: x, y, z, nx, ny, nz, u, v ������ ����
    std::vector<unsigned int> indices;  // �ﰢ���� �����ϴ� �ε��� ������
    int stride_;                        // �� ������ float ���� (��: ��ġ 3 + ���� 3 + UV 2 = 8)

    Sphere() : stride_(8) {} // �����ڿ��� stride �ʱ�ȭ
};

Sphere generate_sphere(float radius, int sectors, int stacks);
