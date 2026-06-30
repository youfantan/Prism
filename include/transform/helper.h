#pragma once

#include <base.h>

namespace Prism
{
    inline void GenerateNormal(XMFLOAT3* p0, XMFLOAT3* p1, XMFLOAT3* p2, XMFLOAT3* dest) {
        XMVECTOR v0 = XMLoadFloat3(p0);
        XMVECTOR v1 = XMLoadFloat3(p1);
        XMVECTOR v2 = XMLoadFloat3(p2);
        XMVECTOR v3 = XMVectorSubtract(v1, v0);
        XMVECTOR v4 = XMVectorSubtract(v2, v0);
        XMVECTOR v_normal = XMVector3Normalize(XMVector3Cross(v3, v4));
        XMStoreFloat3(dest, v_normal);
    }

    class GeometryGenerator {
    public:
        struct Vertex {
            float x;
            float y;
            float z;
        };

        using Index = uint32_t;
    public:
        static std::pair<std::vector<Vertex>, std::vector<Index>> GenUVSphere(uint32_t subdiv) {
            std::vector<Vertex> vertices;
            std::vector<Index> indices;
            for (uint32_t j = 0; j <= subdiv; j++) {
                float theta = (float)j / (float)subdiv * XM_PI;
                float sin_theta = sinf(theta);
                float cos_theta = cosf(theta);
                for (uint32_t i = 0; i <= subdiv; i++) {
                    float phi = (float)i / (float)subdiv * 2.0f * XM_PI;
                    float sin_phi = sinf(phi);
                    float cos_phi = cosf(phi);
                    vertices.emplace_back(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi);
                }
            }
            for (uint32_t j = 0; j < subdiv; j++) {
                for (uint32_t i = 0; i < subdiv; i++) {
                    uint32_t a = j * (subdiv + 1) + i;
                    uint32_t b = a + subdiv + 1;
                    indices.insert(indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
                }
            }
            return { vertices, indices };
        }
    };
}