#pragma once

#include <base.h>

inline void GenerateNormal(XMFLOAT3* p0, XMFLOAT3* p1, XMFLOAT3* p2, XMFLOAT3* dest) {
    XMVECTOR v0 = XMLoadFloat3(p0);
    XMVECTOR v1 = XMLoadFloat3(p1);
    XMVECTOR v2 = XMLoadFloat3(p2);
    XMVECTOR v3 = XMVectorSubtract(v1, v0);
    XMVECTOR v4 = XMVectorSubtract(v2, v0);
    XMVECTOR v_normal = XMVector3Normalize(XMVector3Cross(v3, v4));
    XMStoreFloat3(dest, v_normal);
}