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
#pragma once
#include "Common/Utility.hpp"
#include <pegtl.hh>

namespace usbguard
{
  namespace RuleParser
  {
  struct target;
  struct device_id;
  struct device_id_value;
  struct string_value;
  struct multiset_operator;
  struct interface_value;
  struct condition;
  struct str_name;
  struct str_hash;
  struct str_parent_hash;
  struct str_serial;
  struct str_id;
  struct str_via_port;
  struct str_with_interface;
  struct str_if;

  template<typename Rule>
  struct rule_parser_actions : pegtl::nothing<Rule> {};

  template<>
  struct rule_parser_actions<target>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.setTarget(Rule::targetFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct rule_parser_actions<device_id>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        std::vector<std::string> tokens;
        tokenizeString(in.string(), tokens, ":");
        const USBDeviceID device_id(tokens[0], tokens[1]);
        rule.setDeviceID(device_id);
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  static const String stringValueFromRule(const String& value)
  {
    const String string_raw(value.substr(1, value.size() - 2));
    return Utility::unescapeString(string_raw);
  }

  template<typename Rule>
  struct name_actions : pegtl::nothing<Rule> {};

  template<>
  struct name_actions<str_name>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeName().empty()) {
        throw pegtl::parse_error("name attribute already defined", in);
      }
    }
  };

  template<>
  struct name_actions<string_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeName().append(stringValueFromRule(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct name_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeName().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct id_actions : pegtl::nothing<Rule> {};

  template<>
  struct id_actions<str_id>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeDeviceID().empty()) {
        throw pegtl::parse_error("id attribute already defined", in);
      }
    }
  };

  template<>
  struct id_actions<device_id_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        std::vector<std::string> tokens;
        tokenizeString(in.string(), tokens, ":");
        const USBDeviceID device_id(tokens[0], tokens[1]);
        rule.attributeDeviceID().append(device_id);
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct id_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeDeviceID().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct hash_actions : pegtl::nothing<Rule> {};

  template<>
  struct hash_actions<str_hash>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeHash().empty()) {
        throw pegtl::parse_error("hash attribute already defined", in);
      }
    }
  };

  template<>
  struct hash_actions<string_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeHash().append(stringValueFromRule(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct hash_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeHash().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct parent_hash_actions : pegtl::nothing<Rule> {};

  template<>
  struct parent_hash_actions<str_parent_hash>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeParentHash().empty()) {
        throw pegtl::parse_error("parent-hash attribute already defined", in);
      }
    }
  };

  template<>
  struct parent_hash_actions<string_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeParentHash().append(stringValueFromRule(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct parent_hash_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeParentHash().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct serial_actions : pegtl::nothing<Rule> {};

  template<>
  struct serial_actions<str_serial>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeSerial().empty()) {
        throw pegtl::parse_error("serial attribute already defined", in);
      }
    }
  };

  template<>
  struct serial_actions<string_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeSerial().append(stringValueFromRule(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct serial_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeSerial().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct via_port_actions : pegtl::nothing<Rule> {};

  template<>
  struct via_port_actions<str_via_port>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeViaPort().empty()) {
        throw pegtl::parse_error("via-port attribute already defined", in);
      }
    }
  };

  template<>
  struct via_port_actions<string_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeViaPort().append(stringValueFromRule(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct via_port_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeViaPort().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct with_interface_actions : pegtl::nothing<Rule> {};

  template<>
  struct with_interface_actions<str_with_interface>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeWithInterface().empty()) {
        throw pegtl::parse_error("with-interface attribute already defined", in);
      }
    }
  };

  template<>
  struct with_interface_actions<interface_value>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        const USBInterfaceType interface_type(in.string());
        rule.attributeWithInterface().append(interface_type);
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct with_interface_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeWithInterface().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<typename Rule>
  struct condition_actions : pegtl::nothing<Rule> {};

  template<>
  struct condition_actions<str_if>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      if (!rule.attributeConditions().empty()) {
        throw pegtl::parse_error("conditions already defined", in);
      }
    }
  };

  template<>
  struct condition_actions<condition>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        RuleCondition* condition = RuleCondition::getImplementation(in.string());
        rule.attributeConditions().append(condition);
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };

  template<>
  struct condition_actions<multiset_operator>
  {
    template<typename Input>
    static void apply(const Input& in, Rule& rule)
    {
      try {
        rule.attributeConditions().setSetOperator(Rule::setOperatorFromString(in.string()));
      }
      catch(const std::exception& ex) {
        throw pegtl::parse_error(ex.what(), in);
      }
    }
  };
  } /* namespace RuleParser */
} /* namespace usbguard */
