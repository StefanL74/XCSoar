/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "InputEvents.hpp"
#include "UIActions.hpp"
#include "Interface.hpp"
#include "MainWindow.hpp"
#include "Gauge/BigTrafficWidget.hpp"
#include "FLARM/Glue.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/TextEntry.hpp"
#include "Dialogs/Traffic/TrafficDialogs.hpp"
#include "Language/Language.hpp"
#include "FLARM/FlarmDetails.hpp"
#include "FLARM/Glue.hpp"

void
InputEvents::eventFLARMRadar(gcc_unused const TCHAR *misc)
{
  if (StringIsEqual(misc, _T("ForceToggle"))) {
    CommonInterface::main_window->ToggleForceFLARMRadar();
  } else
    CommonInterface::main_window->ToggleSuppressFLARMRadar();
}

// FLARM Traffic
// Displays the FLARM traffic dialog
void
InputEvents::eventFlarmTraffic(gcc_unused const TCHAR *misc)
{
  UIActions::ShowTrafficRadar();
}

void
InputEvents::eventTraffic(const TCHAR *misc)
{
  LoadFlarmDatabases();

  if (StringIsEqual(misc, _T("show"))) {
    UIActions::ShowTrafficRadar();
    return;
  }

  TrafficWidget *traffic_widget = (TrafficWidget *)
    CommonInterface::main_window->GetFlavourWidget(_T("Traffic"));
  if (traffic_widget == nullptr)
    return;

  if (StringIsEqual(misc, _T("zoom auto toggle"))) {
    traffic_widget->ToggleAutoZoom();
  } else if (StringIsEqual(misc, _T("zoom in"))) {
    traffic_widget->ZoomIn();
  } else if (StringIsEqual(misc, _T("zoom out"))) {
    traffic_widget->ZoomOut();
  } else if (StringIsEqual(misc, _T("northup toggle"))) {
    traffic_widget->ToggleNorthUp();
  } else if (StringIsEqual(misc, _T("details"))) {
    traffic_widget->OpenDetails();
  } else if (StringIsEqual(misc, _T("label toggle"))) {
    traffic_widget->SwitchData();
  }
}

void
InputEvents::eventFlarmDetails(gcc_unused const TCHAR *misc)
{
  LoadFlarmDatabases();

  StaticString<4> callsign;
  callsign.clear();
  if (!TextEntryDialog(callsign, _("Competition ID")) ||
      callsign.empty())
    return;

  FlarmId ids[30];
  unsigned count = FlarmDetails::FindIdsByCallSign(callsign, ids, 30);

  if (count > 0) {
    FlarmId id = dlgFlarmDetailsListShowModal(_("Show details:"), ids, count);

    if (id.IsDefined())
      dlgFlarmTrafficDetailsShowModal(id);
  } else {
    ShowMessageBox(_("Unknown competition number"),
                _("Not found"), MB_OK | MB_ICONINFORMATION);
  }
}
