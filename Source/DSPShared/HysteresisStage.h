#pragma once

#include <cmath>
#include <algorithm>

namespace kalyx::dsp
{

/**
 * Modèle 64 bits d'hystérésis magnétique de transformateur de console :
 * - Console 0 (SSL 4000 E) : Transformateur Jensen (saturation serrée sous 95 Hz)
 * - Console 1 (SSL 4000 G) : Étage VCA sans transformateur (sous-grave ultra-propre)
 * - Console 2 (API 1608)   : Transformateur de sortie API 2503 (impact 130 Hz + punch bas-médium)
 * - Console 3 (Neve 1073)  : Transformateurs Marinair LO1166 (poids harmonique profond 165 Hz + inductance)
 */
class HysteresisStage
{
public:
    HysteresisStage() noexcept = default;

    void reset() noexcept
    {
        flux_   = 0.0;
        prevIn_ = 0.0;
    }

    [[nodiscard]] inline float processSample(float x, float drive, double sampleRate, int consoleModel) noexcept
    {
        const double x64 = static_cast<double>(x);
        const double d64 = static_cast<double>(drive);

        if (d64 <= 1.0e-5 || std::abs(x64) < 1.0e-6)
        {
            flux_ *= 0.9;
            if (std::abs(flux_) < 1.0e-12)
                flux_ = 0.0;
            prevIn_ = 0.0;
            return x;
        }

        double fc    = 95.0;
        double kHyst = 2.3;
        double amt   = 0.14;

        switch (consoleModel)
        {
            case 0: // SSL 4000 E (Jensen)
                fc = 95.0;  kHyst = 2.3; amt = 0.14;
                break;
            case 1: // SSL 4000 G (VCA clean)
                fc = 65.0;  kHyst = 1.4; amt = 0.05;
                break;
            case 2: // API 1608 (2503)
                fc = 130.0; kHyst = 2.6; amt = 0.18;
                break;
            case 3: // Neve 1073 (Marinair LO1166)
            default:
                fc = 165.0; kHyst = 2.9; amt = 0.24;
                break;
        }

        const double alpha = std::exp(-2.0 * 3.14159265358979323846 * fc / std::max(sampleRate, 44100.0));
        flux_ = alpha * flux_ + (1.0 - alpha) * x64;

        const double dx = x64 - prevIn_;
        prevIn_ = x64;

        const double hFlux  = flux_ + 0.18 * dx;
        const double nonlin = std::tanh(kHyst * d64 * hFlux) - (kHyst * d64 * hFlux);
        const double out    = x64 + amt * nonlin;
        return std::isfinite(out) ? static_cast<float>(out) : x;
    }

private:
    double flux_   { 0.0 };
    double prevIn_ { 0.0 };
};

} // namespace kalyx::dsp
