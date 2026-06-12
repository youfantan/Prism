#pragma once

#include <base.h>
#include <unordered_map>

struct alignas(16) float4x4 {
    float f0_0; float f0_1; float f0_2; float f0_3;
    float f1_0; float f1_1; float f1_2; float f1_3;
    float f2_0; float f2_1; float f2_2; float f2_3;
    float f3_0; float f3_1; float f3_2; float f3_3;
};

class FreeCamera {
public:
    struct object_params_t {
        XMFLOAT3 position;
        XMFLOAT3 size { 1.0f, 1.0f, 1.0f };
        XMFLOAT3 rotation;
    };
private:
    XMFLOAT3 camera_pos_;
    XMFLOAT3 up_direction_ { 0.0f, 1.0f, 0.0f };
    XMMATRIX projection_;
    float speed_ { 0.5f };
    float sensitivity_ { 0.005f };
    HWND hwnd_;
    bool is_focus_;
    float yaw_;
    float pitch_;
public:
    FreeCamera(HWND window, uint64_t width, uint64_t height, uint16_t fov_angle, float near_z = 1.0f, float far_z = 1000.0f);

    XMFLOAT3& GetCameraPos() {
        return camera_pos_;
    }

    float& GetYaw() {
        return yaw_;
    }

    float& GetPitch() {
        return pitch_;
    }

    XMFLOAT3 GetFocusPos() {
        return { cosf(pitch_) * sinf(yaw_), sinf(pitch_), cosf(pitch_) * cosf(yaw_) };
    }

    XMFLOAT3& GetUpDirection() {
        return up_direction_;
    }

    float& GetSpeed() {
        return speed_;
    }

    float& GetSensitivity() {
        return sensitivity_;
    }

    bool& IsFocus() {
        return is_focus_;
    }

    void MakeWVP(const object_params_t& op, float4x4& mat);
};

class KMInput {
public:
    struct keyboard_control_key_mappings_t {
        WPARAM forward_vk;
        WPARAM backward_vk;
        WPARAM left_vk;
        WPARAM right_vk;
        WPARAM escape_vk;
    };
private:
    struct keyboard_event_t {
        WPARAM vk;
        float duration_ms;
    };

    keyboard_control_key_mappings_t key_mappings_;
    LARGE_INTEGER freq_;
    LARGE_INTEGER prev_;
    HWND hwnd_;
public:
    KMInput(HWND hwnd, const keyboard_control_key_mappings_t& mappings);
    void UpdateFreeCamera(FreeCamera& fc);
};