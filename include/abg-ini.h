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

class property;
/// Convenience typefef for shared_ptr to @ref property.
typedef shared_ptr<property> property_sptr;

/// The base class of the different kinds of properties of an INI
/// file.
class property
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;
  priv_sptr priv_;

public:

  property();

  property(const string& name);

  const string&
  get_name() const;

  void
  set_name(const string& name);

  virtual ~property();
}; // end class property

class property_value;

/// Convenience typedef for a shared_ptr to @ref property_value.
typedef shared_ptr<property_value> property_value_sptr;

/// Base class of propertie values.
class property_value
{
public:
  enum value_kind
  {
    ABSTRACT_PROPERTY_VALUE = 0,
    STRING_PROPERTY_VALUE = 1,
    TUPLE_PROPERTY_VALUE = 2,
  };

private:
  struct priv;
  typedef shared_ptr<priv> priv_sptr;
  priv_sptr priv_;

public:

  property_value();
  property_value(value_kind);

  value_kind
  get_kind() const;

  virtual const string&
  as_string() const = 0;

  operator const string& () const;

  virtual ~property_value();
}; // end class property_value.

class string_property_value;

/// A convenience typedef for a shared_ptr to @ref string_property_value.
typedef shared_ptr<string_property_value> string_property_value_sptr;

/// A property value which is a string.
class string_property_value : public property_value
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;
  priv_sptr priv_;

public:
  string_property_value();
  string_property_value(const string& value);

  void
  set_content(const string&);

  virtual const string&
  as_string() const;

  operator string() const;

  virtual ~string_property_value();
}; // end class string_property_value

string_property_value*
is_string_property_value(const property_value*);

string_property_value_sptr
is_string_property_value(const property_value_sptr);

class tuple_property_value;

/// Convenience typedef for a shared_ptr to a @ref
/// tuple_property_value.
typedef shared_ptr<tuple_property_value> tuple_property_value_sptr;

/// A property value that is a tuple.
///
/// Each element of the tuple is itself a property value that can
/// either be a string, or another tuple, for instance.
class tuple_property_value : public property_value
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;
  priv_sptr priv_;

public:
  tuple_property_value(const vector<property_value_sptr>&);

  const vector<property_value_sptr>&
  get_value_items() const;

  vector<property_value_sptr>&
  get_value_items();

  virtual const string&
  as_string() const;

  operator string() const;

  virtual ~tuple_property_value();
}; // end class tuple_property_value

tuple_property_value*
is_tuple_property_value(const property_value*);

tuple_property_value_sptr
is_tuple_property_value(const property_value_sptr);

class simple_property;
/// Convenience typedef for a shared_ptr to an @ref simple_property.
typedef shared_ptr<simple_property> simple_property_sptr;

/// A simple property.  That is, one which value is a string.
class simple_property : public property
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;

  priv_sptr priv_;

public:
  simple_property();

  simple_property(const string& name,
		  const string_property_value_sptr value);

  const string_property_value_sptr&
  get_value() const;

  void
  set_value(const string_property_value_sptr value);

  virtual ~simple_property();
}; // end class simple_property

simple_property*
is_simple_property(const property* p);

simple_property_sptr
is_simple_property(const property_sptr p);

class tuple_property;
/// Convenience typedef for a shared_ptr of @ref tuple_property.
typedef shared_ptr<tuple_property> tuple_property_sptr;

/// Abstraction of a tuple property.  A tuple property is a property
/// which value is a tuple.
class tuple_property : public property
{
  struct priv;
  typedef shared_ptr<priv> priv_sptr;

  priv_sptr priv_;

public:
  tuple_property();

  tuple_property(const string& name,
		 const tuple_property_value_sptr v);

  void
  set_value(const tuple_property_value_sptr value);

  const tuple_property_value_sptr&
  get_value() const;

  virtual
  ~tuple_property();
}; // end class tuple_property

tuple_property*
is_tuple_property(const property* p);

tuple_property_sptr
is_tuple_property(const property_sptr p);

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
  typedef vector<property_sptr> property_vector;

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
  add_property(const property_sptr prop);

  property_sptr
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
