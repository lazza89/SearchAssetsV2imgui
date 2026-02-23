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

    m_connected[index] = true;
    return true;
}

void ControllerEmulator::Disconnect(int index)
{
    if (index < 0 || index >= kMaxControllers) return;
    if (!m_connected[index])                   return;

    if (m_client && m_targets[index])
        vigem_target_remove(m_client, m_targets[index]);

    if (m_targets[index])
    {
        vigem_target_free(m_targets[index]);
        m_targets[index] = nullptr;
    }

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
