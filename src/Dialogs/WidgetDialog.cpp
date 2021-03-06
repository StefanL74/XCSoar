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

#include "Dialogs/WidgetDialog.hpp"
#include "Look/DialogLook.hpp"
#include "Form/Form.hpp"
#include "Form/ButtonPanel.hpp"
#include "Widget/Widget.hpp"
#include "Language/Language.hpp"
#include "Screen/SingleWindow.hpp"
#include "Screen/Layout.hpp"

gcc_const
static WindowStyle
GetDialogStyle()
{
  WindowStyle style;
  style.Hide();
  style.ControlParent();
  return style;
}

WidgetDialog::WidgetDialog(const DialogLook &look)
  :WndForm(look),
   buttons(GetClientAreaWindow(), look),
   widget(GetClientAreaWindow()),
   changed(false)
{
}

void
WidgetDialog::Create(SingleWindow &parent,
                     const TCHAR *caption, const PixelRect &rc,
                     Widget *_widget)
{
  auto_size = false;
  WndForm::Create(parent, rc, caption, GetDialogStyle());
  widget.Set(_widget);
  widget.Move(buttons.UpdateLayout());
}

void
WidgetDialog::CreateFull(SingleWindow &parent, const TCHAR *caption,
                         Widget *widget)
{
  Create(parent, caption, parent.GetClientRect(), widget);
}

void
WidgetDialog::CreateAuto(SingleWindow &parent, const TCHAR *caption,
                         Widget *_widget)
{
  auto_size = true;
  WndForm::Create(parent, caption, GetDialogStyle());
  widget.Set(_widget);
  widget.Move(buttons.UpdateLayout());
}

void
WidgetDialog::CreatePreliminary(SingleWindow &parent, const TCHAR *caption)
{
  WndForm::Create(parent, parent.GetClientRect(), caption, GetDialogStyle());
}

void
WidgetDialog::FinishPreliminary(Widget *_widget)
{
  assert(IsDefined());
  assert(!widget.IsDefined());
  assert(_widget != nullptr);

  widget.Set(_widget);
  widget.Move(buttons.UpdateLayout());

  AutoSize();
}

void
WidgetDialog::AutoSize()
{
  const PixelRect parent_rc = GetParentClientRect();
  const PixelSize parent_size = parent_rc.GetSize();

  widget.Prepare();
  PixelSize min_size = widget.Get()->GetMinimumSize();
  min_size.cy += GetTitleHeight();
  PixelSize max_size = widget.Get()->GetMaximumSize();
  max_size.cy += GetTitleHeight();

  const PixelScalar min_height_with_buttons =
    min_size.cy + Layout::GetMaximumControlHeight();
  const PixelScalar max_height_with_buttons =
    max_size.cy + Layout::GetMaximumControlHeight();
  if (/* need full dialog height even for minimum widget height? */
      min_height_with_buttons >= parent_size.cy ||
      /* try to avoid putting buttons left on portrait screens; try to
         comply with maximum widget height only on landscape
         screens */
      (parent_size.cx > parent_size.cy &&
       max_height_with_buttons >= parent_size.cy)) {
    /* need full height, buttons must be left */
    PixelRect rc = parent_rc;
    if (max_size.cy < parent_size.cy)
      rc.bottom = rc.top + max_size.cy;

    PixelRect remaining = buttons.LeftLayout(rc);
    PixelSize remaining_size = remaining.GetSize();
    if (remaining_size.cx > max_size.cx)
      rc.right -= remaining_size.cx - max_size.cx;

    Move(rc);
    widget.Move(buttons.LeftLayout());
    return;
  }

  /* see if buttons fit at the bottom */

  PixelRect rc = parent_rc;
  if (max_size.cx < parent_size.cx)
    rc.right = rc.left + max_size.cx;

  PixelRect remaining = buttons.BottomLayout(rc);
  PixelSize remaining_size = remaining.GetSize();

  if (remaining_size.cy > max_size.cy)
    rc.bottom -= remaining_size.cy - max_size.cy;

  Move(rc);
  widget.Move(buttons.BottomLayout());
}

int
WidgetDialog::ShowModal()
{
  if (auto_size)
    AutoSize();
  else
    widget.Move(buttons.UpdateLayout());

  widget.Show();
  return WndForm::ShowModal();
}

void
WidgetDialog::OnAction(int id)
{
  if (id == mrOK) {
    bool require_restart;
    if (!widget.Get()->Save(changed, require_restart))
      return;
  }

  WndForm::OnAction(id);
}

void
WidgetDialog::OnDestroy()
{
  widget.Unprepare();

  WndForm::OnDestroy();
}

void
WidgetDialog::OnResize(UPixelScalar width, UPixelScalar height)
{
  WndForm::OnResize(width, height);

  if (auto_size)
    return;

  widget.Move(buttons.UpdateLayout());
}

bool
DefaultWidgetDialog(SingleWindow &parent, const DialogLook &look,
                    const TCHAR *caption, const PixelRect &rc, Widget &widget)
{
  WidgetDialog dialog(look);
  dialog.Create(parent, caption, rc, &widget);
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);

  dialog.ShowModal();

  /* the caller manages the Widget */
  dialog.StealWidget();

  return dialog.GetChanged();
}

bool
DefaultWidgetDialog(SingleWindow &parent, const DialogLook &look,
                    const TCHAR *caption, Widget &widget)
{
  WidgetDialog dialog(look);
  dialog.CreateAuto(parent, caption, &widget);
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);

  dialog.ShowModal();

  /* the caller manages the Widget */
  dialog.StealWidget();

  return dialog.GetChanged();
}
