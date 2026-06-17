#include "ControllerPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

// =============================================================================
// LAYOUT CONSTANTS
// Tutti i valori sono in "unità canoniche" (0..kCW x 0..kCH).
// A runtime vengono moltiplicati per `scale` = larghezza_pannello / kCW,
// quindi le proporzioni rimangono identiche a qualsiasi dimensione finestra.
//
// Modifica queste costanti per riposizionare o ridimensionare ogni elemento.
// =============================================================================

// ── Canvas ───────────────────────────────────────────────────────────────────
static constexpr float kCW = 300.0f;   // larghezza canonica del canvas
static constexpr float kCH = 250.0f;   // altezza  canonica del canvas

// ── Left Trigger (LT) ────────────────────────────────────────────────────────
static constexpr float kLT_X = 28.0f;  // bordo sinistro
static constexpr float kLT_Y =  2.0f;  // bordo superiore
static constexpr float kLT_W = 74.0f;  // larghezza
static constexpr float kLT_H = 26.0f;  // altezza

// ── Right Trigger (RT) ───────────────────────────────────────────────────────
static constexpr float kRT_X = 198.0f;
static constexpr float kRT_Y =   2.0f;
static constexpr float kRT_W =  74.0f;
static constexpr float kRT_H =  26.0f;

// ── Left Bumper (LB) ─────────────────────────────────────────────────────────
static constexpr float kLB_X = 28.0f;
static constexpr float kLB_Y = 30.0f;
static constexpr float kLB_W = 74.0f;
static constexpr float kLB_H = 24.0f;

// ── Right Bumper (RB) ────────────────────────────────────────────────────────
static constexpr float kRB_X = 198.0f;
static constexpr float kRB_Y =  30.0f;
static constexpr float kRB_W =  74.0f;
static constexpr float kRB_H =  24.0f;

// ── Tasti ABXY (cluster diamante) ────────────────────────────────────────────
static constexpr float kFace_CX =  238.0f; // centro X del cluster
static constexpr float kFace_CY =   100.0f; // centro Y del cluster
static constexpr float kFace_D  =   23.0f; // distanza centro→ogni tasto
static constexpr float kFace_R  =   16.0f; // raggio di ogni tasto

// ── D-Pad ────────────────────────────────────────────────────────────────────
static constexpr float kDPad_CX  =  102.0f; // centro X
static constexpr float kDPad_CY  = 170.0f; // centro Y
static constexpr float kDPad_Arm =  18.0f; // lunghezza braccio (centro→punta)
static constexpr float kDPad_HW  =  14.0f; // metà larghezza di ogni braccio
static constexpr float kDPad_R   =   4.0f; // raggio angoli arrotondati

// ── Back ─────────────────────────────────────────────────────────────────────
static constexpr float kBack_CX = 132.0f;  // centro X
static constexpr float kBack_CY =  65.0f;  // centro Y
static constexpr float kBack_R  =  13.0f;  // raggio

// ── Start ────────────────────────────────────────────────────────────────────
static constexpr float kStart_CX = 168.0f;
static constexpr float kStart_CY =  65.0f;
static constexpr float kStart_R  =  13.0f;

// ── Left Stick (LS) ──────────────────────────────────────────────────────────
static constexpr float kLS_CX =  65.0f;  // centro X
static constexpr float kLS_CY =  97.0f;  // centro Y
static constexpr float kLS_R  =  35.0f;  // raggio area interattiva

// ── Right Stick (RS) ─────────────────────────────────────────────────────────
static constexpr float kRS_CX = 197.0f;
static constexpr float kRS_CY = 170.0f;
static constexpr float kRS_R  =  35.0f;

// ── Etichetta hint "(RMB=click)" ─────────────────────────────────────────────
static constexpr float kHint_Y = 115.0f;  // posizione Y dell'etichetta

// ── Barre Rumble (intensità motori L/S) ──────────────────────────────────────
// Disegnate nell'header in PIXEL REALI (non scalati col canvas).
// Modifica questi valori per spostare/ridimensionare le barre.
static constexpr float kRumble_BarW   = 100.0f; // larghezza di ogni barra
static constexpr float kRumble_BarH   =  30.0f; // altezza di ogni barra
static constexpr float kRumble_Gap    =  10.0f; // spazio tra barra L e barra S
static constexpr float kRumble_AlignX =   0.5f; // 0=sinistra, 0.5=centro, 1=destra
static constexpr float kRumble_OffX   =   0.0f; // spostamento orizzontale fine (px)
static constexpr float kRumble_OffY   =   0.0f; // spostamento verticale fine (px)

// =============================================================================
// PALETTE COLORI
// =============================================================================
static constexpr ImU32 kBody      = IM_COL32( 32,  32,  32, 255);
static constexpr ImU32 kBodyEdge  = IM_COL32( 70,  70,  70, 255);
static constexpr ImU32 kStickBg   = IM_COL32( 16,  16,  16, 255);
static constexpr ImU32 kStickRim  = IM_COL32( 88,  88,  88, 255);
static constexpr ImU32 kStickDot  = IM_COL32(190, 190, 190, 255);
static constexpr ImU32 kXhair     = IM_COL32( 48,  48,  48, 255);
static constexpr ImU32 kDPadCol   = IM_COL32( 52,  52,  52, 255);
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

// Colori tasti faccia (dim = a riposo, lit = premuto)
struct FBCol { ImU32 dim, lit; };
static constexpr FBCol kA = { IM_COL32(10, 72, 10, 255), IM_COL32(22, 185,  22, 255) };
static constexpr FBCol kB = { IM_COL32(80, 10, 10, 255), IM_COL32(215,  28,  28, 255) };
static constexpr FBCol kX = { IM_COL32(10, 26, 95, 255), IM_COL32( 26,  78, 220, 255) };
static constexpr FBCol kY = { IM_COL32(80, 68,  8, 255), IM_COL32(205, 168,  16, 255) };

// Colore accent per player (header + anello stick attivo)
static const ImU32 kAccent[4] = {
    IM_COL32( 46, 204,  46, 255),
    IM_COL32(220,  50,  50, 255),
    IM_COL32( 50, 120, 230, 255),
    IM_COL32(220, 180,  20, 255),
};
static const char* kPlayerName[4] = { "Player 1", "Player 2", "Player 3", "Player 4" };

// =============================================================================
// Helpers
// =============================================================================
static inline float    StoF(int16_t v) { return v / 32767.0f; }
static inline int16_t  FtoS(float   v) {
    return static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
}
static inline ImVec2 operator+(ImVec2 a, ImVec2 b) { return { a.x + b.x, a.y + b.y }; }
static inline ImVec2 scaled(ImVec2 o, float cx, float cy, float s) {
    return { o.x + cx * s, o.y + cy * s };
}

// =============================================================================
ControllerPanel::ControllerPanel(int playerIndex, ControllerEmulator& emulator)
    : m_playerIndex(playerIndex), m_emulator(emulator) {}

void ControllerPanel::Submit()
{
    if (!m_enabled) return;
    m_emulator.SubmitState(m_playerIndex, m_state);

    // Propagate to mirror group (skip if this call originated from a peer)
    if (!m_fromMirror && m_mirrorGroup > 0 && m_peers) {
        for (int i = 0; i < m_peerCount; ++i) {
            ControllerPanel* peer = m_peers[i];
            if (peer && peer != this && peer->m_mirrorGroup == m_mirrorGroup)
                peer->ReceiveMirroredState(m_state);
        }
    }
}

void ControllerPanel::ReceiveMirroredState(const GamepadState& state)
{
    if (!m_enabled) return;
    m_state = state;
    m_fromMirror = true;
    Submit();
    m_fromMirror = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawBody
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawBody(ImDrawList* dl, ImVec2 o, float s)
{
    dl->AddRectFilled(scaled(o,  24,  27, s), scaled(o, 276, 162, s), kBody, 28 * s);
    dl->AddRectFilled(scaled(o,  24, 108, s), scaled(o, 102, 220, s), kBody, 22 * s);
    dl->AddRectFilled(scaled(o, 198, 108, s), scaled(o, 276, 220, s), kBody, 22 * s);
    dl->AddRect      (scaled(o,  24,  27, s), scaled(o, 276, 162, s), kBodyEdge, 28 * s, 0, 1.3f * s);
    dl->AddRect      (scaled(o,  24, 108, s), scaled(o, 102, 220, s), kBodyEdge, 22 * s, 0, 1.3f * s);
    dl->AddRect      (scaled(o, 198, 108, s), scaled(o, 276, 220, s), kBodyEdge, 22 * s, 0, 1.3f * s);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawTrigger — barra orizzontale, trascina per impostare il valore
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawTrigger(ImDrawList* dl, ImVec2 o, float s,
                                   const char* id, uint8_t& val, bool isLeft)
{
    const float pw = isLeft ? kLT_W : kRT_W;
    const float ph = isLeft ? kLT_H : kRT_H;
    const float px = isLeft ? kLT_X : kRT_X;
    const float py = isLeft ? kLT_Y : kRT_Y;

    ImVec2 tl = scaled(o, px,      py,      s);
    ImVec2 br = scaled(o, px + pw, py + ph, s);

    dl->AddRectFilled(tl, br, kTrigBg, 5 * s);

    float frac  = val / 255.0f;
    float fillW = (pw - 2.0f) * s * frac;
    if (fillW > 1.0f)
        dl->AddRectFilled({ tl.x + 1*s, tl.y + 1*s }, { tl.x + 1*s + fillW, br.y - 1*s }, kTrigFill, 4*s);

    dl->AddRect(tl, br, kBodyEdge, 5 * s, 0, 1.0f * s);

    const char* lbl = isLeft ? "LT" : "RT";
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText({ tl.x + (pw*s - ts.x)*0.5f, tl.y + (ph*s - ts.y)*0.5f },
                frac > 0.05f ? kText : kTextDim, lbl);

    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton(id, { pw*s, ph*s });
    if (ImGui::IsItemActive()) {
        float newFrac = (ImGui::GetMousePos().x - tl.x) / (pw * s);
        val = static_cast<uint8_t>(std::clamp(newFrac, 0.0f, 1.0f) * 255.0f);
        Submit();
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        { val = 0; Submit(); }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawBumper — rettangolo arrotondato, tieni premuto per attivare
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawBumper(ImDrawList* dl, ImVec2 o, float s,
                                  const char* id, uint16_t bit, bool isLeft)
{
    const float pw = isLeft ? kLB_W : kRB_W;
    const float ph = isLeft ? kLB_H : kRB_H;
    const float px = isLeft ? kLB_X : kRB_X;
    const float py = isLeft ? kLB_Y : kRB_Y;

    ImVec2 tl = scaled(o, px,      py,      s);
    ImVec2 br = scaled(o, px + pw, py + ph, s);

    bool active = (m_state.buttons & bit) != 0;
    dl->AddRectFilled(tl, br, active ? kBumperAct : kBumper, 6 * s);
    dl->AddRect      (tl, br, kBodyEdge, 6 * s, 0, 1.0f * s);

    const char* lbl = isLeft ? "LB" : "RB";
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText({ tl.x + (pw*s - ts.x)*0.5f, tl.y + (ph*s - ts.y)*0.5f },
                active ? kText : kTextDim, lbl);

    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton(id, { pw*s, ph*s });
    bool changed = false;
    if (ImGui::IsItemActivated())   { m_state.buttons |=  bit; changed = true; }
    if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~bit; changed = true; }
    if (changed) Submit();
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawDPad — croce con 4 aree di hit indipendenti
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawDPad(ImDrawList* dl, ImVec2 o, float s)
{
    const float cx  = o.x + kDPad_CX  * s;
    const float cy  = o.y + kDPad_CY  * s;
    const float arm = kDPad_Arm * s;
    const float hw  = kDPad_HW  * s;
    const float r   = kDPad_R   * s;

    dl->AddRectFilled({ cx-hw,      cy-arm-hw }, { cx+hw,      cy+arm+hw }, kDPadCol, r);
    dl->AddRectFilled({ cx-arm-hw,  cy-hw     }, { cx+arm+hw,  cy+hw     }, kDPadCol, r);
    dl->AddRect      ({ cx-hw,      cy-arm-hw }, { cx+hw,      cy+arm+hw }, kBodyEdge, r, 0, 1.0f*s);
    dl->AddRect      ({ cx-arm-hw,  cy-hw     }, { cx+arm+hw,  cy+hw     }, kBodyEdge, r, 0, 1.0f*s);
    dl->AddCircleFilled({ cx, cy }, hw * 0.55f, IM_COL32(42, 42, 42, 255));

    struct { const char* id; uint16_t bit; float x0, y0, w, h; } dirs[4] = {
        { "dU", XUSB_GAMEPAD_DPAD_UP,    cx-hw,     cy-arm-hw, hw*2, arm  },
        { "dD", XUSB_GAMEPAD_DPAD_DOWN,  cx-hw,     cy+hw,     hw*2, arm  },
        { "dL", XUSB_GAMEPAD_DPAD_LEFT,  cx-arm-hw, cy-hw,     arm,  hw*2 },
        { "dR", XUSB_GAMEPAD_DPAD_RIGHT, cx+hw,     cy-hw,     arm,  hw*2 },
    };

    for (auto& d : dirs) {
        bool active = (m_state.buttons & d.bit) != 0;
        if (active)
            dl->AddRectFilled({ d.x0, d.y0 }, { d.x0+d.w, d.y0+d.h }, kDPadAct, r);
        ImGui::SetCursorScreenPos({ d.x0, d.y0 });
        ImGui::InvisibleButton(d.id, { d.w, d.h });
        bool changed = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |=  d.bit; changed = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~d.bit; changed = true; }
        if (changed) Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawFaceButtons — cerchi colorati ABXY a diamante
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawFaceButtons(ImDrawList* dl, ImVec2 o, float s)
{
    const float cx = o.x + kFace_CX * s;
    const float cy = o.y + kFace_CY * s;
    const float d  = kFace_D * s;
    const float r  = kFace_R * s;

    struct { const char* id; uint16_t bit; float ox, oy; FBCol col; const char* lbl; } btns[4] = {
        { "fA", XUSB_GAMEPAD_A,  0,  d, kA, "A" },
        { "fB", XUSB_GAMEPAD_B,  d,  0, kB, "B" },
        { "fX", XUSB_GAMEPAD_X, -d,  0, kX, "X" },
        { "fY", XUSB_GAMEPAD_Y,  0, -d, kY, "Y" },
    };

    for (auto& b : btns) {
        ImVec2 c = { cx + b.ox, cy + b.oy };
        bool active = (m_state.buttons & b.bit) != 0;

        dl->AddCircleFilled(c, r, active ? b.col.lit : b.col.dim, 32);
        dl->AddCircle      (c, r, kBodyEdge, 32, 1.0f * s);
        if (active)
            dl->AddCircle(c, r * 0.65f, IM_COL32(255, 255, 255, 40), 32, 1.0f * s);

        ImVec2 ts = ImGui::CalcTextSize(b.lbl);
        dl->AddText({ c.x - ts.x*0.5f, c.y - ts.y*0.5f }, kText, b.lbl);

        ImGui::SetCursorScreenPos({ c.x - r, c.y - r });
        ImGui::InvisibleButton(b.id, { r*2, r*2 });
        bool changed = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |=  b.bit; changed = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~b.bit; changed = true; }
        if (changed) Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawStick — pad circolare 2D
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawStick(ImDrawList* dl, ImVec2 o, float s,
                                 const char* id, int16_t& sx, int16_t& sy,
                                 uint16_t clickBit, float cx, float cy, float r)
{
    ImVec2 centre = scaled(o, cx, cy, s);
    float  rad    = r * s;
    float  thr    = rad * 0.36f;

    float fx = StoF(sx);
    float fy = StoF(sy);

    dl->AddCircleFilled(centre, rad, kStickBg, 64);
    dl->AddCircle      (centre, rad, kStickRim, 64, 1.5f * s);
    dl->AddLine({ centre.x - rad + 4*s, centre.y }, { centre.x + rad - 4*s, centre.y }, kXhair);
    dl->AddLine({ centre.x, centre.y - rad + 4*s }, { centre.x, centre.y + rad - 4*s }, kXhair);

    ImVec2 dot = { centre.x + fx*(rad - thr), centre.y - fy*(rad - thr) };
    dl->AddCircleFilled(dot, thr, kStickDot, 32);
    dl->AddCircle      (dot, thr, kStickRim, 32, 1.0f * s);

    if (m_state.buttons & clickBit)
        dl->AddCircle(centre, rad + 2.5f*s, kAccent[m_playerIndex], 64, 2.0f * s);

    ImGui::SetCursorScreenPos({ centre.x - rad, centre.y - rad });
    ImGui::InvisibleButton(id, { rad*2, rad*2 });

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float dx  = (mouse.x - centre.x) / (rad - thr);
        float dy  = -(mouse.y - centre.y) / (rad - thr);
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 1.0f) { dx /= len; dy /= len; }
        int16_t nx = FtoS(dx), ny = FtoS(dy);
        if (nx != sx || ny != sy) { sx = nx; sy = ny; Submit(); }
    }
    if (ImGui::IsItemDeactivated())
        { sx = 0; sy = 0; Submit(); }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        { sx = 0; sy = 0; Submit(); }
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseClicked (ImGuiMouseButton_Right)) { m_state.buttons |=  clickBit; Submit(); }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { m_state.buttons &= ~clickBit; Submit(); }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawMenuButtons — Back, Guide (decorativo), Start
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::DrawMenuButtons(ImDrawList* dl, ImVec2 o, float s)
{
    // Back ( < )
    {
        ImVec2 c = scaled(o, kBack_CX, kBack_CY, s);
        float  r = kBack_R * s;
        bool   a = (m_state.buttons & XUSB_GAMEPAD_BACK) != 0;
        dl->AddCircleFilled(c, r, a ? kMenuAct : kMenuBtn, 32);
        dl->AddCircle      (c, r, kBodyEdge, 32, 1.0f * s);
        ImVec2 ts = ImGui::CalcTextSize("<");
        dl->AddText({ c.x - ts.x*0.5f, c.y - ts.y*0.5f }, a ? kText : kTextDim, "<");
        ImGui::SetCursorScreenPos({ c.x - r, c.y - r });
        ImGui::InvisibleButton("bk", { r*2, r*2 });
        bool ch = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |=  XUSB_GAMEPAD_BACK; ch = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~XUSB_GAMEPAD_BACK; ch = true; }
        if (ch) Submit();
    }

    // Start ( > )
    {
        ImVec2 c = scaled(o, kStart_CX, kStart_CY, s);
        float  r = kStart_R * s;
        bool   a = (m_state.buttons & XUSB_GAMEPAD_START) != 0;
        dl->AddCircleFilled(c, r, a ? kMenuAct : kMenuBtn, 32);
        dl->AddCircle      (c, r, kBodyEdge, 32, 1.0f * s);
        ImVec2 ts = ImGui::CalcTextSize(">");
        dl->AddText({ c.x - ts.x*0.5f, c.y - ts.y*0.5f }, a ? kText : kTextDim, ">");
        ImGui::SetCursorScreenPos({ c.x - r, c.y - r });
        ImGui::InvisibleButton("st", { r*2, r*2 });
        bool ch = false;
        if (ImGui::IsItemActivated())   { m_state.buttons |=  XUSB_GAMEPAD_START; ch = true; }
        if (ImGui::IsItemDeactivated()) { m_state.buttons &= ~XUSB_GAMEPAD_START; ch = true; }
        if (ch) Submit();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Render — entry point, chiamato una volta per frame
// ─────────────────────────────────────────────────────────────────────────────
void ControllerPanel::Render()
{
    ImGui::PushID(m_playerIndex);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Header
    ImVec4 accentF = ImGui::ColorConvertU32ToFloat4(kAccent[m_playerIndex]);
    ImGui::TextColored(accentF, "%s", kPlayerName[m_playerIndex]);
    ImGui::SameLine();

    bool prevEnabled = m_enabled;
    ImGui::Checkbox("##en", &m_enabled);
    if (m_enabled != prevEnabled) {
        if (m_enabled) { m_enabled = m_emulator.Connect(m_playerIndex); }
        else           { m_emulator.Disconnect(m_playerIndex); m_state = {}; }
    }

    ImGui::SameLine();
    ImVec2 dotPos = { ImGui::GetCursorScreenPos().x + 4,
                      ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight()*0.5f };
    dl->AddCircleFilled(dotPos, 5.0f, m_enabled ? kAccent[m_playerIndex] : IM_COL32(60,60,60,255), 16);
    ImGui::Dummy({ 14, ImGui::GetTextLineHeight() });

    // Rumble bars — due barre di intensità per Large Motor (L) e Small Motor (S)
    {
        uint8_t large = 0, small = 0;
        if (m_enabled)
            m_emulator.ConsumePeak(m_playerIndex, large, small);

        // Hold timer: 2 secondi di visibilità con fade graduale
        // NOTA: non ripristinare large/small dal hold — il fade dipende SOLO da m_rumbleHold
        const float kHoldSec = 2.0f;
        if (large > 0 || small > 0) {
            m_rumbleHold  = kHoldSec;
            m_rumbleLarge = large;
            m_rumbleSmall = small;
        } else if (m_rumbleHold > 0.0f) {
            m_rumbleHold = std::max(0.0f, m_rumbleHold - ImGui::GetIO().DeltaTime);
        }

        // holdRatio va da 1.0 (appena vibrato) a 0.0 (spento)
        float holdRatio = m_rumbleHold / kHoldSec;

        // Shimmer pulsante nella prima metà del hold (fase "caldo")
        float shimmer = (holdRatio > 0.5f)
            ? 1.0f + 0.18f * sinf(static_cast<float>(ImGui::GetTime()) * 22.0f)
            : 1.0f;

        float fL = std::min(1.0f, (m_rumbleLarge / 255.0f) * holdRatio * shimmer);
        float fS = std::min(1.0f, (m_rumbleSmall / 255.0f) * holdRatio * shimmer);

        ImGui::SameLine();
        const float lineH = ImGui::GetTextLineHeight();
        const float barW  = kRumble_BarW;
        const float barH  = kRumble_BarH;
        const float gap   = kRumble_Gap;
        const float rowH  = std::max(lineH, barH);   // riga abbastanza alta da contenere le barre
        ImVec2      cur   = ImGui::GetCursorScreenPos();

        // Etichette "L" e "S"
        ImVec2 tsL = ImGui::CalcTextSize("L");
        ImVec2 tsS = ImGui::CalcTextSize("S");

        // Larghezza totale del blocco (etichette + barre) per allineamento
        const float blockW = tsL.x + 2.0f + barW + gap + tsS.x + 2.0f + barW;
        const float availW = ImGui::GetContentRegionAvail().x;
        const float baseX  = cur.x + std::max(0.0f, (availW - blockW) * kRumble_AlignX) + kRumble_OffX;
        const float baseY  = cur.y + kRumble_OffY;

        auto drawBar = [&](float x, float fill, ImU32 colFill, ImU32 colGlow, ImU32 colBorder)
        {
            float y = baseY + (rowH - barH) * 0.5f;
            dl->AddRectFilled({ x, y }, { x + barW, y + barH },
                              IM_COL32(25, 15, 0, 255), 2.5f);
            if (fill > 0.005f) {
                float fw = barW * fill;
                dl->AddRectFilled({ x, y }, { x + fw, y + barH }, colFill, 2.5f);
                // glow sopra la barra
                dl->AddRectFilled({ x, y }, { x + fw, y + barH * 0.45f },
                                  colGlow, 2.5f);
            }
            dl->AddRect({ x, y }, { x + barW, y + barH }, colBorder, 2.5f, 0, 0.9f);
        };

        float ty = baseY + (rowH - tsL.y) * 0.5f;

        int aL = (int)(255 * std::min(1.0f, holdRatio * 2.0f));  // label alpha
        int aS = aL;

        float xL = baseX;
        dl->AddText({ xL, ty }, IM_COL32(215, 215, 215, aL), "L");
        xL += tsL.x + 2.0f;
        drawBar(xL, fL,
                IM_COL32((int)(228 * holdRatio), (int)(82  * holdRatio), 0, 255),
                IM_COL32(255, 200, 100, (int)(80 * holdRatio)),
                IM_COL32(90, 50, 0, 200));

        float xS = xL + barW + gap;
        dl->AddText({ xS, ty }, IM_COL32(215, 215, 215, aS), "S");
        xS += tsS.x + 2.0f;
        drawBar(xS, fS,
                IM_COL32((int)(200 * holdRatio), (int)(180 * holdRatio), 0, 255),
                IM_COL32(255, 240, 140, (int)(80 * holdRatio)),
                IM_COL32(80, 70, 0, 200));

        // Riserva lo spazio dell'intera riga header (da cur.x fino a fine blocco)
        ImGui::Dummy({ (xS + barW) - cur.x, rowH });

        // Tooltip con valori raw + contatore callback (diagnostica)
        if (ImGui::IsItemHovered()) {
            uint32_t hits = m_emulator.GetNotifyCount(m_playerIndex);
            ImGui::SetTooltip("Vibration  L: %d  S: %d\nCallback hits: %u%s",
                              (int)m_rumbleLarge, (int)m_rumbleSmall, hits,
                              hits == 0 ? "\n(no callbacks received yet)" : "");
        }
    }

    {
        ImVec2 lp = ImGui::GetCursorScreenPos();
        dl->AddLine(lp, { lp.x + ImGui::GetContentRegionAvail().x, lp.y }, kAccent[m_playerIndex], 2.0f);
    }
    ImGui::Dummy({ 0, 4.0f });

    // Canvas — fit both width and height so nothing gets clipped
    float  scale  = ImGui::GetContentRegionAvail().x / kCW;
    ImVec2 origin  = ImGui::GetCursorScreenPos();
    float  canvasH = kCH * scale;

    DrawBody(dl, origin, scale);

    if (!m_enabled || !m_emulator.IsDriverAvailable()) {
        dl->AddRectFilled(origin, { origin.x + kCW*scale, origin.y + canvasH }, IM_COL32(0,0,0,165));
        const char* msg = m_emulator.IsDriverAvailable() ? "Disabled" : "Driver missing";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText({ origin.x + (kCW*scale - ts.x)*0.5f, origin.y + (canvasH - ts.y)*0.5f },
                    IM_COL32(130,130,130,255), msg);
        ImGui::SetCursorScreenPos({ origin.x, origin.y + canvasH });
        ImGui::Dummy({ kCW*scale, 1.0f });
        ImGui::PopID();
        return;
    }

    DrawTrigger    (dl, origin, scale, "lt", m_state.leftTrigger,  true);
    DrawTrigger    (dl, origin, scale, "rt", m_state.rightTrigger, false);
    DrawBumper     (dl, origin, scale, "lb", XUSB_GAMEPAD_LEFT_SHOULDER,  true);
    DrawBumper     (dl, origin, scale, "rb", XUSB_GAMEPAD_RIGHT_SHOULDER, false);
    DrawMenuButtons(dl, origin, scale);
    DrawDPad       (dl, origin, scale);
    DrawFaceButtons(dl, origin, scale);

    DrawStick(dl, origin, scale, "ls",
              m_state.leftStickX,  m_state.leftStickY,
              XUSB_GAMEPAD_LEFT_THUMB,  kLS_CX, kLS_CY, kLS_R);
    DrawStick(dl, origin, scale, "rs",
              m_state.rightStickX, m_state.rightStickY,
              XUSB_GAMEPAD_RIGHT_THUMB, kRS_CX, kRS_CY, kRS_R);

    // Hint RMB
    {
        ImVec2 ts  = ImGui::CalcTextSize("(RMB=click)");
        ImVec2 hlp = scaled(origin, (kCW - ts.x/scale)*0.5f, kHint_Y, scale);
        dl->AddText(hlp, IM_COL32(55,55,55,255), "(RMB=click)");
    }

    ImGui::SetCursorScreenPos({ origin.x, origin.y + canvasH });
    ImGui::Dummy({ kCW*scale, 1.0f });
    ImGui::PopID();
}
