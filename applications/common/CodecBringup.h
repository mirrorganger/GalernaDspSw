#pragma once

namespace galerna::app
{

// Probes and configures the ES8388 codec over I2C2 for I2S duplex operation. Needs a real
// HAL_Delay() between reset() and the register-configuration script, so this is a genuine HAL
// passthrough rather than host-testable control logic -- same reasoning as BoardControls.cpp.
// Returns false (with a diagnostic already printed) if the codec doesn't ACK or register
// configuration fails; the caller must not start the audio engine in that case, since every
// App's init() assumes the codec is already configured by the time it's called.
bool runCodecBringup();

} // namespace galerna::app
