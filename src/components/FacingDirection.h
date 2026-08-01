#pragma once

struct FacingDirection {
    float dx = 1.0f;  //朝向 X 分量（单位向量，InputSystem 跟随鼠标更新）
    float dy = 0.0f;  //朝向 Y 分量（单位向量，InputSystem 跟随鼠标更新）
};
