#pragma once

// Windows headers must come before ViGEm
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <ViGEm/Client.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// GamepadState – plain data struct representing the full Xbox 360 report.
// ---------------------------------------------------------------------------
struct GamepadState
{
    uint16_t buttons      = 0;   // bitmask of XUSB_GAMEPAD_* flags
    uint8_t  leftTrigger  = 0;   // 0-255
    uint8_t  rightTrigger = 0;   // 0-255
    int16_t  leftStickX   = 0;   // -32768 … 32767
    int16_t  leftStickY   = 0;
    int16_t  rightStickX  = 0;
    int16_t  rightStickY  = 0;
};

// ---------------------------------------------------------------------------
// ControllerEmulator
//
// Owns the ViGEmClient connection and up to 4 virtual Xbox 360 targets.
// Each slot (0-3) can be independently connected / disconnected.
// ---------------------------------------------------------------------------

// Per-slot user data forwarded to the ViGEm vibration callback.
struct RumbleCallbackData
{
    class ControllerEmulator* self  = nullptr;
    int                       index = 0;
};

class ControllerEmulator
{
public:
    static constexpr int kMaxControllers = 4;

    ControllerEmulator();
    ~ControllerEmulator();

    // Non-copyable / non-movable
    ControllerEmulator(const ControllerEmulator&)            = delete;
    ControllerEmulator& operator=(const ControllerEmulator&) = delete;

    // Returns false if the ViGEmBus driver could not be reached.
    bool               IsDriverAvailable() const { return m_driverAvailable; }
    const std::string& GetError()          const { return m_errorMessage; }

    // Connect / disconnect a virtual controller slot (0-3).
    // Connect returns false if the slot is already taken or the driver failed.
    bool Connect   (int index);
    void Disconnect(int index);
    bool IsConnected(int index) const;

    // Push a new gamepad report to the driver.  No-op if slot is not connected.
    void SubmitState(int index, const GamepadState& state);

    // Returns the last known motor intensities sent by the host application.
    // Values are 0-255; both are 0 when the controller is idle / not connected.
    // Returns the last non-zero motor values seen by the callback; clears them after reading.
    // Call once per frame: if the result is non-zero a rumble event just arrived.
    void     ConsumePeak   (int index, uint8_t& large, uint8_t& small);
    // Total number of vibration callbacks received on this slot since last Connect().
    uint32_t GetNotifyCount(int index) const;

private:
    static VOID CALLBACK OnX360Notification(
        PVIGEM_CLIENT Client, PVIGEM_TARGET Target,
        UCHAR LargeMotor, UCHAR SmallMotor,
        UCHAR LedNumber, LPVOID UserData);

    PVIGEM_CLIENT                              m_client          = nullptr;
    std::array<PVIGEM_TARGET, kMaxControllers> m_targets         = {};
    std::array<bool,          kMaxControllers> m_connected       = {};
    bool                                       m_driverAvailable = false;
    std::string                                m_errorMessage;

    std::array<RumbleCallbackData,    kMaxControllers> m_callbackData  = {};
    // Peak non-zero motor values since last ConsumePeak() call.
    // Written by the callback thread, read+cleared by the UI thread.
    std::array<std::atomic<uint8_t>,  kMaxControllers> m_peakLarge;
    std::array<std::atomic<uint8_t>,  kMaxControllers> m_peakSmall;
    // Counts how many times the notification callback has fired — useful for diagnostics.
    std::array<std::atomic<uint32_t>, kMaxControllers> m_notifyCount;
};
