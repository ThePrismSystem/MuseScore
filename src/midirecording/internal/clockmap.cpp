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
#include "clockmap.h"

#include <algorithm>
#include <cmath>

using namespace mu::midirecording;

static constexpr double CLOCKMAP_ADVANCE_EPSILON_SECS = 1e-6;
//! A step faster than this many playback seconds per host second, beyond the slack, is a seek
static constexpr double CLOCKMAP_MAX_SPEED = 4.0;
//! Allows for reports bunched by the queue: a seek jumps seconds, queue jitter is milliseconds
static constexpr double CLOCKMAP_JUMP_SLACK_SECS = 0.1;
static constexpr double CLOCKMAP_INTERCEPT_PERCENTILE = 0.9;
static constexpr double CLOCKMAP_NS_PER_SEC = 1e9;

bool mu::midirecording::clockSampleAdvances(const ClockSample& prev, const ClockSample& next)
{
    const double dSecs = next.playbackSecs - prev.playbackSecs;
    const double dHost = static_cast<double>(next.hostNs - prev.hostNs) / CLOCKMAP_NS_PER_SEC;
    return dSecs > CLOCKMAP_ADVANCE_EPSILON_SECS && dSecs <= CLOCKMAP_MAX_SPEED * dHost + CLOCKMAP_JUMP_SLACK_SECS;
}

bool mu::midirecording::clockSampleJumps(const ClockSample& prev, const ClockSample& next)
{
    const double dSecs = next.playbackSecs - prev.playbackSecs;
    const double dHost = static_cast<double>(next.hostNs - prev.hostNs) / CLOCKMAP_NS_PER_SEC;
    return dSecs < -CLOCKMAP_ADVANCE_EPSILON_SECS || dSecs > CLOCKMAP_MAX_SPEED * dHost + CLOCKMAP_JUMP_SLACK_SECS;
}

void ClockMap::addSample(const ClockSample& sample)
{
    m_samples.push_back(sample);
    m_dirty = true;
}

void ClockMap::clear()
{
    m_samples.clear();
    m_dirty = true;
}

const std::vector<ClockSample>& ClockMap::samples() const
{
    return m_samples;
}

bool ClockMap::isValid() const
{
    refit();
    return m_valid;
}

double ClockMap::slope() const
{
    refit();
    return m_valid ? m_slope : 0.0;
}

double ClockMap::secsAt(int64_t hostNs) const
{
    refit();
    if (!m_valid) {
        return 0.0;
    }

    const double x = static_cast<double>(hostNs - m_originNs) / CLOCKMAP_NS_PER_SEC;
    return m_slope * x + m_intercept;
}

void ClockMap::refit() const
{
    if (!m_dirty) {
        return;
    }

    m_dirty = false;
    m_valid = false;
    m_slope = 0.0;
    m_intercept = 0.0;

    std::vector<size_t> used;
    for (size_t i = 1; i < m_samples.size(); ++i) {
        if (clockSampleAdvances(m_samples[i - 1], m_samples[i])) {
            used.push_back(i);
        }
    }

    const size_t count = used.size();
    if (count < 2) {
        return;
    }

    m_originNs = m_samples[used.front()].hostNs;

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    for (size_t i : used) {
        const double x = static_cast<double>(m_samples[i].hostNs - m_originNs) / CLOCKMAP_NS_PER_SEC;
        const double y = m_samples[i].playbackSecs;
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
    }

    const double n = static_cast<double>(count);
    const double denominator = n * sumXX - sumX * sumX;
    if (denominator <= 0.0) {
        return;
    }

    const double slope = (n * sumXY - sumX * sumY) / denominator;
    if (slope <= 0.0) {
        return;
    }
    m_slope = slope;

    std::vector<double> residuals;
    residuals.reserve(count);
    for (size_t i : used) {
        const double x = static_cast<double>(m_samples[i].hostNs - m_originNs) / CLOCKMAP_NS_PER_SEC;
        residuals.push_back(m_samples[i].playbackSecs - m_slope * x);
    }
    std::sort(residuals.begin(), residuals.end());

    const size_t rank = static_cast<size_t>(std::ceil(CLOCKMAP_INTERCEPT_PERCENTILE * n));
    m_intercept = residuals[std::min(std::max<size_t>(rank, 1), count) - 1];
    m_valid = true;
}
