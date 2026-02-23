#pragma once

#include "ControllerEmulator.h"

#include <imgui.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// ControllerPanel
//
// Renders a visual Xbox 360 controller widget using ImDrawList.
// Buttons, triggers and sticks are interactive and positioned correctly.
// One panel instance per player (0-3).
// ---------------------------------------------------------------------------
class ControllerPanel
{
public:
    ControllerPanel(int playerIndex, ControllerEmulator& emulator);
    void Render();

private:
    // --- Drawing helpers (o = canvas origin in screen coords, s = scale) ---
    void DrawBody          (ImDrawList* dl, ImVec2 o, float s);
    void DrawTrigger       (ImDrawList* dl, ImVec2 o, float s,
                            const char* id, uint8_t& val, bool isLeft);
    void DrawBumper        (ImDrawList* dl, ImVec2 o, float s,
                            const char* id, uint16_t bit, bool isLeft);
    void DrawDPad          (ImDrawList* dl, ImVec2 o, float s);
    void DrawFaceButtons   (ImDrawList* dl, ImVec2 o, float s);
    void DrawStick         (ImDrawList* dl, ImVec2 o, float s,
                            const char* id, int16_t& sx, int16_t& sy,
                            uint16_t clickBit, float cx, float cy, float r);
    void DrawMenuButtons   (ImDrawList* dl, ImVec2 o, float s);

    void Submit();

    int                 m_playerIndex;
    ControllerEmulator& m_emulator;
    GamepadState        m_state;
    bool                m_enabled = false;
};
