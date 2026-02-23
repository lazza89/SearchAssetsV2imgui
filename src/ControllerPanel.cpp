#include "ControllerPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Canvas dimensions (canonical units – everything is scaled by panelWidth/kCW)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float kCW = 300.0f;   // canonical canvas width
static constexpr float kCH = 220.0f;   // canonical canvas height

// ─────────────────────────────────────────────────────────────────────────────
// Palette
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ImU32 kBody      = IM_COL32( 32,  32,  32, 255);
static constexpr ImU32 kBodyEdge  = IM_COL32( 70,  70,  70, 255);
static constexpr ImU32 kStickBg   = IM_COL32( 16,  16,  16, 255);
static constexpr ImU32 kStickRim  = IM_COL32( 88,  88,  88, 255);
static constexpr ImU32 kStickDot  = IM_COL32(190, 190, 190, 255);
static constexpr ImU32 kXhair     = IM_COL32( 48,  48,  48, 255);
static constexpr ImU32 kDPad      = IM_COL32( 52,  52,  52, 255);
static constexpr ImU32 kDPadAct   = IM_COL32(135, 135, 135, 255);
static constexpr ImU32 kBumper    = IM_COL32( 52,  52,  52, 255);
static constexpr ImU32 kBumperAct = IM_COL32(145, 145, 145, 255);
static constexpr ImU32 kTrigBg    = IM_COL32( 28,  28,  28, 255);
static constexpr ImU32 kTrigFill  = IM_COL32(200, 128,  22, 255);
static constexpr ImU32 kMenuBtn   = IM_COL32( 58,  58,  58, 255);
static constexpr ImU32 kMenuAct   = IM_COL32(145, 145, 145, 255);
static constexpr ImU32 kGuide     = IM_COL32( 55,  85,  55, 255);
static constexpr ImU32 kGuideRim  = IM_COL32( 80, 130,  80, 255);
static constexpr ImU32 kText      = IM_COL32(215, 215, 215, 255);
static constexpr ImU32 kTextDim   = IM_COL32(100, 100, 100, 255);

// Xbox face-button colours (dim = resting, lit = pressed)
struct FBCol { ImU32 dim, lit; };
static constexpr FBCol kA = { IM_COL32(10, 72, 10, 255), IM_COL32(22,185, 22,255) };
static constexpr FBCol kB = { IM_COL32(80, 10, 10, 255), IM_COL32(215, 28, 28,255) };
static constexpr FBCol kX = { IM_COL32(10, 26, 95, 255), IM_COL32(26, 78,220,255) };
static constexpr FBCol kY = { IM_COL32(80, 68,  8, 255), IM_COL32(205,168, 16,255) };

// Per-player accent (header + stick active ring)
static const ImU32 kAccent[4] = {
    IM_COL32( 46, 204,  46, 255),
    IM_COL32(220,  50,  50, 255),
    IM_COL32( 50, 120, 230, 255),
    IM_COL32(220, 180,  20, 255),
};
static const char* kPlayerName[4] = { "Player 1","Player 2","Player 3","Player 4" };

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static inline float  StoF(int16_t v) { return v / 32767.0f; }
static inline int16_t FtoS(float  v) {
    return static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
}
static inline ImVec2 operator+(ImVec2 a, ImVec2 b) { return {a.x+b.x, a.y+b.y}; }
static inline ImVec2 scaled(ImVec2 o, float cx, float cy, float s) {
    return { o.x + cx*s, o.y + cy*s };
}

// ─────────────────────────────────────────────────────────────────────────────
ControllerPanel::ControllerPanel(int playerIndex, ControllerEmulator& emulator)
    : m_playerIndex(playerIndex), m_emulator(emulator) {}

void ControllerPanel::Submit()
{
    if (m_enabled) m_emulator.SubmitState(m_playerIndex, m_state);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawBody — the dark controller silhouette
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawBody(ImDrawList* dl, ImVec2 o, float s)
{
    // Main body
    dl->AddRectFilled(scaled(o,24,27,s), scaled(o,276,162,s), kBody, 28*s);
    // Left grip
    dl->AddRectFilled(scaled(o,24,108,s), scaled(o,102,220,s), kBody, 22*s);
    // Right grip
    dl->AddRectFilled(scaled(o,198,108,s), scaled(o,276,220,s), kBody, 22*s);
    // Edge highlights
    dl->AddRect(scaled(o,24,27,s),   scaled(o,276,162,s), kBodyEdge, 28*s, 0, 1.3f*s);
    dl->AddRect(scaled(o,24,108,s),  scaled(o,102,220,s), kBodyEdge, 22*s, 0, 1.3f*s);
    dl->AddRect(scaled(o,198,108,s), scaled(o,276,220,s), kBodyEdge, 22*s, 0, 1.3f*s);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawTrigger — horizontal fill bar, drag left/right to set value
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawTrigger(ImDrawList* dl, ImVec2 o, float s,
                                   const char* id, uint8_t& val, bool isLeft)
{
    constexpr float pw = 72.0f, ph = 20.0f;
    const float     px = isLeft ? 28.0f : 200.0f;
    const float     py = 3.0f;

    ImVec2 tl = scaled(o, px,    py,    s);
    ImVec2 br = scaled(o, px+pw, py+ph, s);

    // Background
    dl->AddRectFilled(tl, br, kTrigBg, 5*s);

    // Fill (left = 0, right = max)
    float frac = val / 255.0f;
    float fillW = (pw - 2.0f) * s * frac;
    if (fillW > 1.0f)
        dl->AddRectFilled({tl.x+1*s, tl.y+1*s}, {tl.x+1*s+fillW, br.y-1*s}, kTrigFill, 4*s);

    // Border
    dl->AddRect(tl, br, kBodyEdge, 5*s, 0, 1.0f*s);

    // Label
    const char* lbl = isLeft ? "LT" : "RT";
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText({tl.x + (pw*s - ts.x)*0.5f, tl.y + (ph*s - ts.y)*0.5f},
                frac > 0.05f ? kText : kTextDim, lbl);

    // Interaction – horizontal drag
    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton(id, {pw*s, ph*s});
    if (ImGui::IsItemActive()) {
        float newFrac = (ImGui::GetMousePos().x - tl.x) / (pw * s);
        val = static_cast<uint8_t>(std::clamp(newFrac, 0.0f, 1.0f) * 255.0f);
        Submit();
    }
    // Double-click to reset
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        val = 0; Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawBumper — rounded rect, hold to keep pressed
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawBumper(ImDrawList* dl, ImVec2 o, float s,
                                  const char* id, uint16_t bit, bool isLeft)
{
    constexpr float pw = 70.0f, ph = 16.0f;
    const float     px = isLeft ? 30.0f : 200.0f;
    const float     py = 26.0f;

    ImVec2 tl = scaled(o, px,    py,    s);
    ImVec2 br = scaled(o, px+pw, py+ph, s);

    bool active = (m_state.buttons & bit) != 0;
    dl->AddRectFilled(tl, br, active ? kBumperAct : kBumper, 6*s);
    dl->AddRect(tl, br, kBodyEdge, 6*s, 0, 1.0f*s);

    const char* lbl = isLeft ? "LB" : "RB";
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText({tl.x + (pw*s - ts.x)*0.5f, tl.y + (ph*s - ts.y)*0.5f},
                active ? kText : kTextDim, lbl);

    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton(id, {pw*s, ph*s});
    bool changed = false;
    if (ImGui::IsItemActivated())   { m_state.buttons |= bit;  changed = true; }
    if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~bit; changed = true; }
    if (changed) Submit();
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawDPad — plus/cross shape with 4 independent hit areas
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawDPad(ImDrawList* dl, ImVec2 o, float s)
{
    const float cx = o.x + 88.0f * s;
    const float cy = o.y + 155.0f * s;
    const float arm = 13.0f * s;   // from center to tip
    const float hw  = 10.0f * s;   // half-width of each arm
    const float r   =  3.0f * s;   // corner radius

    // Draw cross shape
    dl->AddRectFilled({cx-hw,      cy-arm-hw}, {cx+hw,      cy+arm+hw}, kDPad, r);
    dl->AddRectFilled({cx-arm-hw,  cy-hw},     {cx+arm+hw,  cy+hw    }, kDPad, r);
    dl->AddRect      ({cx-hw,      cy-arm-hw}, {cx+hw,      cy+arm+hw}, kBodyEdge, r, 0, 1.0f*s);
    dl->AddRect      ({cx-arm-hw,  cy-hw},     {cx+arm+hw,  cy+hw    }, kBodyEdge, r, 0, 1.0f*s);
    // Small center cap
    dl->AddCircleFilled({cx, cy}, hw * 0.55f, IM_COL32(42, 42, 42, 255));

    // 4 non-overlapping hit areas (arms only, center square is dead zone)
    struct { const char* id; uint16_t bit; float x0,y0,w,h; } dirs[4] = {
        {"dU", XUSB_GAMEPAD_DPAD_UP,    cx-hw,      cy-arm-hw, hw*2,      arm    },
        {"dD", XUSB_GAMEPAD_DPAD_DOWN,  cx-hw,      cy+hw,     hw*2,      arm    },
        {"dL", XUSB_GAMEPAD_DPAD_LEFT,  cx-arm-hw,  cy-hw,     arm,       hw*2   },
        {"dR", XUSB_GAMEPAD_DPAD_RIGHT, cx+hw,      cy-hw,     arm,       hw*2   },
    };

    for (auto& d : dirs) {
        bool active = (m_state.buttons & d.bit) != 0;
        if (active)
            dl->AddRectFilled({d.x0, d.y0}, {d.x0+d.w, d.y0+d.h}, kDPadAct, r);

        ImGui::SetCursorScreenPos({d.x0, d.y0});
        ImGui::InvisibleButton(d.id, {d.w, d.h});
        bool changed = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |= d.bit;  changed = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~d.bit; changed = true; }
        if (changed) Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawFaceButtons — ABXY coloured circles in diamond layout
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawFaceButtons(ImDrawList* dl, ImVec2 o, float s)
{
    const float cx = o.x + 221.0f * s;
    const float cy = o.y +  95.0f * s;
    const float d  = 18.0f * s;   // distance from cluster centre to each button
    const float r  = 11.0f * s;   // button radius

    struct { const char* id; uint16_t bit; float ox,oy; FBCol col; const char* lbl; } btns[4] = {
        {"fA", XUSB_GAMEPAD_A,  0,  d, kA, "A"},
        {"fB", XUSB_GAMEPAD_B,  d,  0, kB, "B"},
        {"fX", XUSB_GAMEPAD_X, -d,  0, kX, "X"},
        {"fY", XUSB_GAMEPAD_Y,  0, -d, kY, "Y"},
    };

    for (auto& b : btns) {
        ImVec2 c = { cx + b.ox, cy + b.oy };
        bool active = (m_state.buttons & b.bit) != 0;

        dl->AddCircleFilled(c, r,       active ? b.col.lit : b.col.dim, 32);
        dl->AddCircle      (c, r,       kBodyEdge, 32, 1.0f*s);
        // Subtle inner highlight ring when pressed
        if (active)
            dl->AddCircle(c, r * 0.65f, IM_COL32(255,255,255,40), 32, 1.0f*s);

        ImVec2 ts = ImGui::CalcTextSize(b.lbl);
        dl->AddText({c.x - ts.x*0.5f, c.y - ts.y*0.5f}, kText, b.lbl);

        ImGui::SetCursorScreenPos({c.x - r, c.y - r});
        ImGui::InvisibleButton(b.id, {r*2, r*2});
        bool changed = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |= b.bit;  changed = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~b.bit; changed = true; }
        if (changed) Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawStick — 2-D circular pad
//   • Left drag  → move stick
//   • Double-click → snap back to centre
//   • Right-click  → stick-click button (LS / RS)
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawStick(ImDrawList* dl, ImVec2 o, float s,
                                 const char* id, int16_t& sx, int16_t& sy,
                                 uint16_t clickBit, float cx, float cy, float r)
{
    ImVec2 centre = scaled(o, cx, cy, s);
    float  rad    = r * s;
    float  thr    = rad * 0.36f;   // thumb radius

    float fx = StoF(sx);
    float fy = StoF(sy);

    // Background
    dl->AddCircleFilled(centre, rad, kStickBg, 64);
    dl->AddCircle      (centre, rad, kStickRim, 64, 1.5f*s);

    // Crosshair
    dl->AddLine({centre.x - rad + 4*s, centre.y}, {centre.x + rad - 4*s, centre.y}, kXhair);
    dl->AddLine({centre.x, centre.y - rad + 4*s}, {centre.x, centre.y + rad - 4*s}, kXhair);

    // Thumb dot (Y axis: positive = up = negative screen Y)
    ImVec2 dot = { centre.x + fx*(rad - thr), centre.y - fy*(rad - thr) };
    dl->AddCircleFilled(dot, thr,       kStickDot, 32);
    dl->AddCircle      (dot, thr,       kStickRim, 32, 1.0f*s);

    // Active accent ring when stick-clicked
    if (m_state.buttons & clickBit)
        dl->AddCircle(centre, rad + 2.5f*s, kAccent[m_playerIndex], 64, 2.0f*s);

    // Invisible button
    ImGui::SetCursorScreenPos({centre.x - rad, centre.y - rad});
    ImGui::InvisibleButton(id, {rad*2, rad*2});

    // Drag (left mouse button)
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float  dx = (mouse.x - centre.x) / (rad - thr);
        float  dy = -(mouse.y - centre.y) / (rad - thr);
        float  len = sqrtf(dx*dx + dy*dy);
        if (len > 1.0f) { dx /= len; dy /= len; }
        int16_t nx = FtoS(dx), ny = FtoS(dy);
        if (nx != sx || ny != sy) { sx = nx; sy = ny; Submit(); }
    }

    // Release → snap back to centre
    if (ImGui::IsItemDeactivated()) {
        sx = 0; sy = 0; Submit();
    }

    // Double-click → manual snap
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        sx = 0; sy = 0; Submit();
    }

    // Right-click → stick-click button (hold)
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            { m_state.buttons |= clickBit;  Submit(); }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            { m_state.buttons &= ~clickBit; Submit(); }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawMenuButtons — Back, Xbox guide (deco only), Start
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawMenuButtons(ImDrawList* dl, ImVec2 o, float s)
{
    // Back ( < )
    {
        ImVec2 c = scaled(o, 132, 88, s);
        float  r = 9.0f * s;
        bool   a = (m_state.buttons & XUSB_GAMEPAD_BACK) != 0;
        dl->AddCircleFilled(c, r, a ? kMenuAct : kMenuBtn, 32);
        dl->AddCircle      (c, r, kBodyEdge, 32, 1.0f*s);
        ImVec2 ts = ImGui::CalcTextSize("<");
        dl->AddText({c.x - ts.x*0.5f, c.y - ts.y*0.5f}, a ? kText : kTextDim, "<");
        ImGui::SetCursorScreenPos({c.x - r, c.y - r});
        ImGui::InvisibleButton("bk", {r*2, r*2});
        bool ch = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |= XUSB_GAMEPAD_BACK;  ch = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~XUSB_GAMEPAD_BACK; ch = true; }
        if (ch) Submit();
    }

    // Xbox Guide button — decorative only (no standard XInput mapping)
    {
        ImVec2 c = scaled(o, 150, 78, s);
        float  r = 12.0f * s;
        dl->AddCircleFilled(c, r,          kGuide,   32);
        dl->AddCircle      (c, r,          kGuideRim, 32, 1.5f*s);
        dl->AddCircle      (c, r * 0.55f,  kBodyEdge, 32, 1.0f*s);
        // Four-quadrant Xbox logo
        float qr = r * 0.38f;
        struct { float ox,oy; ImU32 col; } q[4] = {
            { -qr*0.7f, -qr*0.7f, IM_COL32(180,180,180,120) },
            {  qr*0.7f, -qr*0.7f, IM_COL32(180,180,180,120) },
            { -qr*0.7f,  qr*0.7f, IM_COL32(180,180,180,120) },
            {  qr*0.7f,  qr*0.7f, IM_COL32(180,180,180,120) },
        };
        for (auto& qq : q)
            dl->AddCircleFilled({c.x+qq.ox, c.y+qq.oy}, qr, qq.col, 16);
    }

    // Start ( > )
    {
        ImVec2 c = scaled(o, 168, 88, s);
        float  r = 9.0f * s;
        bool   a = (m_state.buttons & XUSB_GAMEPAD_START) != 0;
        dl->AddCircleFilled(c, r, a ? kMenuAct : kMenuBtn, 32);
        dl->AddCircle      (c, r, kBodyEdge, 32, 1.0f*s);
        ImVec2 ts = ImGui::CalcTextSize(">");
        dl->AddText({c.x - ts.x*0.5f, c.y - ts.y*0.5f}, a ? kText : kTextDim, ">");
        ImGui::SetCursorScreenPos({c.x - r, c.y - r});
        ImGui::InvisibleButton("st", {r*2, r*2});
        bool ch = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |= XUSB_GAMEPAD_START;  ch = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~XUSB_GAMEPAD_START; ch = true; }
        if (ch) Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Render — main entry point, called once per frame
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::Render()
{
    ImGui::PushID(m_playerIndex);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Header ──────────────────────────────────────────────────────────────
    ImVec4 accentF = ImGui::ColorConvertU32ToFloat4(kAccent[m_playerIndex]);
    ImGui::TextColored(accentF, "%s", kPlayerName[m_playerIndex]);
    ImGui::SameLine();

    bool prevEnabled = m_enabled;
    ImGui::Checkbox("##en", &m_enabled);
    if (m_enabled != prevEnabled) {
        if (m_enabled) { m_enabled = m_emulator.Connect(m_playerIndex); }
        else           { m_emulator.Disconnect(m_playerIndex); m_state = {}; }
    }

    // Status indicator dot
    ImGui::SameLine();
    ImVec2 dotPos = { ImGui::GetCursorScreenPos().x + 4,
                      ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight()*0.5f };
    ImU32  dotCol = m_enabled ? kAccent[m_playerIndex] : IM_COL32(60,60,60,255);
    dl->AddCircleFilled(dotPos, 5.0f, dotCol, 16);
    ImGui::Dummy({14, ImGui::GetTextLineHeight()});

    // Coloured separator line
    {
        ImVec2 lp = ImGui::GetCursorScreenPos();
        float  lw = ImGui::GetContentRegionAvail().x;
        dl->AddLine(lp, {lp.x+lw, lp.y}, kAccent[m_playerIndex], 2.0f);
    }
    ImGui::Dummy({0, 4.0f});

    // ── Controller canvas ───────────────────────────────────────────────────
    float  scale   = ImGui::GetContentRegionAvail().x / kCW;
    ImVec2 origin  = ImGui::GetCursorScreenPos();
    float  canvasH = kCH * scale;

    // Always draw the body (even when disabled – overlay dims it)
    DrawBody(dl, origin, scale);

    if (!m_enabled || !m_emulator.IsDriverAvailable()) {
        // Dark overlay + "off" text
        dl->AddRectFilled(origin, {origin.x + kCW*scale, origin.y + canvasH},
                          IM_COL32(0,0,0,165));
        const char* msg = m_emulator.IsDriverAvailable() ? "Disabled" : "Driver missing";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText({origin.x + (kCW*scale - ts.x)*0.5f,
                     origin.y + (canvasH - ts.y)*0.5f},
                    IM_COL32(130,130,130,255), msg);
        // Advance cursor past canvas
        ImGui::SetCursorScreenPos({origin.x, origin.y + canvasH});
        ImGui::Dummy({kCW*scale, 1.0f});
        ImGui::PopID();
        return;
    }

    // Interactive elements — each uses SetCursorScreenPos + InvisibleButton
    DrawTrigger    (dl, origin, scale, "lt", m_state.leftTrigger,  true);
    DrawTrigger    (dl, origin, scale, "rt", m_state.rightTrigger, false);
    DrawBumper     (dl, origin, scale, "lb", XUSB_GAMEPAD_LEFT_SHOULDER,  true);
    DrawBumper     (dl, origin, scale, "rb", XUSB_GAMEPAD_RIGHT_SHOULDER, false);
    DrawMenuButtons(dl, origin, scale);
    DrawDPad       (dl, origin, scale);
    DrawFaceButtons(dl, origin, scale);

    // Left stick (upper-left area), Right stick (lower-right area)
    DrawStick(dl, origin, scale, "ls",
              m_state.leftStickX,  m_state.leftStickY,
              XUSB_GAMEPAD_LEFT_THUMB,  90.0f, 100.0f, 26.0f);
    DrawStick(dl, origin, scale, "rs",
              m_state.rightStickX, m_state.rightStickY,
              XUSB_GAMEPAD_RIGHT_THUMB, 196.0f, 145.0f, 26.0f);

    // Stick labels (small, below each stick)
    {
        const float labelY = 132.0f;
        ImVec2 llp = scaled(origin,  78.0f, labelY, scale);
        ImVec2 rlp = scaled(origin, 184.0f, labelY, scale);
        dl->AddText(llp, kTextDim, "LS");
        dl->AddText(rlp, kTextDim, "RS");
        ImVec2 ts = ImGui::CalcTextSize("(RMB=click)");
        ImVec2 hlp = scaled(origin, (kCW - ts.x/scale)*0.5f, labelY, scale);
        dl->AddText(hlp, IM_COL32(55,55,55,255), "(RMB=click)");
    }

    // Advance cursor past canvas
    ImGui::SetCursorScreenPos({origin.x, origin.y + canvasH});
    ImGui::Dummy({kCW*scale, 1.0f});

    ImGui::PopID();
}
