#pragma once

#include <base.h>

namespace Prism
{

    template<typename T>
    concept is_transform = requires(T t)
    {
        { t.MakeTransformMatrix() } -> std::same_as<XMMATRIX>;
    };

    struct TranslationTransform {
        float x;
        float y;
        float z;

        XMMATRIX MakeTransformMatrix() const {
            return XMMatrixTranslation(x, y, z);
        }
    };

    struct ScalingTransform {
        float x;
        float y;
        float z;

        ScalingTransform() : x(1.0f), y(1.0f), z(1.0f) {}
        ScalingTransform(float s) : x(s), y(s), z(s) {}

        XMMATRIX MakeTransformMatrix() const {
            return XMMatrixScaling(x, y, z);
        }
    };

    struct EulerRotationTransform {
        float pitch;
        float yaw;
        float roll;

        XMMATRIX MakeTransformMatrix() const {
            return XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
        }
    };

    struct XYZRotationTransform {
        float x;
        float y;
        float z;

        XMMATRIX MakeTransformMatrix() const {
            return XMMatrixRotationX(x) * XMMatrixRotationY(y) * XMMatrixRotationZ(z);
        }
    };

    struct XYZWRotationTransform {
        float x;
        float y;
        float z;
        float w;

        XYZWRotationTransform() : w(1.0f) {}

        XMMATRIX MakeTransformMatrix() const {
            return XMMatrixRotationQuaternion(XMVectorSet(x, y, z, w));
        }
    };

    template<typename... Trans>
    requires (is_transform<Trans> && ...)
    XMMATRIX MakeWorldMatrix(const Trans&... trans) {
        XMMATRIX xm = (trans.MakeTransformMatrix() * ...);
        return xm;
    }

    template<typename... Trans>
    requires (is_transform<Trans> && ...)
    XMFLOAT4X4 MakeWorldMatrixF(const Trans&... trans) {
        XMMATRIX xm = (trans.MakeTransformMatrix() * ...);
        XMFLOAT4X4 dest;
        XMStoreFloat4x4(&dest, xm);
        return dest;
    }
}