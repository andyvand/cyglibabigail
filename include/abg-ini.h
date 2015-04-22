// -*- Mode: C++ -*-
//
// Copyright (C) 2013-2015 Red Hat, Inc.
//
// This file is part of the GNU Application Binary Interface Generic
// Analysis and Instrumentation Library (libabigail).  This library is
// free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the
// Free Software Foundation; either version 3, or (at your option) any
// later version.

// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Lesser Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this program; see the file COPYING-LGPLV3.  If
// not, see <http://www.gnu.org/licenses/>.
//
// Author: Dodji Seketeli

/// @file
///
/// This file contains the declarations for the ini file reader used in
/// the libabigail library.

#ifndef __ABG_INI_H__
#define __ABG_INI_H__

#include <tr1/memory>
#include <string>
#include <vector>
#include <istream>
#include <ostream>

namespace abigail
{
/// Namespace for handling ini-style files
namespace ini
{
// Inject some standard types in this namespace.
using std::tr1::shared_ptr;
using std::tr1::dynamic_pointer_cast;
using std::string;
using std::vector;
using std:: pair;

class property_base;
/// Convenience typefef for shared_ptr to @ref property_base.
typedef shared_ptr<property_base> property_base_sptr;

/// The base class of the different kinds of properties of an INI
/// file.
class property_base
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;
  priv_sptr priv_;

public:

  property_base();

  property_base(const string& name);

  const string&
  get_name() const;

  void
  set_name(const string& name);

  virtual ~property_base();
}; // end class property_base

class simple_property;
/// Convenience typedef for a shared_ptr to an @ref simple_property.
typedef shared_ptr<simple_property> simple_property_sptr;

/// A simple property.  That is, one which value is a string.
class simple_property : public property_base
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;

  priv_sptr priv_;

public:
  simple_property();

  simple_property(const string& name, const string& value);

  const string&
  get_value() const;

  void
  set_value(const string&);

  virtual ~simple_property();
}; // end class simple_property

simple_property*
is_simple_property(const property_base* p);

simple_property_sptr
is_simple_property(const property_base_sptr p);

class tuple_property;
/// Convenience typedef for a shared_ptr of @ref tuple_property.
typedef shared_ptr<tuple_property> tuple_property_sptr;

/// Abstraction of a tuple property.  A tuple property is a property
/// which value is a tuple.
class tuple_property : public property_base
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;

  priv_sptr priv_;

public:
  tuple_property();

  tuple_property(const string& name,
		 const vector<string>& values);

  void
  set_values(const vector<string>& values);

  const vector<string>&
  get_values() const;

  vector<string>&
  get_values();

  virtual
  ~tuple_property();
}; // end class tuple_property

tuple_property*
is_tuple_property(const property_base* p);

tuple_property_sptr
is_tuple_property(const property_base_sptr p);

class config;

/// A convenience typedef for a shared pointer to @ref config
typedef shared_ptr<config> config_sptr;

/// The abstraction of the structured content of an .ini file.  This
/// roughly follows what is explained at
/// http://en.wikipedia.org/wiki/INI_file.
class config
{
  class priv;
  typedef shared_ptr<priv> priv_sptr;

public:
  class section;
  /// A convenience typedef for a shared pointer to a config::section.
  typedef shared_ptr<section> section_sptr;

  /// A convenience typedef for a vector of config::section_sptr.
  typedef vector<section_sptr> sections_type;

  /// A convenience typedef for a vector of @ref property_sptr
  typedef vector<property_base_sptr> property_vector;

private:
  priv_sptr priv_;

public:

  config();

  config(const string& path,
	 sections_type& sections);

  virtual ~config();

  const string&
  get_path() const;

  void
  set_path(const string& path);

  const sections_type&
  get_sections() const;

  void
  set_sections(const sections_type& sections);
}; // end class config

/// The abstraction of one section of the .ini config.
class config::section
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;

  priv_sptr priv_;

  // Forbid this
  section();

public:
  section(const string& name);

  section(const string& name, const property_vector& properties);

  const string&
  get_name() const;

  const property_vector&
  get_properties() const;

  void
  set_properties(const property_vector& properties);

  void
  add_property(const property_base_sptr prop);

  property_base_sptr
  find_property(const string& prop_name) const;

  virtual ~section();
}; //end class config::section

bool
read_sections(std::istream& input,
	      config::sections_type& sections);

bool
read_sections(const string& path,
	      config::sections_type& sections);

bool
read_config(std::istream& input,
	    config& conf);

config_sptr
read_config(std::istream& input);

bool
read_config(const string& path,
	    config& conf);

config_sptr
read_config(const string& path);

bool
write_sections(const config::sections_type& sections,
	       std::ostream& output);

bool
write_sections(const config::sections_type& sections,
	       const string& path);

bool
write_config(const config& conf,
	     std::ostream& output);

bool
write_config(const config& conf,
	     const string& path);

}// end namespace ini
}// end namespace abigail
#endif // __ABG_INI_H__
