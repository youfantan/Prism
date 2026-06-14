#pragma once

#include <base.h>

struct object_params_t {
    XMFLOAT3 position;
    XMFLOAT3 size { 1.0f, 1.0f, 1.0f };
    XMFLOAT3 rotation;
};

void MakeWorld(const object_params_t& op, XMFLOAT4X4& mat);

class FreeCamera {
public:

private:
    XMFLOAT3 camera_pos_;
    XMFLOAT3 up_direction_ { 0.0f, 1.0f, 0.0f };
    XMMATRIX projection_;
    float speed_ { 2.0f };
    float sensitivity_ { 0.002f };
    HWND hwnd_;
    bool is_focus_;
    float yaw_;
    float pitch_;
public:
    FreeCamera(HWND window, uint64_t width, uint64_t height, uint16_t fov_angle, float near_z = 1.0f, float far_z = 1000.0f);

    XMFLOAT3& GetCameraPos() {
        return camera_pos_;
    }

    XMFLOAT4 GetCameraPos4() {
        return { camera_pos_.x, camera_pos_.y, camera_pos_.z, 0.0f };
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

    void MakeViewAndProjection(XMFLOAT4X4& mat);
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