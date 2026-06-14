#include <transform/camera.h>

void MakeWorld(const object_params_t& op, XMFLOAT4X4& mat) {
    XMMATRIX S = XMMatrixScaling(op.size.x, op.size.y, op.size.z);
    XMMATRIX R = XMMatrixRotationX(op.rotation.x) * XMMatrixRotationY(op.rotation.y) * XMMatrixRotationZ(op.rotation.z);
    XMMATRIX T = XMMatrixTranslation(op.position.x, op.position.y, op.position.z);
    XMMATRIX world = S * R* T;
    XMStoreFloat4x4(&mat, world);
}

FreeCamera::FreeCamera(HWND window, uint64_t width, uint64_t height, uint16_t fov_angle, float near_z, float far_z) : hwnd_(window), camera_pos_{ 0.0f, 2.0f, -2.0f }, yaw_(0.0f) , pitch_(0.0f), projection_() {
    float r_fov = (static_cast<float>(fov_angle) / 180.0f) * XM_PI;
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    projection_ = XMMatrixPerspectiveFovLH(r_fov, aspect, near_z, far_z);
}

void FreeCamera::MakeViewAndProjection(XMFLOAT4X4& mat) {
    XMVECTOR eye = XMLoadFloat3(&camera_pos_);
    XMFLOAT3 focus_pos = GetFocusPos();
    XMVECTOR focus = XMVectorAdd(eye, XMLoadFloat3(&focus_pos));
    XMVECTOR up = XMLoadFloat3(&up_direction_);
    XMMATRIX view = XMMatrixLookAtLH(eye, focus, up);
    XMMATRIX vp = view * projection_;
    XMStoreFloat4x4(&mat, vp);
}

KMInput::KMInput(HWND hwnd, const keyboard_control_key_mappings_t& mappings) : hwnd_(hwnd), key_mappings_(mappings) {
    QueryPerformanceFrequency(&freq_);
    QueryPerformanceCounter(&prev_);
}

void KMInput::UpdateFreeCamera(FreeCamera& fc) {
    // Mouse
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rect;
    GetWindowRect(hwnd_, &rect);
    int cent_x = rect.left + (rect.right - rect.left) / 2;
    int cent_y = rect.top + (rect.bottom - rect.top) / 2;
    if (fc.IsFocus()) SetCursorPos(cent_x, cent_y);
    float cursor_off_x = static_cast<float>(cursor.x) - cent_x;
    fc.GetYaw() += cursor_off_x * fc.GetSensitivity();
    float cursor_off_y = static_cast<float>(cursor.y) - cent_y;
    fc.GetPitch() -= cursor_off_y * fc.GetSensitivity();

    // Keyboard
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float delta_ms = static_cast<float>(now.QuadPart - prev_.QuadPart) / static_cast<float>(freq_.QuadPart) * 1000;
    prev_ = now;
    XMVECTOR v_camera = XMLoadFloat3(&fc.GetCameraPos());
    XMFLOAT3 focus_pos = fc.GetFocusPos();
    XMVECTOR v_focus = XMVectorAdd(v_camera, XMLoadFloat3(&focus_pos));
    XMVECTOR v_cf = XMVectorSubtract(v_focus, v_camera);
    XMVECTOR v_up = XMLoadFloat3(&fc.GetUpDirection());
    XMVECTOR v_fwd = XMVector3Normalize(v_cf);
    XMVECTOR v_right = XMVector3Normalize(XMVector3Cross(v_up, v_cf));
    if (GetAsyncKeyState(key_mappings_.forward_vk)) {
        XMVECTOR offset = XMVectorScale(v_fwd, delta_ms / 1000);
        v_camera = XMVectorAdd(v_camera, offset);
        v_focus = XMVectorAdd(v_focus, offset);
    }
    if (GetAsyncKeyState(key_mappings_.backward_vk)) {
        XMVECTOR offset = XMVectorScale(v_fwd, -delta_ms / 1000);
        v_camera = XMVectorAdd(v_camera, offset);
        v_focus = XMVectorAdd(v_focus, offset);
    }
    if (GetAsyncKeyState(key_mappings_.left_vk)) {
        XMVECTOR offset = XMVectorScale(v_right, -delta_ms / 1000);
        v_camera = XMVectorAdd(v_camera, offset);
        v_focus = XMVectorAdd(v_focus, offset);
    }
    if (GetAsyncKeyState(key_mappings_.right_vk)) {
        XMVECTOR offset = XMVectorScale(v_right, delta_ms / 1000);
        v_camera = XMVectorAdd(v_camera, offset);
        v_focus = XMVectorAdd(v_focus, offset);
    }
    if (GetAsyncKeyState(key_mappings_.escape_vk)) {
        DestroyWindow(hwnd_);
    }
    XMStoreFloat3(&fc.GetCameraPos(), v_camera);
}