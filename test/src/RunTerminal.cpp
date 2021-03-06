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

#define ENABLE_SCREEN

#include "Main.hpp"
#include "Screen/SingleWindow.hpp"
#include "Screen/TerminalWindow.hpp"
#include "Screen/Timer.hpp"
#include "Look/TerminalLook.hpp"

class TestWindow : public SingleWindow {
  TerminalWindow terminal;

  WindowTimer timer;

public:
  TestWindow(const TerminalLook &look)
    :terminal(look), timer(*this) {}

  void Create(PixelSize size) {
    SingleWindow::Create(_T("RunTerminal"), size);

    PixelRect rc = GetClientRect();

    terminal.Create(*this, rc);
  }

protected:
  virtual void OnCreate() {
    SingleWindow::OnCreate();
    timer.Schedule(10);
  }

  virtual void OnDestroy() {
    timer.Cancel();
    SingleWindow::OnDestroy();
  }

  virtual bool OnTimer(WindowTimer &_timer) {
    if (_timer == timer) {
      unsigned r = rand();
      char ch;
      if ((r % 16) == 0)
        ch = '\n';
      else
        ch = 0x20 + ((r / 16) % 0x60);
      terminal.Write(&ch, 1);
      return true;
    } else
      return SingleWindow::OnTimer(_timer);
  }
};

static void
Main()
{
  TerminalLook look;
  look.Initialise(monospace_font);

  TestWindow window(look);
  window.Create({400, 400});
  window.Show();

  window.RunEventLoop();
}
