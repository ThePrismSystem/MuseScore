/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "recordingtypes.h"

namespace mu::midirecording {
//! Maps host time to playback position. Position reports reach the main
//! thread through a queue, so a sample can only ever arrive late: the true
//! line lies on or above the sampled points, and the intercept comes from a
//! high percentile of the residuals rather than their mean. Samples taken
//! before the position starts moving (the count-in) are left out.
class ClockMap
{
public:
    void addSample(const ClockSample& sample);
    void clear();

    const std::vector<ClockSample>& samples() const;

    //! At least two samples after the playback position starts advancing
    bool isValid() const;

    //! Playback seconds per host second; 0 while invalid
    double slope() const;

    //! Playback position at hostNs; 0 while invalid
    double secsAt(int64_t hostNs) const;

private:
    void refit() const;

    std::vector<ClockSample> m_samples;

    mutable bool m_dirty = true;
    mutable bool m_valid = false;
    mutable double m_slope = 0.0;
    mutable double m_intercept = 0.0;
    mutable int64_t m_originNs = 0;
};
}
