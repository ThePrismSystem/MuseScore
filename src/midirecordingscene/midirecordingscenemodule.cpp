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

#include "midirecordingscenemodule.h"

#include "modularity/ioc.h"
#include "ui/iuiactionsregister.h"

#include "internal/midirecordingconfiguration.h"
#include "internal/midirecordingcontroller.h"
#include "internal/midirecordingmenu.h"
#include "internal/midirecordinguiactions.h"

using namespace muse;
using namespace muse::modularity;
using namespace muse::ui;
using namespace mu::midirecording;

std::string MidiRecordingSceneModule::moduleName() const
{
    return "midirecordingscene";
}

void MidiRecordingSceneModule::registerExports()
{
    m_configuration = std::make_shared<MidiRecordingConfiguration>();
    m_controller = std::make_shared<MidiRecordingController>(iocContext());
    m_uiActions = std::make_shared<MidiRecordingUiActions>(m_controller);

    ioc()->registerExport<IMidiRecordingConfiguration>(moduleName(), m_configuration);

    m_menu = std::make_shared<MidiRecordingMenu>();
    ioc()->registerExport<IMidiRecordingMenu>(moduleName(), m_menu);
}

void MidiRecordingSceneModule::resolveImports()
{
    auto ar = ioc()->resolve<IUiActionsRegister>(moduleName());
    if (ar) {
        ar->reg(m_uiActions);
    }
}

void MidiRecordingSceneModule::onInit(const IApplication::RunMode& mode)
{
    m_configuration->init();

    if (mode != IApplication::RunMode::GuiApp) {
        return;
    }

    m_controller->init();
    m_uiActions->init();
    m_menu->init();
}
