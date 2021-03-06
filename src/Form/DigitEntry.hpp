/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
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

#ifndef XCSOAR_FORM_DIGIT_ENTRY_HPP
#define XCSOAR_FORM_DIGIT_ENTRY_HPP

#include "Screen/PaintWindow.hpp"
#include "Math/fixed.hpp"

#include <assert.h>

class RoughTime;
struct DialogLook;
class ContainerWindow;
class ActionListener;

/**
 * A control that allows entering numbers or other data types digit by
 * digit.  It aims to be usable for both touch screens and knob-only
 * (e.g. Altair).
 */
class DigitEntry : public PaintWindow {
  static const unsigned MAX_LENGTH = 16;

  struct Column {
    enum class Type : uint8_t {
      DIGIT,
      DIGIT6,
      HOUR,
      SIGN,
      DECIMAL_POINT,
      COLON,
      NORTH_SOUTH,
      EAST_WEST,
      UNIT,
      DEGREES,
    };

    Type type;

    uint8_t value;

    unsigned left, right;

    constexpr bool IsSign() const {
      return type == Type::SIGN ||
        type == Type::NORTH_SOUTH || type == Type::EAST_WEST;
    }

    constexpr bool IsNegative() const {
      return value != 0;
    }

    constexpr bool IsNumber() const {
      return type == Type::DIGIT ||
        type == Type::DIGIT6 || type == Type::HOUR;
    }

    constexpr unsigned GetMaxNumber() const {
      return type == Type::DIGIT
        ? 9
        : (type == Type::HOUR ? 23 : 5);
    }

    constexpr bool IsEditable() const {
      return IsNumber() || IsSign();
    }

    void SetNegative(bool is_negative) {
      assert(IsSign());

      value = is_negative;
    }
  };

  const DialogLook &look;

  ActionListener *action_listener;
  int action_id;

  UPixelScalar padding;

  /**
   * Total number of columns.
   */
  unsigned length;

  Column columns[MAX_LENGTH];

  bool valid;

  unsigned top, bottom;

  /**
   * The current digit.
   */
  unsigned cursor;

public:
  DigitEntry(const DialogLook &_look)
    :look(_look), action_listener(nullptr) {}

  virtual ~DigitEntry();

protected:
  void Create(ContainerWindow &parent, const PixelRect &rc,
              const WindowStyle style,
              unsigned length);

public:
  void CreateSigned(ContainerWindow &parent, const PixelRect &rc,
                    const WindowStyle style,
                    unsigned ndigits, unsigned precision);

  void CreateTime(ContainerWindow &parent, const PixelRect &rc,
                  const WindowStyle style);

  gcc_pure
  PixelSize GetRecommendedSize() const {
    return PixelSize(columns[length - 1].right, bottom + top);
  }

  /**
   * Sets a listener that will be notified when the user "activates"
   * the control (for example by pressing the "enter" key).
   */
  void SetActionListener(ActionListener &listener, int id) {
    action_listener = &listener;
    action_id = id;
  }

  void SetCursor(unsigned cursor);

  void SetInvalid();

  void SetValue(int value);
  void SetValue(unsigned value);
  void SetValue(fixed value);
  void SetValue(RoughTime value);

  gcc_pure
  int GetIntegerValue() const;

  gcc_pure
  unsigned GetUnsignedValue() const;

  gcc_pure
  fixed GetFixedValue() const;

  gcc_pure
  RoughTime GetTimeValue() const;

protected:
  gcc_pure
  bool IsSigned() const {
    return columns[0].IsSign();
  }

  gcc_pure
  bool IsNegative() const {
    return columns[0].IsSign() && columns[0].IsNegative();
  }

  gcc_pure
  int FindDecimalPoint() const;

  gcc_pure
  int FindNumberLeft(int i) const;

  gcc_pure
  int FindEditableLeft(int i) const;

  gcc_pure
  int FindEditableRight(unsigned i) const;

  gcc_pure
  unsigned GetPositiveInteger() const;

  gcc_pure
  fixed GetPositiveFractional() const;

  void IncrementColumn(unsigned i);
  void DecrementColumn(unsigned i);

  gcc_pure
  int FindColumnAt(unsigned x) const;

protected:
  virtual void OnSetFocus() gcc_override;
  virtual void OnKillFocus() gcc_override;
  virtual bool OnMouseDown(PixelScalar x, PixelScalar y) gcc_override;
  virtual bool OnKeyCheck(unsigned key_code) const gcc_override;
  virtual bool OnKeyDown(unsigned key_code) gcc_override;
  virtual void OnPaint(Canvas &canvas) gcc_override;
};

#endif
