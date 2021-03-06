// -*- mode: C++ -*-
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

/// @file
///
/// This file contains the definitions of the entry points to
/// de-serialize an instance of @ref abigail::translation_unit to an
/// ABI Instrumentation file in libabigail native XML format.

#include "config.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tr1/unordered_map>
#include "abg-config.h"
#include "abg-corpus.h"

#if WITH_ZIP_ARCHIVE
#include "abg-libzip-utils.h"
#endif

#include "abg-writer.h"
#include "abg-libxml-utils.h"

namespace abigail
{
using std::cerr;
using std::tr1::shared_ptr;
using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;
using std::ofstream;
using std::ostream;
using std::ostringstream;
using std::list;
using std::vector;
using std::tr1::unordered_map;

#if WITH_ZIP_ARCHIVE
using zip_utils::zip_sptr;
using zip_utils::zip_file_sptr;
using zip_utils::open_archive;
using zip_utils::open_file_in_archive;
#endif // WITH_ZIP_ARCHIVE

/// The namespace for the native XML file format writer.
///
/// It contains utilities to serialize ABI artifacts from the @ref ir
/// namespace into the native XML format.
namespace xml_writer
{

class id_manager
{
  unsigned long long m_cur_id;

  unsigned long long
  get_new_id()
  { return ++m_cur_id; }

public:
  id_manager() : m_cur_id(0) { }

  /// Return a unique string representing a numerical id.
  string
  get_id()
  {
    ostringstream o;
    o << get_new_id();
    return o.str();
  }

  /// Return a unique string representing a numerical ID, prefixed by
  /// prefix.
  ///
  /// @param prefix the prefix of the returned unique id.
  string
  get_id_with_prefix(const string& prefix)
  {
    ostringstream o;
    o << prefix << get_new_id();
    return o.str();
  }
};

typedef unordered_map<type_base*,
		      string,
		      type_base::cached_hash,
		      type_ptr_equal> type_ptr_map;

typedef unordered_map<shared_ptr<function_tdecl>,
		      string,
		      function_tdecl::shared_ptr_hash> fn_tmpl_shared_ptr_map;

typedef unordered_map<shared_ptr<class_tdecl>,
		      string,
		      class_tdecl::shared_ptr_hash> class_tmpl_shared_ptr_map;

class write_context
{
  write_context();

public:

  write_context(ostream& os) : m_ostream(os)
  {}

  const config&
  get_config() const
  {return m_config;}

  ostream&
  get_ostream()
  {return m_ostream;}

  id_manager&
  get_id_manager()
  {return m_id_manager;}

  /// @return true iff type has already been assigned an ID.
  bool
  type_has_existing_id(type_base_sptr type) const
  {return type_has_existing_id(type.get());}

  /// @return true iff type has already been assigned an ID.
  bool
  type_has_existing_id(type_base* type) const
  {return (m_type_id_map.find(type) != m_type_id_map.end());}

  /// Associate a unique id to a given type.  For that, put the type
  /// in a hash table, hashing the type.  So if the type has no id
  /// associated to it, create a new one and return it.  Otherwise,
  /// return the existing id for that type.
  string
  get_id_for_type(shared_ptr<type_base> t)
  {return get_id_for_type(t.get());}

  /// Associate a unique id to a given type.  For that, put the type
  /// in a hash table, hashing the type.  So if the type has no id
  /// associated to it, create a new one and return it.  Otherwise,
  /// return the existing id for that type.
  string
  get_id_for_type(type_base* t)
  {
    type_ptr_map::const_iterator it = m_type_id_map.find(t);
    if (it == m_type_id_map.end())
      {
	string id = get_id_manager().get_id_with_prefix("type-id-");
	m_type_id_map[t] = id;
	return id;
      }
    return it->second;
  }

  string
  get_id_for_fn_tmpl(shared_ptr<function_tdecl> f)
  {
    fn_tmpl_shared_ptr_map::const_iterator it = m_fn_tmpl_id_map.find(f);
    if (it == m_fn_tmpl_id_map.end())
      {
	string id = get_id_manager().get_id_with_prefix("fn-tmpl-id-");
	m_fn_tmpl_id_map[f] = id;
	return id;
      }
    return m_fn_tmpl_id_map[f];
  }

  string
  get_id_for_class_tmpl(shared_ptr<class_tdecl> c)
  {
    class_tmpl_shared_ptr_map::const_iterator it = m_class_tmpl_id_map.find(c);
    if (it == m_class_tmpl_id_map.end())
      {
	string id = get_id_manager().get_id_with_prefix("class-tmpl-id-");
	m_class_tmpl_id_map[c] = id;
	return id;
      }
    return m_class_tmpl_id_map[c];
  }

  void
  clear_type_id_map()
  {m_type_id_map.clear();}

  const string_elf_symbol_sptr_map_type&
  get_fun_symbol_map() const
  {return m_fun_symbol_map;}

  string_elf_symbol_sptr_map_type&
  get_fun_symbol_map()
  {return m_fun_symbol_map;}

private:
  id_manager m_id_manager;
  config m_config;
  ostream& m_ostream;
  type_ptr_map m_type_id_map;
  fn_tmpl_shared_ptr_map m_fn_tmpl_id_map;
  class_tmpl_shared_ptr_map m_class_tmpl_id_map;
  string_elf_symbol_sptr_map_type m_fun_symbol_map;
  string_elf_symbol_sptr_map_type m_var_symbol_map;
}; //end write_context

static bool write_translation_unit(const translation_unit&,
				   write_context&, unsigned);
static void write_location(location, translation_unit&, ostream&);
static void write_location(const shared_ptr<decl_base>&, ostream&);
static bool write_visibility(const shared_ptr<decl_base>&, ostream&);
static bool write_binding(const shared_ptr<decl_base>&, ostream&);
static void write_array_size_and_alignment(const shared_ptr<array_type_def>, ostream&);
static void write_size_and_alignment(const shared_ptr<type_base>, ostream&);
static void write_access(access_specifier, ostream&);
static void write_layout_offset(var_decl_sptr, ostream&);
static void write_layout_offset(shared_ptr<class_decl::base_spec>, ostream&);
static void write_cdtor_const_static(bool, bool, bool, bool, ostream&);
static void write_voffset(function_decl_sptr, ostream&);
static void write_elf_symbol_type(elf_symbol::type, ostream&);
static void write_elf_symbol_binding(elf_symbol::binding, ostream&);
static bool write_elf_symbol_aliases(const elf_symbol&, ostream&);
static bool write_elf_symbol_reference(const elf_symbol&, ostream&);
static bool write_elf_symbol_reference(const elf_symbol_sptr, ostream&);
static void write_class_is_declaration_only(const shared_ptr<class_decl>,
					    ostream&);
static void write_is_struct(const shared_ptr<class_decl>, ostream&);
static bool write_decl(const shared_ptr<decl_base>,
		       write_context&, unsigned);
static bool write_type_decl(const shared_ptr<type_decl>,
			    write_context&, unsigned);
static bool write_namespace_decl(const shared_ptr<namespace_decl>,
				 write_context&, unsigned);
static bool write_qualified_type_def(const shared_ptr<qualified_type_def>,
				     write_context&, unsigned);
static bool write_pointer_type_def(const shared_ptr<pointer_type_def>,
				   write_context&, unsigned);
static bool write_reference_type_def(const shared_ptr<reference_type_def>,
				     write_context&, unsigned);
static bool write_array_type_def(const shared_ptr<array_type_def>,
			         write_context&, unsigned);
static bool write_enum_type_decl(const shared_ptr<enum_type_decl>,
				 write_context&, unsigned);
static bool write_typedef_decl(const shared_ptr<typedef_decl>,
			       write_context&, unsigned);
static bool write_elf_symbol(const shared_ptr<elf_symbol>,
			     write_context&, unsigned);
static bool write_elf_symbols_table(const elf_symbols&,
				    write_context&, unsigned);
static bool write_var_decl(const shared_ptr<var_decl>,
			   write_context&, bool, unsigned);
static bool write_function_decl(const shared_ptr<function_decl>,
				write_context&, bool, unsigned);
static bool write_member_type(const type_base_sptr,
			      write_context&, unsigned);
static bool write_class_decl(const shared_ptr<class_decl>,
			     write_context&, unsigned);
static bool write_type_tparameter
(const shared_ptr<type_tparameter>, write_context&, unsigned);
static bool write_non_type_tparameter
(const shared_ptr<non_type_tparameter>, write_context&, unsigned);
static bool write_template_tparameter
(const shared_ptr<template_tparameter>, write_context&, unsigned);
static bool write_type_composition
(const shared_ptr<type_composition>, write_context&, unsigned);
static bool write_template_parameter(const shared_ptr<template_parameter>,
				     write_context&, unsigned);
static void write_template_parameters(const shared_ptr<template_decl>,
				      write_context&, unsigned);
static bool write_function_tdecl
(const shared_ptr<function_tdecl>,
 write_context&, unsigned);
static bool write_class_tdecl
(const shared_ptr<class_tdecl>,
 write_context&, unsigned);
static void	do_indent(ostream&, unsigned);
static void	do_indent_to_level(write_context&, unsigned, unsigned);
static unsigned get_indent_to_level(write_context&, unsigned, unsigned);

/// Emit nb_whitespaces white spaces into the output stream.
void
do_indent(ostream& o, unsigned nb_whitespaces)
{
  for (unsigned i = 0; i < nb_whitespaces; ++i)
    o << ' ';
}

/// Indent initial_indent + level number of xml element indentation.
///
/// @param ctxt the context of the parsing.
///
/// @param initial_indent the initial number of white space to indent to.
///
/// @param level the number of indentation level to indent to.
static void
do_indent_to_level(write_context& ctxt,
		   unsigned initial_indent,
		   unsigned level)
{
  do_indent(ctxt.get_ostream(),
	    get_indent_to_level(ctxt, initial_indent, level));
}

/// Return the number of white space of indentation that
/// #do_indent_to_level would have used.
///
/// @param ctxt the context of the parsing.
///
/// @param initial_indent the initial number of white space to indent to.
///
/// @param level the number of indentation level to indent to.
static unsigned
get_indent_to_level(write_context& ctxt, unsigned initial_indent,
		    unsigned level)
{
    int nb_ws = initial_indent +
      level * ctxt.get_config().get_xml_element_indent();
    return nb_ws;
}

/// Write a location to the output stream.
///
/// If the location is empty, nothing is written.
///
/// @param loc the location to consider.
///
/// @param tu the translation unit the location belongs to.
///
/// @param o the output stream to write to.
static void
write_location(location	loc, translation_unit& tu, ostream&	o)
{
  if (!loc)
    return;

  string filepath;
  unsigned line = 0, column = 0;

  tu.get_loc_mgr().expand_location(loc, filepath, line, column);

  o << " filepath='" << filepath << "'"
    << " line='"     << line     << "'"
    << " column='"   << column   << "'";
}

/// Write the location of a decl to the output stream.
///
/// If the location is empty, nothing is written.
///
/// @param decl the decl to consider.
///
/// @param o the output stream to write to.
static void
write_location(const shared_ptr<decl_base>&	decl,
	       ostream&			o)
{
  if (!decl)
    return;

  location loc = decl->get_location();
  if (!loc)
    return;

  string filepath;
  unsigned line = 0, column = 0;
  translation_unit& tu = *get_translation_unit(decl);

  tu.get_loc_mgr().expand_location(loc, filepath, line, column);

  o << " filepath='" << filepath << "'"
    << " line='"     << line     << "'"
    << " column='"   << column   << "'";
}

/// Serialize the visibility property of the current decl as the
/// 'visibility' attribute for the current xml element.
///
/// @param decl the instance of decl_base to consider.
///
/// @param o the output stream to serialize the property to.
///
/// @return true upon successful completion, false otherwise.
static bool
write_visibility(const shared_ptr<decl_base>&	decl, ostream& o)
{
  if (!decl)
    return false;

  decl_base::visibility v = decl->get_visibility();
  string str;

  switch (v)
    {
    case decl_base::VISIBILITY_NONE:
      return true;
    case decl_base::VISIBILITY_DEFAULT:
      str = "default";
      break;
    case decl_base::VISIBILITY_PROTECTED:
      str = "protected";
      break;
    case decl_base::VISIBILITY_HIDDEN:
      str = "hidden";
      break;
    case decl_base::VISIBILITY_INTERNAL:
	str = "internal";
	break;
    }

  if (str.empty())
    return false;

  o << " visibility='" << str << "'";

  return true;
}

/// Serialize the 'binding' property of the current decl.
///
/// @param decl the decl to consider.
///
/// @param o the output stream to serialize the property to.
static bool
write_binding(const shared_ptr<decl_base>& decl, ostream& o)
{
  if (!decl)
    return false;

  decl_base::binding bind = decl_base::BINDING_NONE;

  shared_ptr<var_decl> var =
    dynamic_pointer_cast<var_decl>(decl);
  if (var)
    bind = var->get_binding();
  else
    {
      shared_ptr<function_decl> fun =
	dynamic_pointer_cast<function_decl>(decl);
      if (fun)
	bind = fun->get_binding();
    }

  string str;
  switch (bind)
    {
    case decl_base::BINDING_NONE:
      break;
    case decl_base::BINDING_LOCAL:
      str = "local";
      break;
    case decl_base::BINDING_GLOBAL:
	str = "global";
      break;
    case decl_base::BINDING_WEAK:
      str = "weak";
      break;
    }

  if (!str.empty())
    o << " binding='" << str << "'";

  return true;
}

/// Serialize the size and alignment attributes of a given type.
///
/// @param decl the type to consider.
///
/// @param o the output stream to serialize to.
static void
write_size_and_alignment(const shared_ptr<type_base> decl, ostream& o)
{
  size_t size_in_bits = decl->get_size_in_bits();
  if (size_in_bits)
    o << " size-in-bits='" << size_in_bits << "'";

  size_t alignment_in_bits = decl->get_alignment_in_bits();
  if (alignment_in_bits)
    o << " alignment-in-bits='" << alignment_in_bits << "'";
}

/// Serialize the size and alignment attributes of a given type.
/// @param decl the type to consider.
///
/// @param o the output stream to serialize to.
static void
write_array_size_and_alignment(const shared_ptr<array_type_def> decl, ostream& o)
{
  if (decl->is_infinite())
    o << " size-in-bits='" << "infinite" << "'";
  else {
    size_t size_in_bits = decl->get_size_in_bits();
    if (size_in_bits)
      o << " size-in-bits='" << size_in_bits << "'";
  }

  size_t alignment_in_bits = decl->get_alignment_in_bits();
  if (alignment_in_bits)
    o << " alignment-in-bits='" << alignment_in_bits << "'";
}
/// Serialize the access specifier.
///
/// @param a the access specifier to serialize.
///
/// @param o the output stream to serialize it to.
static void
write_access(access_specifier a, ostream& o)
{
  string access_str = "private";

  switch (a)
    {
    case private_access:
      access_str = "private";
      break;

    case protected_access:
      access_str = "protected";
      break;

    case public_access:
      access_str = "public";
      break;

    default:
      break;
    }

  o << " access='" << access_str << "'";
}

/// Serialize the layout offset of a data member.
static void
write_layout_offset(var_decl_sptr member, ostream& o)
{
  if (!is_data_member(member))
    return;

  if (get_data_member_is_laid_out(member))
    o << " layout-offset-in-bits='"
      << get_data_member_offset(member)
      << "'";
}

/// Serialize the layout offset of a base class
static void
write_layout_offset(shared_ptr<class_decl::base_spec> base, ostream& o)
{
  if (!base)
    return;

  if (base->get_offset_in_bits() >= 0)
    o << " layout-offset-in-bits='" << base->get_offset_in_bits() << "'";
}

/// Serialize the access specifier of a class member.
///
/// @param member a pointer to the class member to consider.
///
/// @param o the ostream to serialize the member to.
static void
write_access(decl_base_sptr member, ostream& o)
{write_access(get_member_access_specifier(member), o);}

/// Write the voffset of a member function if it's non-zero
///
/// @param fn the member function to consider
///
/// @param o the output stream to write to
static void
write_voffset(function_decl_sptr fn, ostream&o)
{
  if (!fn)
    return;

  if (get_member_function_is_virtual(fn))
    {
      size_t voffset = get_member_function_vtable_offset(fn);
      o << " vtable-offset='" << voffset << "'";
    }
}

/// Serialize an elf_symbol::type into an XML node attribute named
/// 'type'.
///
/// @param t the elf_symbol::type to serialize.
///
/// @param o the output stream to serialize it to.
static void
write_elf_symbol_type(elf_symbol::type t, ostream& o)
{
  string repr;

  switch (t)
    {
    case elf_symbol::NOTYPE_TYPE:
      repr = "no-type";
      break;
    case elf_symbol::OBJECT_TYPE:
      repr = "object-type";
      break;
    case elf_symbol::FUNC_TYPE:
      repr = "func-type";
      break;
    case elf_symbol::SECTION_TYPE:
      repr = "section-type";
      break;
    case elf_symbol::FILE_TYPE:
      repr = "file-type";
      break;
    case elf_symbol::COMMON_TYPE:
      repr = "common-type";
      break;
    case elf_symbol::TLS_TYPE:
      repr = "tls-type";
      break;
    case elf_symbol::GNU_IFUNC_TYPE:
      repr = "gnu-ifunc-type";
      break;
    default:
      repr = "no-type";
      break;
    }

  o << " type='" << repr << "'";
}

/// Serialize an elf_symbol::binding into an XML element attribute of
/// name 'binding'.
///
/// @param b the elf_symbol::binding to serialize.
///
/// @param o the output stream to serialize the binding to.
static void
write_elf_symbol_binding(elf_symbol::binding b, ostream& o)
{
  string repr;

  switch (b)
    {
    case elf_symbol::LOCAL_BINDING:
      repr = "local-binding";
      break;
    case elf_symbol::GLOBAL_BINDING:
      repr = "global-binding";
      break;
    case elf_symbol::WEAK_BINDING:
      repr = "weak-binding";
      break;
    case elf_symbol::GNU_UNIQUE_BINDING:
      repr = "gnu-unique-binding";
      break;
    default:
      repr = "no-binding";
      break;
    }

  o << " binding='" << repr << "'";
}

/// Write alias attributes for the aliases of a given symbol.
///
/// @param sym the symbol to write the attributes for.
///
/// @param o the output stream to write the attributes to.
///
/// @return true upon successful completion.
static bool
write_elf_symbol_aliases(const elf_symbol& sym, ostream& o)
{
  if (!sym.is_main_symbol() || !sym.has_aliases())
    return false;

  bool emitted = false;
  o << " alias='";
  for (elf_symbol* s = sym.get_next_alias();
       !s->is_main_symbol();
       s = s->get_next_alias())
    {
      if (s->get_next_alias() == s->get_main_symbol())
          o << s->get_id_string() << "'";
      else
          o << s->get_id_string() << ",";

      emitted = true;
    }

  return emitted;
}

/// Write an XML attribute for the reference to a symbol for the
/// current decl.
///
/// @param sym the symbol to consider.
///
/// @param o the output stream to write the attribute to.
///
/// @return true upon successful completion.
static bool
write_elf_symbol_reference(const elf_symbol& sym, ostream& o)
{
  o << " elf-symbol-id='" << sym.get_id_string() << "'";
  return true;
}

/// Write an XML attribute for the reference to a symbol for the
/// current decl.
///
/// @param sym the symbol to consider.
///
/// @param o the output stream to write the attribute to.
///
/// @return true upon successful completion.
static bool
write_elf_symbol_reference(const elf_symbol_sptr sym, ostream& o)
{
  if (!sym)
    return false;

  return write_elf_symbol_reference(*sym, o);
}

/// Serialize the attributes "constructor", "destructor" or "static"
/// if they have true value.
///
/// @param is_ctor if set to true, the "constructor='true'" string is
/// emitted.
///
/// @param is_dtor if set to true the "destructor='true' string is
/// emitted.
///
/// @param is_static if set to true the "static='true'" string is
/// emitted.
///
/// @param o the output stream to use for the serialization.
static void
write_cdtor_const_static(bool is_ctor,
			 bool is_dtor,
			 bool is_const,
			 bool is_static,
			 ostream& o)
{
  if (is_static)
    o << " static='yes'";
  if (is_ctor)
    o << " constructor='yes'";
  else if (is_dtor)
    o << " destructor='yes'";
  if (is_const)
    o << " const='yes'";
}

/// Serialize the attribute "is-declaration-only", if the class has
/// its 'is_declaration_only property set.
///
/// @param klass the pointer to instance of class_decl to consider.
///
/// @param o the output stream to serialize to.
static void
write_class_is_declaration_only(const shared_ptr<class_decl> klass, ostream& o)
{
  if (klass->get_is_declaration_only())
    o << " is-declaration-only='yes'";
}

/// Serialize the attribute "is-struct", if the current instance of
/// class_decl is a struct.
///
/// @param klass a pointer to the instance of class_decl to consider.
///
/// @param o the output stream to serialize to.
static void
write_is_struct(const shared_ptr<class_decl> klass, ostream& o)
{
  if (klass->is_struct())
    o << " is-struct='yes'";
}

/// Serialize a pointer to an of decl_base into an output stream.
///
/// @param decl the pointer to decl_base to serialize
///
/// @param ctxt the context of the serialization.  It contains e.g, the
/// output stream to serialize to.
///
/// @param indent how many indentation spaces to use during the
/// serialization.
///
/// @return true upon successful completion, false otherwise.
static bool
write_decl(const shared_ptr<decl_base>	decl, write_context& ctxt,
	   unsigned indent)
{
  if (write_type_decl(dynamic_pointer_cast<type_decl> (decl),
		      ctxt, indent)
      || write_namespace_decl(dynamic_pointer_cast<namespace_decl>(decl),
			      ctxt, indent)
      || write_qualified_type_def (dynamic_pointer_cast<qualified_type_def>
				   (decl),
				   ctxt, indent)
      || write_pointer_type_def(dynamic_pointer_cast<pointer_type_def>(decl),
				ctxt, indent)
      || write_reference_type_def(dynamic_pointer_cast
				  <reference_type_def>(decl), ctxt, indent)
      || write_array_type_def(dynamic_pointer_cast
			      <array_type_def>(decl), ctxt, indent)
      || write_enum_type_decl(dynamic_pointer_cast<enum_type_decl>(decl),
			      ctxt, indent)
      || write_typedef_decl(dynamic_pointer_cast<typedef_decl>(decl),
			    ctxt, indent)
      || write_var_decl(dynamic_pointer_cast<var_decl>(decl), ctxt,
			/*write_linkage_name=*/true, indent)
      || write_function_decl(dynamic_pointer_cast<class_decl::method_decl>
			     (decl), ctxt, /*skip_first_parameter=*/true,
			     indent)
      || write_function_decl(dynamic_pointer_cast<function_decl>(decl),
			     ctxt, /*skip_first_parameter=*/false, indent)
      || write_class_decl(dynamic_pointer_cast<class_decl>(decl), ctxt, indent)
      || (write_function_tdecl
	  (dynamic_pointer_cast<function_tdecl>(decl), ctxt, indent))
      || (write_class_tdecl
	  (dynamic_pointer_cast<class_tdecl>(decl), ctxt, indent)))
    return true;

  return false;
}

/// Serialize a translation unit to an output stream.
///
/// @param tu the translation unit to serialize.
///
/// @param ctxt the context of the serialization.  It contains e.g,
/// the output stream to serialize to.
///
/// @param indent how many indentation spaces to use during the
/// serialization.
///
/// @return true upon successful completion, false otherwise.
static bool
write_translation_unit(const translation_unit&	tu,
		       write_context&		ctxt,
		       unsigned		indent)
{
  ostream& o = ctxt.get_ostream();
  const config& c = ctxt.get_config();

  do_indent(o, indent);

  o << "<abi-instr version='"
    << static_cast<int> (c.get_format_major_version_number())
    << "." << static_cast<int>(c.get_format_minor_version_number())
    << "'";

  if (tu.get_address_size() != 0)
    o << " address-size='" << static_cast<int>(tu.get_address_size()) << "'";

  if (!tu.get_path().empty())
    o << " path='" << tu.get_path() << "'";

  if (tu.is_empty())
    {
      o << "/>";
      return true;
    }

  o << ">";

  typedef scope_decl::declarations		declarations;
  typedef declarations::const_iterator const_iterator;
  const declarations& d = tu.get_global_scope()->get_member_decls();

  for (const_iterator i = d.begin(); i != d.end(); ++i)
    {
      o << "\n";
      write_decl(*i, ctxt, indent + c.get_xml_element_indent());
    }

  o << "\n";
  do_indent(o, indent);
  o << "</abi-instr>\n";

  return true;
}

/// Serialize a translation unit to an output stream.
///
/// @param tu the translation unit to serialize.
///
/// @param indent how many indentation spaces to use during the
/// serialization.
///
/// @param out the output stream to serialize the translation unit to.
///
/// @return true upon successful completion, false otherwise.
bool
write_translation_unit(const translation_unit&	tu,
		       unsigned		indent,
		       std::ostream&		out)
{
    write_context ctxt(out);
    return write_translation_unit(tu, ctxt, indent);
}

/// Serialize a translation unit to a file.
///
/// @param tu the translation unit to serialize.
///
/// @param indent how many indentation spaces to use during the
/// serialization.
///
/// @param out the file to serialize the translation unit to.
///
/// @return true upon successful completion, false otherwise.
bool
write_translation_unit(const translation_unit&	tu,
		       unsigned		indent,
		       const string&		path)
{
  bool result = true;

  try
    {
      ofstream of(path.c_str(), std::ios_base::trunc);
      if (!of.is_open())
	{
	  cerr << "failed to access " << path << "\n";
	  return false;
	}

      if (!write_translation_unit(tu, indent, of))
	{
	  cerr << "failed to access " << path << "\n";
	  result = false;
	}

      of.close();
    }
  catch(...)
    {
      cerr << "failed to write to " << path << "\n";
      result = false;
    }

  return result;
}


/// Serialize a pointer to an instance of basic type declaration, into
/// an output stream.
///
/// @param d the basic type declaration to serialize.
///
/// @param ctxt the context of the serialization.  It contains e.g, the
/// output stream to serialize to.
///
/// @param indent how many indentation spaces to use during the
/// serialization.
///
/// @return true upon successful completion, false otherwise.
static bool
write_type_decl(const type_decl_sptr d,
		write_context& ctxt,
		unsigned indent)
{
  if (!d)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<type-decl name='" << xml::escape_xml_string(d->get_name()) << "'";

  write_size_and_alignment(d, o);

  write_location(d, o);

  o << " id='" << ctxt.get_id_for_type(d) << "'" <<  "/>";

  return true;
}

/// Serialize a namespace declaration int an output stream.
///
/// @param decl the namespace declaration to serialize.
///
/// @param ctxt the context of the serialization.  It contains e.g, the
/// output stream to serialize to.
///
/// @param indent how many indentation spaces to use during the
/// serialization.
///
/// @return true upon successful completion, false otherwise.
static bool
write_namespace_decl(const shared_ptr<namespace_decl> decl,
		     write_context& ctxt, unsigned indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();
  const config &c = ctxt.get_config();

  do_indent(o, indent);

  o << "<namespace-decl name='" << decl->get_name() << "'>";

  typedef scope_decl::declarations		declarations;
  typedef declarations::const_iterator const_iterator;
  const declarations& d = decl->get_member_decls();

  for (const_iterator i = d.begin(); i != d.end(); ++i)
    {
      o << "\n";
      write_decl(*i, ctxt, indent + c.get_xml_element_indent());
    }

  o << "\n";
  do_indent(o, indent);
  o << "</namespace-decl>";

  return true;
}

/// Serialize a qualified type declaration to an output stream.
///
/// @param decl the qualfied type declaration to write.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the write context.
///
/// @param indent the number of space to indent to during the
/// serialization.
///
/// @return true upon successful completion, false otherwise.
static bool
write_qualified_type_def(const shared_ptr<qualified_type_def>	decl,
			 const string&				id,
			 write_context&			ctxt,
			 unsigned				indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<qualified-type-def type-id='"
    << ctxt.get_id_for_type(decl->get_underlying_type())
    << "'";

  if (decl->get_cv_quals() & qualified_type_def::CV_CONST)
    o << " const='yes'";
  if (decl->get_cv_quals() & qualified_type_def::CV_VOLATILE)
    o << " volatile='yes'";
  if (decl->get_cv_quals() & qualified_type_def::CV_RESTRICT)
    o << " restrict='yes'";

  write_location(static_pointer_cast<decl_base>(decl), o);

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);

  o<< " id='" << i << "'/>";

  return true;
}

/// Serialize a qualified type declaration to an output stream.
///
/// @param decl the qualfied type declaration to write.
///
/// @param ctxt the write context.
///
/// @param indent the number of space to indent to during the
/// serialization.
///
/// @return true upon successful completion, false otherwise.
static bool
write_qualified_type_def(const shared_ptr<qualified_type_def>	decl,
			 write_context&			ctxt,
			 unsigned				indent)
{return write_qualified_type_def(decl, "", ctxt, indent);}

/// Serialize a pointer to an instance of pointer_type_def.
///
/// @param decl the pointer_type_def to serialize.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_pointer_type_def(const shared_ptr<pointer_type_def> decl,
		       const string& id,
		       write_context& ctxt,
		       unsigned indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<pointer-type-def type-id='"
    << ctxt.get_id_for_type(decl->get_pointed_to_type())
    << "'";

  write_size_and_alignment(decl, o);

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);

  o << " id='" << i << "'";

  write_location(static_pointer_cast<decl_base>(decl), o);
  o << "/>";

  return true;
}

/// Serialize a pointer to an instance of pointer_type_def.
///
/// @param decl the pointer_type_def to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_pointer_type_def(const shared_ptr<pointer_type_def> decl,
		       write_context& ctxt,
		       unsigned indent)
{return write_pointer_type_def(decl, "", ctxt, indent);}

/// Serialize a pointer to an instance of reference_type_def.
///
/// @param decl the reference_type_def to serialize.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_reference_type_def(const shared_ptr<reference_type_def>	decl,
			 const string&				id,
			 write_context&			ctxt,
			 unsigned				indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<reference-type-def kind='";
  if (decl->is_lvalue())
    o << "lvalue";
  else
    o << "rvalue";
  o << "'";

  o << " type-id='" << ctxt.get_id_for_type(decl->get_pointed_to_type()) << "'";

  write_size_and_alignment(decl, o);

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);
  o << " id='" << i << "'";

  write_location(static_pointer_cast<decl_base>(decl), o);

  o << "/>";
  return true;
}

/// Serialize a pointer to an instance of reference_type_def.
///
/// @param decl the reference_type_def to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_reference_type_def(const shared_ptr<reference_type_def>	decl,
			 write_context&			ctxt,
			 unsigned				indent)
{return write_reference_type_def(decl, "", ctxt, indent);}

/// Serialize a pointer to an instance of array_type_def.
///
/// @param decl the array_type_def to serialize.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_array_type_def(const shared_ptr<array_type_def>	decl,
		     const string&			id,
		     write_context&			ctxt,
		     unsigned				indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent(o, indent);
  o << "<array-type-def";

  o << " dimensions='" << decl->get_dimension_count() << "'";

  o << " type-id='" << ctxt.get_id_for_type(decl->get_element_type()) << "'";

  write_array_size_and_alignment(decl, o);

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);
  o << " id='" << i << "'";

  write_location(static_pointer_cast<decl_base>(decl), o);

  if (!decl->get_dimension_count())
    o << "/>";
  else
    {
      o << ">\n";

      vector<array_type_def::subrange_sptr>::const_iterator si;

      for (si = decl->get_subranges().begin();
           si != decl->get_subranges().end(); ++si)
        {
          do_indent(o, indent + ctxt.get_config().get_xml_element_indent());

          o << "<subrange length='";
          if ((*si)->is_infinite())
          o << "infinite";
          else
            {
              o << (*si)->get_length();
            }
          o << "'";

          write_location((*si)->get_location(), *get_translation_unit(decl), o);

          o << "/>\n";
        }

      do_indent(o, indent);
      o << "</array-type-def>";
    }

  return true;
}

/// Serialize a pointer to an instance of array_type_def.
///
/// @param decl the array_type_def to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_array_type_def(const shared_ptr<array_type_def>	decl,
		     write_context&			ctxt,
		     unsigned				indent)
{return write_array_type_def(decl, "", ctxt, indent);}

/// Serialize a pointer to an instance of enum_type_decl.
///
/// @param decl the enum_type_decl to serialize.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_enum_type_decl(const shared_ptr<enum_type_decl>	decl,
		     const string&			id,
		     write_context&			ctxt,
		     unsigned				indent)
{
    if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent(o, indent);
  o << "<enum-decl name='" << xml::escape_xml_string(decl->get_name()) << "'";

  write_location(decl, o);

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);
  o << " id='" << i << "'>\n";

  do_indent(o, indent + ctxt.get_config().get_xml_element_indent());
  o << "<underlying-type type-id='"
    << ctxt.get_id_for_type(decl->get_underlying_type())
    << "'/>\n";

  for (enum_type_decl::enumerators::const_iterator i =
	 decl->get_enumerators().begin();
       i != decl->get_enumerators().end();
       ++i)
    {
      do_indent(o, indent + ctxt.get_config().get_xml_element_indent());
      o << "<enumerator name='"
	<< i->get_name()
	<< "' value='"
	<< i->get_value()
	<< "'/>\n";
    }

  do_indent(o, indent);
  o << "</enum-decl>";

  return true;
}

/// Serialize a pointer to an instance of enum_type_decl.
///
/// @param decl the enum_type_decl to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_enum_type_decl(const shared_ptr<enum_type_decl>	decl,
		     write_context&			ctxt,
		     unsigned				indent)
{return write_enum_type_decl(decl, "", ctxt, indent);}

/// Serialize an @ref elf_symbol to an XML element of name
/// 'elf-symbol'.
///
/// @param sym the elf symbol to serialize.
///
/// @param ctxt the read context to use.
///
/// @param indent the number of white spaces to use as indentation.
///
/// @return true iff the function completed successfully.
static bool
write_elf_symbol(const shared_ptr<elf_symbol>	sym,
		 write_context&		ctxt,
		 unsigned			indent)
{
  if (!sym)
    return false;

  ostream &o = ctxt.get_ostream();

  do_indent(o, indent);
  o << "<elf-symbol name='" << sym->get_name() << "'";
  if (!sym->get_version().is_empty())
    {
      o << " version='" << sym->get_version().str() << "'";
      o << " is-default-version='";
      if (sym->get_version().is_default())
	o <<  "yes";
      else
	o << "no";
      o << "'";
    }

  write_elf_symbol_type(sym->get_type(), o);

  write_elf_symbol_binding(sym->get_binding(), o);

  write_elf_symbol_aliases(*sym, o);

  o << " is-defined='";
  if (sym->is_defined())
    o << "yes";
  else
    o << "no";
  o << "'";

  o << "/>";

  return true;
}

/// Write the elf symbol database to the output associated to the
/// current context.
///
/// @param syms the sorted elf symbol data to write out.
///
/// @param ctxt the context to consider.
///
/// @param indent the number of white spaces to use as indentation.
///
/// @return true upon successful completion.
static bool
write_elf_symbols_table(const elf_symbols&	syms,
			write_context&		ctxt,
			unsigned		indent)
{
  if (syms.empty())
    return false;

  ostream& o = ctxt.get_ostream();

  unordered_map<string, bool> emitted_syms;
  for (elf_symbols::const_iterator it = syms.begin(); it != syms.end(); ++it)
    {
      write_elf_symbol(*it, ctxt, indent);
      o << "\n";
    }

  return true;
}

/// Write a vector of dependency names for the current corpus we are
/// writting.
///
/// @param needed the vector of dependency names to write.
///
/// @param ctxt the write context to use for the writting.
///
/// @param indent the number of indendation spaces to use.
///
/// @return true upon successful completion, false otherwise.
static bool
write_elf_needed(const vector<string>&	needed,
		 write_context&	ctxt,
		 unsigned		indent)
{
  if (needed.empty())
    return false;

  ostream& o = ctxt.get_ostream();

  for (vector<string>::const_iterator i = needed.begin();
       i != needed.end();
       ++i)
    {
      if (i != needed.begin())
	o << "\n";
      do_indent(o, indent);
      o << "<dependency name='" << *i << "'/>";
    }
  return true;
}

/// Serialize a pointer to an instance of typedef_decl.
///
/// @param decl the typedef_decl to serialize.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_typedef_decl(const shared_ptr<typedef_decl>	decl,
		   const string&			id,
		   write_context&			ctxt,
		   unsigned				indent)
{
  if (!decl)
    return false;

  ostream &o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<typedef-decl name='" << decl->get_name() << "'";

  o << " type-id='" << ctxt.get_id_for_type(decl->get_underlying_type()) << "'";

  write_location(decl, o);

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);

  o << " id='" << i << "'/>";

  return true;
}

/// Serialize a pointer to an instance of typedef_decl.
///
/// @param decl the typedef_decl to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_typedef_decl(const shared_ptr<typedef_decl>	decl,
		   write_context&			ctxt,
		   unsigned				indent)
{return write_typedef_decl(decl, "", ctxt, indent);}

/// Serialize a pointer to an instances of var_decl.
///
/// @param decl the var_decl to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param write_linkage_name if true, serialize the mangled name of
/// this variable.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_var_decl(const shared_ptr<var_decl> decl, write_context& ctxt,
	       bool write_linkage_name, unsigned indent)
{
  if (!decl)
    return false;

  ostream &o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<var-decl name='" << decl->get_name() << "'";
  o << " type-id='" << ctxt.get_id_for_type(decl->get_type()) << "'";

  if (write_linkage_name)
    {
      const string& linkage_name = decl->get_linkage_name();
      if (!linkage_name.empty())
	o << " mangled-name='" << linkage_name << "'";
    }

  write_visibility(decl, o);

  write_binding(decl, o);

  write_location(decl, o);

  write_elf_symbol_reference(decl->get_symbol(), o);

  o << "/>";

  return true;
}

/// Serialize a pointer to a function_decl.
///
/// @param decl the pointer to function_decl to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param skip_first_parm if true, do not serialize the first
/// parameter of the function decl.
///
/// @param indent the number of indentation white spaces to use.
///
/// @return true upon succesful completion, false otherwise.
static bool
write_function_decl(const shared_ptr<function_decl> decl, write_context& ctxt,
		    bool skip_first_parm, unsigned indent)
{
  if (!decl)
    return false;

  ostream &o = ctxt.get_ostream();

  do_indent(o, indent);

  o << "<function-decl name='"
    << xml::escape_xml_string(decl->get_name())
    << "'";

  if (!decl->get_linkage_name().empty())
    o << " mangled-name='"
      << xml::escape_xml_string(decl->get_linkage_name()) << "'";

  write_location(decl, o);

  if (decl->is_declared_inline())
    o << " declared-inline='yes'";

  write_visibility(decl, o);

  write_binding(decl, o);

  write_size_and_alignment(decl->get_type(), o);

  write_elf_symbol_reference(decl->get_symbol(), o);

  o << ">\n";

  vector<shared_ptr<function_decl::parameter> >::const_iterator pi =
    decl->get_parameters().begin();
  for ((skip_first_parm && pi != decl->get_parameters().end()) ? ++pi: pi;
       pi != decl->get_parameters().end();
       ++pi)
    {
      do_indent(o, indent + ctxt.get_config().get_xml_element_indent());
      if ((*pi)->get_variadic_marker())
	o << "<parameter is-variadic='yes'";
      else
	{
	  o << "<parameter type-id='"
	    << ctxt.get_id_for_type((*pi)->get_type())
	    << "'";

	  if (!(*pi)->get_name().empty())
	    o << " name='" << (*pi)->get_name() << "'";
	}
      if ((*pi)->get_artificial())
	  o << " is-artificial='yes'";
      write_location((*pi)->get_location(), *get_translation_unit(decl), o);
      o << "/>\n";
    }

  if (shared_ptr<type_base> return_type = decl->get_return_type())
    {
      do_indent(o, indent + ctxt.get_config().get_xml_element_indent());
      o << "<return type-id='" << ctxt.get_id_for_type(return_type) << "'/>\n";
    }

  do_indent(o, indent);
  o << "</function-decl>";

  return true;
}

/// Serialize a class_decl type.
///
/// @param decl the pointer to class_decl to serialize.
///
/// @param id the type id identitifier to use in the serialized
/// output.  If this is empty, the function will compute an
/// appropriate one.  This is useful when this function is called to
/// serialize the underlying type of a member type; in that case, the
/// caller has already computed the id of the *member type*, and that
/// id is the one to be written as the value of the 'id' attribute of
/// the XML element of the underlying type.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the initial indentation to use.
static bool
write_class_decl(const shared_ptr<class_decl>	decl,
		 const string&			id,
		 write_context&		ctxt,
		 unsigned			indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent_to_level(ctxt, indent, 0);

  o << "<class-decl name='" << xml::escape_xml_string(decl->get_name()) << "'";

  write_size_and_alignment(decl, o);

  write_is_struct(decl, o);

  write_visibility(decl, o);

  write_location(decl, o);

  write_class_is_declaration_only(decl, o);

  if (decl->get_earlier_declaration())
    {
      // This instance is the definition of an earlier declaration.
      o << " def-of-decl-id='"
	<< ctxt.get_id_for_type(is_type(decl->get_earlier_declaration()))
	<< "'";
    }

  string i = id;
  if (i.empty())
    i = ctxt.get_id_for_type(decl);
  o << " id='" << i << "'";
  if (decl->has_no_base_nor_member())
    o << "/>";
  else
    {
      o << ">\n";

      unsigned nb_ws = get_indent_to_level(ctxt, indent, 1);
      for (class_decl::base_specs::const_iterator base =
	     decl->get_base_specifiers().begin();
	   base != decl->get_base_specifiers().end();
	   ++base)
	{
	  do_indent(o, nb_ws);
	  o << "<base-class";

	  write_access((*base)->get_access_specifier(), o);

	  write_layout_offset (*base, o);

	  if ((*base)->get_is_virtual ())
	    o << " is-virtual='yes'";

	  o << " type-id='"
	    << ctxt.get_id_for_type((*base)->get_base_class())
	    << "'/>\n";
	}

      for (class_decl::member_types::const_iterator ti =
	     decl->get_member_types().begin();
	   ti != decl->get_member_types().end();
	   ++ti)
	write_member_type(*ti, ctxt, nb_ws);

      for (class_decl::data_members::const_iterator data =
	     decl->get_data_members().begin();
	   data != decl->get_data_members().end();
	   ++data)
	{
	  do_indent(o, nb_ws);
	  o << "<data-member";
	  write_access(get_member_access_specifier(*data), o);

	  bool is_static = get_member_is_static(*data);
	  write_cdtor_const_static(/*is_ctor=*/false,
				   /*is_dtor=*/false,
				   /*is_const=*/false,
				   /*is_static=*/is_static,
				   o);
	  write_layout_offset(*data, o);
	  o << ">\n";

	  write_var_decl(*data, ctxt, is_static,
			 get_indent_to_level(ctxt, indent, 2));
	  o << "\n";

	  do_indent_to_level(ctxt, indent, 1);
	  o << "</data-member>\n";
	}

      for (class_decl::member_functions::const_iterator f =
	     decl->get_member_functions().begin();
	   f != decl->get_member_functions().end();
	   ++f)
	{
	  function_decl_sptr fn = *f;
	  if (get_member_function_is_virtual(fn))
	    continue;

	  assert(!get_member_function_is_virtual(fn));

	  do_indent(o, nb_ws);
	  o << "<member-function";
	  write_access(get_member_access_specifier(fn), o);
	  write_cdtor_const_static( get_member_function_is_ctor(fn),
				    get_member_function_is_dtor(fn),
				    get_member_function_is_const(fn),
				    get_member_is_static(fn),
				    o);
	  o << ">\n";

	  write_function_decl(fn, ctxt,
			      /*skip_first_parameter=*/false,
			      get_indent_to_level(ctxt, indent, 2));
	  o << "\n";

	  do_indent_to_level(ctxt, indent, 1);
	  o << "</member-function>\n";
	}

      for (class_decl::member_functions::const_iterator f =
	     decl->get_virtual_mem_fns().begin();
	   f != decl->get_virtual_mem_fns().end();
	   ++f)
	{
	  function_decl_sptr fn = *f;

	  assert(get_member_function_is_virtual(fn));

	  do_indent(o, nb_ws);
	  o << "<member-function";
	  write_access(get_member_access_specifier(fn), o);
	  write_cdtor_const_static( get_member_function_is_ctor(fn),
				    get_member_function_is_dtor(fn),
				    get_member_function_is_const(fn),
				    get_member_is_static(fn),
				    o);
	  write_voffset(fn, o);
	  o << ">\n";

	  write_function_decl(fn, ctxt,
			      /*skip_first_parameter=*/false,
			      get_indent_to_level(ctxt, indent, 2));
	  o << "\n";

	  do_indent_to_level(ctxt, indent, 1);
	  o << "</member-function>\n";
	}

      for (class_decl::member_function_templates::const_iterator fn =
	     decl->get_member_function_templates().begin();
	   fn != decl->get_member_function_templates().end();
	   ++fn)
	{
	  do_indent(o, nb_ws);
	  o << "<member-template";
	  write_access((*fn)->get_access_specifier(), o);
	  write_cdtor_const_static((*fn)->is_constructor(),
				   /*is_dtor=*/false,
				   (*fn)->is_const(),
				   (*fn)->get_is_static(), o);
	  o << ">\n";
	  write_function_tdecl((*fn)->as_function_tdecl(), ctxt,
				       get_indent_to_level(ctxt, indent, 2));
	  o << "\n";
	  do_indent(o, nb_ws);
	  o << "</member-template>\n";
	}

      for (class_decl::member_class_templates::const_iterator cl =
	     decl->get_member_class_templates().begin();
	   cl != decl->get_member_class_templates().end();
	   ++cl)
	{
	  do_indent(o, nb_ws);
	  o << "<member-template";
	  write_access((*cl)->get_access_specifier(), o);
	  write_cdtor_const_static(false, false, false,
				   (*cl)->get_is_static(), o);
	  o << ">\n";
	  write_class_tdecl((*cl)->as_class_tdecl(), ctxt,
				    get_indent_to_level(ctxt, indent, 2));
	  o << "\n";
	  do_indent(o, nb_ws);
	  o << "</member-template>\n";
	}

      do_indent_to_level(ctxt, indent, 0);

      o << "</class-decl>";
    }

  return true;
}

/// Serialize a class_decl type.
///
/// @param decl the pointer to class_decl to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the initial indentation to use.
static bool
write_class_decl(const shared_ptr<class_decl>	decl,
		 write_context&		ctxt,
		 unsigned			indent)
{return write_class_decl(decl, "", ctxt, indent);}

/// Serialize a member type.
///
/// Note that the id written as the value of the 'id' attribute of the
/// underlying type is actually the id of the member type, not the one
/// for the underying type.  That id takes in account, the access
/// specifier and the qualified name of the member type.
///
/// @param decl the declaration of the member type to serialize.
///
/// @param ctxt the write context to use.
///
/// @param indent the number of levels to use for indentation
static bool
write_member_type(const type_base_sptr t,
		  write_context& ctxt, unsigned indent)
{
  if (!t)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent_to_level(ctxt, indent, 0);

  decl_base_sptr decl = get_type_declaration(t);
  assert(decl);

  o << "<member-type";
  write_access(decl, o);
  o << ">\n";

  string id = ctxt.get_id_for_type(t);

  unsigned nb_ws = get_indent_to_level(ctxt, indent, 1);
  assert(write_qualified_type_def(dynamic_pointer_cast<qualified_type_def>(t),
				  id, ctxt, nb_ws)
	 || write_pointer_type_def(dynamic_pointer_cast<pointer_type_def>(t),
				   id, ctxt, nb_ws)
	 || write_reference_type_def(dynamic_pointer_cast<reference_type_def>(t),
				     id, ctxt, nb_ws)
	 || write_array_type_def(dynamic_pointer_cast<array_type_def>(t),
			         id, ctxt, nb_ws)
	 || write_enum_type_decl(dynamic_pointer_cast<enum_type_decl>(t),
				 id, ctxt, nb_ws)
	 || write_typedef_decl(dynamic_pointer_cast<typedef_decl>(t),
			    id, ctxt, nb_ws)
	 || write_class_decl(dynamic_pointer_cast<class_decl>(t),
			     id, ctxt, nb_ws));
  o << "\n";

  do_indent_to_level(ctxt, indent, 0);
  o << "</member-type>\n";

  return true;
}

/// Serialize an instance of type_tparameter.
///
/// @param decl the instance to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the initial indentation to use.
///
/// @return true upon successful completion, false otherwise.
static bool
write_type_tparameter(const shared_ptr<type_tparameter>	decl,
			      write_context&  ctxt, unsigned indent)
{
  if (!decl)
    return false;

  ostream &o = ctxt.get_ostream();
  do_indent_to_level(ctxt, indent, 0);

  string id_attr_name;
  if (ctxt.type_has_existing_id(decl))
    id_attr_name = "type-id";
  else
    id_attr_name = "id";

  o << "<template-type-parameter "
    << id_attr_name << "='" <<  ctxt.get_id_for_type(decl) << "'";

  std::string name = xml::escape_xml_string(decl->get_name ());
  if (!name.empty())
    o << " name='" << name << "'";

  write_location(decl, o);

  o << "/>";

  return true;
}

/// Serialize an instance of non_type_tparameter.
///
/// @param decl the instance to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the intial indentation to use.
///
/// @return true open successful completion, false otherwise.
static bool
write_non_type_tparameter(
 const shared_ptr<non_type_tparameter>	decl,
 write_context&	ctxt, unsigned indent)
{
  if (!decl)
    return false;

  ostream &o = ctxt.get_ostream();
  do_indent_to_level(ctxt, indent, 0);

  o << "<template-non-type-parameter type-id='"
    << ctxt.get_id_for_type(decl->get_type())
    << "'";

  string name = xml::escape_xml_string(decl->get_name());
  if (!name.empty())
    o << " name='" << name << "'";

  write_location(decl, o);

  o << "/>";

  return true;
}

/// Serialize an instance of template template parameter.
///
/// @param decl the instance to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the initial indentation to use.
///
/// @return true upon successful completion, false otherwise.

static bool
write_template_tparameter (const template_tparameter_sptr	decl,
			   write_context&			ctxt,
			   unsigned				indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();
  do_indent_to_level(ctxt, indent, 0);

  string id_attr_name = "id";
  if (ctxt.type_has_existing_id(decl))
    id_attr_name = "type-id";

  o << "<template-template-parameter " << id_attr_name << "='"
    << ctxt.get_id_for_type(decl) << "'";

  string name = xml::escape_xml_string(decl->get_name());
  if (!name.empty())
    o << " name='" << name << "'";

  o << ">\n";

  unsigned nb_spaces = get_indent_to_level(ctxt, indent, 1);
  for (list<shared_ptr<template_parameter> >::const_iterator p =
	 decl->get_template_parameters().begin();
       p != decl->get_template_parameters().end();
       ++p)
    {
      write_template_parameter(decl, ctxt, nb_spaces);
      o <<"\n";
    }

  do_indent_to_level(ctxt, indent, 0);
  o << "</template-template-parameter>";

  return true;
}

/// Serialize an instance of type_composition.
///
/// @param decl the decl to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the initial indentation to use.
///
/// @return true upon successful completion, false otherwise.
static bool
write_type_composition
(const shared_ptr<type_composition> decl,
 write_context& ctxt, unsigned indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent_to_level(ctxt, indent, 0);

  o << "<template-parameter-type-composition>\n";

  unsigned nb_spaces = get_indent_to_level(ctxt, indent, 1);
  (write_pointer_type_def
   (dynamic_pointer_cast<pointer_type_def>(decl->get_composed_type()),
			  ctxt, nb_spaces)
   || write_reference_type_def
   (dynamic_pointer_cast<reference_type_def>(decl->get_composed_type()),
    ctxt, nb_spaces)
   || write_array_type_def
   (dynamic_pointer_cast<array_type_def>(decl->get_composed_type()),
    ctxt, nb_spaces)
   || write_qualified_type_def
   (dynamic_pointer_cast<qualified_type_def>(decl->get_composed_type()),
    ctxt, nb_spaces));

  o << "\n";

  do_indent_to_level(ctxt, indent, 0);
  o << "</template-parameter-type-composition>";

  return true;
}

/// Serialize an instance of template_parameter.
///
/// @param decl the instance to serialize.
///
/// @param ctxt the context of the serialization.
///
/// @param indent the initial indentation to use.
///
/// @return true upon successful completion, false otherwise.
static bool
write_template_parameter(const shared_ptr<template_parameter> decl,
			 write_context& ctxt, unsigned indent)
{
  if ((!write_type_tparameter
       (dynamic_pointer_cast<type_tparameter>(decl), ctxt, indent))
      && (!write_non_type_tparameter
	  (dynamic_pointer_cast<non_type_tparameter>(decl),
	   ctxt, indent))
      && (!write_template_tparameter
	  (dynamic_pointer_cast<template_tparameter>(decl),
	   ctxt, indent))
      && (!write_type_composition
	  (dynamic_pointer_cast<type_composition>(decl),
	   ctxt, indent)))
    return false;

  return true;
}

/// Serialize the template parameters of the a given template.
///
/// @param tmpl the template for which to emit the template parameters.
static void
write_template_parameters(const shared_ptr<template_decl> tmpl,
			  write_context& ctxt, unsigned indent)
{
  if (!tmpl)
    return;

  ostream &o = ctxt.get_ostream();

  unsigned nb_spaces = get_indent_to_level(ctxt, indent, 1);
  for (list<shared_ptr<template_parameter> >::const_iterator p =
	 tmpl->get_template_parameters().begin();
       p != tmpl->get_template_parameters().end();
       ++p)
    {
      write_template_parameter(*p, ctxt, nb_spaces);
      o << "\n";
    }
}

/// Serialize an instance of function_tdecl.
///
/// @param decl the instance to serialize.
///
/// @param ctxt the context of the serialization
///
/// @param indent the initial indentation.
static bool
write_function_tdecl(const shared_ptr<function_tdecl> decl,
		     write_context& ctxt, unsigned indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent_to_level(ctxt, indent, 0);

  o << "<function-template-decl id='" << ctxt.get_id_for_fn_tmpl(decl) << "'";

  write_location(decl, o);

  write_visibility(decl, o);

  write_binding(decl, o);

  o << ">\n";

  write_template_parameters(decl, ctxt, indent);

  write_function_decl(decl->get_pattern(), ctxt,
		      /*skip_first_parameter=*/false,
		      get_indent_to_level(ctxt, indent, 1));
  o << "\n";

  do_indent_to_level(ctxt, indent, 0);

  o << "</function-template-decl>";

  return true;
}


/// Serialize an instance of class_tdecl
///
/// @param decl a pointer to the instance of class_tdecl to serialize.
///
/// @param ctxt the context of the serializtion.
///
/// @param indent the initial number of white space to use for
/// indentation.
///
/// @return true upon successful completion, false otherwise.
static bool
write_class_tdecl(const shared_ptr<class_tdecl> decl,
		  write_context& ctxt, unsigned indent)
{
  if (!decl)
    return false;

  ostream& o = ctxt.get_ostream();

  do_indent_to_level(ctxt, indent, 0);

  o << "<class-template-decl id='" << ctxt.get_id_for_class_tmpl(decl) << "'";

  write_location(decl, o);

  write_visibility(decl, o);

  o << ">\n";

  write_template_parameters(decl, ctxt, indent);

  write_class_decl(decl->get_pattern(), ctxt,
		   get_indent_to_level(ctxt, indent, 1));
  o << "\n";

  do_indent_to_level(ctxt, indent, 0);

  o << "</class-template-decl>";

  return true;
}

#ifdef WITH_ZIP_ARCHIVE

/// A context used by functions that write a corpus out to disk in a
/// ZIP archive of ABI Instrumentation XML files.
///
/// The aim of this context file is to hold the buffers of data that
/// are to be written into a given zip object, until the zip object is
/// closed.  It's at that point that the buffers data is really
/// flushed into the zip archive.
///
/// When an instance of this context type is created for a given zip
/// object, is created, its life time should be longer than the @ref
/// zip_sptr object it holds.
///
/// The definition of this type is private and should remain hidden
/// from client code.
struct archive_write_ctxt
{
  list<string> serialized_tus;
  zip_sptr archive;

  archive_write_ctxt(zip_sptr ar)
    : archive(ar)
  {}
};
typedef shared_ptr<archive_write_ctxt> archive_write_ctxt_sptr;

/// Create a write context to a given archive.  The result of this
/// function is to be passed to the functions that are to write a
/// corpus to an archive, e.g, write_corpus_to_archive().
///
/// @param archive_path the path to the archive to create this write
/// context for.
///
/// @return the resulting write context to pass to the functions that
/// are to write a corpus to @ref archive_path.
static archive_write_ctxt_sptr
create_archive_write_context(const string& archive_path)
{
  if (archive_path.empty())
    return archive_write_ctxt_sptr();

  int error_code = 0;
  zip_sptr archive = open_archive(archive_path,
				  ZIP_CREATE|ZIP_TRUNCATE|ZIP_CHECKCONS,
				  &error_code);
  if (error_code)
    return archive_write_ctxt_sptr();

  archive_write_ctxt_sptr r(new archive_write_ctxt(archive));
  return r;
}

/// Write a translation unit to an on-disk archive.  The archive is a
/// zip archive of ABI Instrumentation files in XML format.
///
/// @param tu the translation unit to serialize.
///
/// @param ctxt the context of the serialization.  Contains
/// information about where the archive is on disk, the zip archive,
/// and the buffers holding the temporary data to be flushed into the archive.
///
/// @return true upon succesful serialization occured, false
/// otherwise.
static bool
write_translation_unit_to_archive(const translation_unit& tu,
				  archive_write_ctxt& ctxt)
{
  if (!ctxt.archive)
    return false;

  ostringstream os;
  if (!write_translation_unit(tu, /*indent=*/0, os))
    return false;
  ctxt.serialized_tus.push_back(os.str());

  zip_source *source;
  if ((source = zip_source_buffer(ctxt.archive.get(),
				  ctxt.serialized_tus.back().c_str(),
				  ctxt.serialized_tus.back().size(),
				  false)) == 0)
    return false;

  if (zip_file_add(ctxt.archive.get(), tu.get_path().c_str(), source,
		   ZIP_FL_OVERWRITE|ZIP_FL_ENC_GUESS) < 0)
    {
      zip_source_free(source);
      return false;
    }

  return true;
}

 /// Serialize a given corpus to disk in a file at a given path.
 ///
 /// @param tu the translation unit to serialize.
 ///
 /// @param ctxt the context of the serialization.  Contains
 /// information about where the archive is on disk, the zip archive
 /// object, and the buffers holding the temporary data to be flushed
 /// into the archive.
 ///
 /// @return true upon successful completion, false otherwise.
static bool
write_corpus_to_archive(const corpus& corp,
			archive_write_ctxt& ctxt)
{
  for (translation_units::const_iterator i =
	 corp.get_translation_units().begin();
       i != corp.get_translation_units().end();
       ++i)
    {
      if (! write_translation_unit_to_archive(**i, ctxt))
	return false;
    }

  // TODO: ensure abi-info descriptor is added to the archive.
  return true;
}

/// Serialize a given corpus to disk in an archive file at a given
/// path.
///
/// @param corp the ABI corpus to serialize.
///
 /// @param ctxt the context of the serialization.  Contains
 /// information about where the archive is on disk, the zip archive
 /// object, and the buffers holding the temporary data to be flushed
 /// into the archive.
 ///
 /// @return upon successful completion, false otherwise.
static bool
write_corpus_to_archive(const corpus& corp,
			archive_write_ctxt_sptr ctxt)
{return write_corpus_to_archive(corp, *ctxt);}

 /// Serialize the current corpus to disk in a file at a given path.
 ///
 /// @param tu the translation unit to serialize.
 ///
 /// @param path the path of the file to serialize the
 /// translation_unit to.
 ///
 /// @return true upon successful completion, false otherwise.
bool
write_corpus_to_archive(const corpus& corp,
			const string& path)
{
  archive_write_ctxt_sptr ctxt = create_archive_write_context(path);
  assert(ctxt);
  return write_corpus_to_archive(corp, ctxt);
}

 /// Serialize the current corpus to disk in a file.  The file path is
 /// given by translation_unit::get_path().
 ///
 /// @param tu the translation unit to serialize.
 ///
 /// @return true upon successful completion, false otherwise.
bool
write_corpus_to_archive(const corpus& corp)
{return write_corpus_to_archive(corp, corp.get_path());}

 /// Serialize the current corpus to disk in a file.  The file path is
 /// given by translation_unit::get_path().
 ///
 /// @param tu the translation unit to serialize.
 ///
 /// @return true upon successful completion, false otherwise.
bool
write_corpus_to_archive(const corpus_sptr corp)
{return write_corpus_to_archive(*corp);}

#endif //WITH_ZIP_ARCHIVE

/// Serialize an ABI corpus to a single native xml document.  The root
/// note of the resulting XML document is 'abi-corpus'.
///
/// @param corpus the corpus to serialize.
///
/// @param indent the number of white space indentation to use.
///
/// @param out the output stream to serialize the ABI corpus to.
bool
write_corpus_to_native_xml(const corpus_sptr	corpus,
			   unsigned		indent,
			   std::ostream&	out)
{
  if (!corpus)
    return false;

  write_context ctxt(out);

  do_indent_to_level(ctxt, indent, 0);
  out << "<abi-corpus";
  if (!corpus->get_path().empty())
    out << " path='" << corpus->get_path() << "'";

  if (!corpus->get_architecture_name().empty())
    out << " architecture='" << corpus->get_architecture_name()<< "'";

  if (!corpus->get_soname().empty())
    out << " soname='" << corpus->get_soname()<< "'";

  if (corpus->is_empty())
    {
      out << "/>\n";
      return true;
    }

  out << ">\n";

  // Write the list of needed corpora
  if (!corpus->get_needed().empty())
    {
      do_indent_to_level(ctxt, indent, 1);
      out << "<elf-needed>\n";
      write_elf_needed(corpus->get_needed(), ctxt,
		       get_indent_to_level(ctxt, indent, 2));
      out << "\n";
      do_indent_to_level(ctxt, indent, 1);
      out << "</elf-needed>\n";
    }

  // Write the function symbols data base.
  if (!corpus->get_fun_symbol_map().empty())
    {
      do_indent_to_level(ctxt, indent, 1);
      out << "<elf-function-symbols>\n";

      write_elf_symbols_table(corpus->get_sorted_fun_symbols(), ctxt,
			      get_indent_to_level(ctxt, indent, 2));

      do_indent_to_level(ctxt, indent, 1);
      out << "</elf-function-symbols>\n";
    }

  // Write the variable symbols data base.
  if (!corpus->get_var_symbol_map().empty())
    {
      do_indent_to_level(ctxt, indent, 1);
      out << "<elf-variable-symbols>\n";

      write_elf_symbols_table(corpus->get_sorted_var_symbols(), ctxt,
			      get_indent_to_level(ctxt, indent, 2));

      do_indent_to_level(ctxt, indent, 1);
      out << "</elf-variable-symbols>\n";
    }

  // Now write the translation units.
  for (translation_units::const_iterator i =
	 corpus->get_translation_units().begin();
       i != corpus->get_translation_units().end();
       ++i)
    write_translation_unit(**i, ctxt, get_indent_to_level(ctxt, indent, 1));

  out << "</abi-corpus>\n";

  return true;
}

/// Serialize an ABI corpus to a single native xml document.  The root
/// note of the resulting XML document is 'abi-corpus'.
///
/// @param corpus the corpus to serialize.
///
/// @param indent the number of white space indentation to use.
///
/// @param out the output file to serialize the ABI corpus to.
bool
write_corpus_to_native_xml_file(const corpus_sptr	corpus,
				unsigned		indent,
				const string&		path)
{
    bool result = true;

  try
    {
      ofstream of(path.c_str(), std::ios_base::trunc);
      if (!of.is_open())
	{
	  cerr << "failed to access " << path << "\n";
	  return false;
	}

      if (!write_corpus_to_native_xml(corpus, indent, of))
	{
	  cerr << "failed to access " << path << "\n";
	  result = false;
	}

      of.close();
    }
  catch(...)
    {
      cerr << "failed to write to " << path << "\n";
      result = false;
    }

  return result;
}

} //end namespace xml_writer

// <Debugging routines>

using namespace abigail::ir;

/// Serialize a pointer to decl_base to an output stream.
///
/// @param d the pointer to decl_base to serialize.
///
/// @param o the output stream to consider.
void
dump(const decl_base_sptr d, std::ostream& o)
{
  xml_writer::write_context ctxt(o);
  write_decl(d, ctxt, /*indent=*/0);
  o << "\n";
}

/// Serialize a pointer to decl_base to stderr.
///
/// @param d the pointer to decl_base to serialize.
void
dump(const decl_base_sptr d)
{dump(d, cerr);}

/// Serialize a pointer to type_base to an output stream.
///
/// @param t the pointer to type_base to serialize.
///
/// @param o the output stream to serialize the @ref type_base to.
void
dump(const type_base_sptr t, std::ostream& o)
{dump(get_type_declaration(t), o);}

/// Serialize a pointer to type_base to stderr.
///
/// @param t the pointer to type_base to serialize.
void
dump(const type_base_sptr t)
{dump(t, cerr);}

/// Serialize a pointer to var_decl to an output stream.
///
/// @param v the pointer to var_decl to serialize.
///
/// @param o the output stream to serialize the @ref var_decl to.
void
dump(const var_decl_sptr v, std::ostream& o)
{
  xml_writer::write_context ctxt(o);
  write_var_decl(v, ctxt, /*linkage_name*/true, /*indent=*/0);
  cerr << "\n";
}

/// Serialize a pointer to var_decl to stderr.
///
/// @param v the pointer to var_decl to serialize.
void
dump(const var_decl_sptr v)
{dump(v, cerr);}

/// Serialize a @ref translation_unit to an output stream.
///
/// @param t the translation_unit to serialize.
///
/// @param o the outpout stream to serialize the translation_unit to.
void
dump(const translation_unit& t, std::ostream& o)
{
  xml_writer::write_context ctxt(o);
  write_translation_unit(t, ctxt, /*indent=*/0);
  o << "\n";
}

/// Serialize an instance of @ref translation_unit to stderr.
///
/// @param t the translation_unit to serialize.
void
dump(const translation_unit& t)
{dump(t, cerr);}

/// Serialize a pointer to @ref translation_unit to an output stream.
///
/// @param t the @ref translation_unit_sptr to serialize.
///
/// @param o the output stream to serialize the translation unit to.
void
dump(const translation_unit_sptr t, std::ostream& o)
{
  if (t)
    dump(*t, o);
}

/// Serialize a pointer to @ref translation_unit to stderr.
///
/// @param t the translation_unit_sptr to serialize.
void
dump(const translation_unit_sptr t)
{
  if (t)
    dump(*t);
}

/// Serialize the source location of a decl to an output stream for
/// debugging purposes.
///
/// @param d the declaration to consider.
///
/// @param o the output stream to serizalize the location to.
void
dump_decl_location(const decl_base& d, ostream& o)
{
  string path;
  unsigned line = 0, col = 0;
  translation_unit* tu = get_translation_unit(d);

  tu->get_loc_mgr().expand_location(d.get_location(), path, line, col);
  o << path << ":" << line << "," << col << "\n";
}

/// Serialize the source location of a decl to stderr for debugging
/// purposes.
///
/// @param d the declaration to consider.
void
dump_decl_location(const decl_base& d)
{dump_decl_location(d, cerr);}

/// Serialize the source location of a dcl to stderr for debugging
/// purposes.
///
/// @param d the declaration to consider.
void
dump_decl_location(const decl_base* d)
{
  if (d)
    dump_decl_location(*d);
}

/// Serialize the source location of a dcl to stderr for debugging
/// purposes.
///
/// @param d the declaration to consider.
void
dump_decl_location(const decl_base_sptr d)
{dump_decl_location(d.get());}

// </Debugging routines>
} //end namespace abigail
