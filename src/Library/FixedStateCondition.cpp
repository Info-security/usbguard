//
// Copyright (C) 2016 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#include "FixedStateCondition.hpp"
#include "RuleParser.hpp"

namespace usbguard
{
  FixedStateCondition::FixedStateCondition(bool state, bool negated)
    : RuleCondition(state ? "true" : "false", negated),
      _state(state)
  {
  }

  FixedStateCondition::FixedStateCondition(const FixedStateCondition& rhs)
    : RuleCondition(rhs),
      _state(rhs._state)
  {
  }

  bool FixedStateCondition::update(const Rule& rule)
  {
    (void)rule;
    return _state;
  }

  RuleCondition * FixedStateCondition::clone() const
  {
    return new FixedStateCondition(*this);
  }
} /* namespace usbguard */

