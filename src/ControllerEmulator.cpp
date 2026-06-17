#include "ControllerEmulator.h"

#include <cstring>   // memset
#include <stdexcept>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ControllerEmulator::ControllerEmulator()
{
    m_targets.fill(nullptr);
    m_connected.fill(false);

    for (int i = 0; i < kMaxControllers; ++i)
    {
        m_callbackData[i] = { this, i };
        m_peakLarge[i].store(0, std::memory_order_relaxed);
        m_peakSmall[i].store(0, std::memory_order_relaxed);
        m_notifyCount[i].store(0, std::memory_order_relaxed);
    }

    // Allocate a ViGEmClient handle
    m_client = vigem_alloc();
    if (!m_client)
    {
        m_errorMessage = "vigem_alloc() failed: out of memory.";
        return;
    }

    // Connect to the ViGEmBus driver
    const VIGEM_ERROR ret = vigem_connect(m_client);
    if (!VIGEM_SUCCESS(ret))
    {
        m_errorMessage =
            "Cannot connect to ViGEmBus driver (error 0x"
            + std::to_string(static_cast<unsigned>(ret)) + ").\n"
            "Please install ViGEmBus from:\n"
            "https://github.com/nefarius/ViGEmBus/releases";

        vigem_free(m_client);
        m_client = nullptr;
        return;
    }

    m_driverAvailable = true;
}

ControllerEmulator::~ControllerEmulator()
{
    // Disconnect all active virtual controllers before releasing the client
    for (int i = 0; i < kMaxControllers; ++i)
        Disconnect(i);

    if (m_client)
    {
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Connect / disconnect
// ---------------------------------------------------------------------------

bool ControllerEmulator::Connect(int index)
{
    if (!m_driverAvailable)           return false;
    if (index < 0 || index >= kMaxControllers) return false;
    if (m_connected[index])           return true;   // already connected

    // Allocate a new Xbox 360 target
    m_targets[index] = vigem_target_x360_alloc();
    if (!m_targets[index])
        return false;

    // Register the target with the driver
    const VIGEM_ERROR ret = vigem_target_add(m_client, m_targets[index]);
    if (!VIGEM_SUCCESS(ret))
    {
        vigem_target_free(m_targets[index]);
        m_targets[index] = nullptr;
        return false;
    }

    // Reset peak state and diagnostic counter for this slot
    m_peakLarge[index].store(0, std::memory_order_relaxed);
    m_peakSmall[index].store(0, std::memory_order_relaxed);
    m_notifyCount[index].store(0, std::memory_order_relaxed);

    // Register vibration notification callback (best-effort; ignore failures)
    vigem_target_x360_register_notification(
        m_client, m_targets[index], OnX360Notification, &m_callbackData[index]);

    m_connected[index] = true;
    return true;
}

void ControllerEmulator::Disconnect(int index)
{
    if (index < 0 || index >= kMaxControllers) return;
    if (!m_connected[index])                   return;

    if (m_targets[index])
        vigem_target_x360_unregister_notification(m_targets[index]);

    if (m_client && m_targets[index])
        vigem_target_remove(m_client, m_targets[index]);

    if (m_targets[index])
    {
        vigem_target_free(m_targets[index]);
        m_targets[index] = nullptr;
    }

    m_peakLarge[index].store(0, std::memory_order_relaxed);
    m_peakSmall[index].store(0, std::memory_order_relaxed);
    m_connected[index] = false;
}

bool ControllerEmulator::IsConnected(int index) const
{
    if (index < 0 || index >= kMaxControllers) return false;
    return m_connected[index];
}

// ---------------------------------------------------------------------------
// State submission
// ---------------------------------------------------------------------------

void ControllerEmulator::SubmitState(int index, const GamepadState& state)
{
    if (!m_driverAvailable)           return;
    if (index < 0 || index >= kMaxControllers) return;
    if (!m_connected[index] || !m_targets[index]) return;

    XUSB_REPORT report;
    memset(&report, 0, sizeof(report));

    report.wButtons       = state.buttons;
    report.bLeftTrigger   = state.leftTrigger;
    report.bRightTrigger  = state.rightTrigger;
    report.sThumbLX       = state.leftStickX;
    report.sThumbLY       = state.leftStickY;
    report.sThumbRX       = state.rightStickX;
    report.sThumbRY       = state.rightStickY;

    vigem_target_x360_update(m_client, m_targets[index], report);
}

// ---------------------------------------------------------------------------
// Rumble query
// ---------------------------------------------------------------------------

void ControllerEmulator::ConsumePeak(int index, uint8_t& large, uint8_t& small)
{
    if (index < 0 || index >= kMaxControllers) { large = 0; small = 0; return; }
    // exchange() atomically reads and clears, so no event is lost between frames.
    large = m_peakLarge[index].exchange(0, std::memory_order_relaxed);
    small = m_peakSmall[index].exchange(0, std::memory_order_relaxed);
}

uint32_t ControllerEmulator::GetNotifyCount(int index) const
{
    if (index < 0 || index >= kMaxControllers) return 0;
    return m_notifyCount[index].load(std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// ViGEm notification callback (called from a driver thread)
// ---------------------------------------------------------------------------

VOID CALLBACK ControllerEmulator::OnX360Notification(
    PVIGEM_CLIENT /*Client*/, PVIGEM_TARGET /*Target*/,
    UCHAR LargeMotor, UCHAR SmallMotor,
    UCHAR /*LedNumber*/, LPVOID UserData)
{
    auto* data = static_cast<RumbleCallbackData*>(UserData);
    // Only propagate non-zero values to the peak; zero (motor-off) is handled
    // by the hold-timer in the UI, so we never lose a short vibration pulse.
    if (LargeMotor > 0) data->self->m_peakLarge[data->index].store(LargeMotor, std::memory_order_relaxed);
    if (SmallMotor > 0) data->self->m_peakSmall[data->index].store(SmallMotor, std::memory_order_relaxed);
    data->self->m_notifyCount[data->index].fetch_add(1, std::memory_order_relaxed);
}
