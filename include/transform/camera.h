#pragma once

#include <base.h>

namespace Prism
{
    class FreeCamera {
        XMFLOAT3 camera_pos_;
        XMFLOAT3 up_direction_ { 0.0f, 1.0f, 0.0f };
        XMMATRIX projection_;
        float speed_ { 8.0f };
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
        XMMATRIX MakeViewAndProjection();
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

    class ShadowCamera {
        XMFLOAT4 camera_pos_{};
        XMFLOAT4 light_pos_{};
        XMFLOAT4 scene_center_{};
        float scene_radius_ = 1.0f;
    public:
        ShadowCamera() = default;

        void SetSceneBounds(float cx, float cy, float cz, float radius) {
            scene_center_ = { cx, cy, cz, 0.0f };
            scene_radius_ = radius;
        }

        void SetSceneBounds(const XMFLOAT3& center, float radius) {
            scene_center_ = { center.x, center.y, center.z, 0.0f };
            scene_radius_ = radius;
        }

        void Update(FreeCamera& camera, const XMFLOAT4& light_pos) {
            light_pos_ = light_pos;
            camera_pos_ = camera.GetCameraPos4();
        }

        void MakeLightVP(XMFLOAT4X4& dest) {
            XMVECTOR light_pos = XMLoadFloat4(&light_pos_);
            XMVECTOR target = XMLoadFloat4(&scene_center_);
            XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            XMMATRIX view = XMMatrixLookAtLH(light_pos, target, up);

            XMFLOAT3 center_ls;
            XMStoreFloat3(&center_ls, XMVector3TransformCoord(target, view));

            float r = scene_radius_;
            float l = center_ls.x - r;
            float b = center_ls.y - r;
            float n = center_ls.z - r;
            float rt = center_ls.x + r;
            float t = center_ls.y + r;
            float f = center_ls.z + r;
            if (n < 0.01f) n = 0.01f;

            XMMATRIX proj = XMMatrixOrthographicOffCenterLH(l, rt, b, t, n, f);
            XMStoreFloat4x4(&dest, XMMatrixMultiply(view, proj));
        }
    };
}