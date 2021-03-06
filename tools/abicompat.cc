// -*- Mode: C++ -*-
//
// Copyright (C) 2014-2015 Red Hat, Inc.
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
/// This program reads a program A, one library L in version V which A
/// links against, and the same library L in a different version, V+P.
/// The program then checks that A is still ABI compatible with L in
/// version V+P.
///
/// The program also comes with a "weak mode" in which just the
/// application and the library in version V+P need to be provided by
/// the user.  In that case, the types of functions and variables of
/// the library that are consumed by the application are compared to
/// the types of the functions and variables expected by the
/// application.  If they match exactly, then the types of functions
/// and variables that the application expects from the library are
/// honoured by the library.  Otherwise, the library might provide
/// functions and variables that mean something different from what
/// the application expects and that might signal an ABI
/// incompatibility between what the application expects and what the
/// library provides.

#include <unistd.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <tr1/memory>
#include "abg-tools-utils.h"
#include "abg-corpus.h"
#include "abg-dwarf-reader.h"
#include "abg-comparison.h"

using std::string;
using std::cerr;
using std::cout;
using std::ostream;
using std::ofstream;
using std::vector;
using std::tr1::shared_ptr;

struct options
{
  string		unknow_option;
  string		app_path;
  string		lib1_path;
  string		lib2_path;
  shared_ptr<char>	app_di_root_path;
  shared_ptr<char>	lib1_di_root_path;
  shared_ptr<char>	lib2_di_root_path;
  vector<string>	suppression_paths;
  bool			display_help;
  bool			weak_mode;
  bool			list_undefined_symbols_only;
  bool			show_base_names;
  bool			show_redundant;

  options()
    :display_help(),
     weak_mode(),
     list_undefined_symbols_only(),
     show_base_names(),
     show_redundant(true)
  {}
}; // end struct options

static void
display_usage(const string& prog_name, ostream& out)
{
  out << "usage: " << prog_name
      << " [options] [application-path] [lib-v1-path] [lib-v2-path]"
      << "\n"
      << " where options can be: \n"
      << "  --help|-h  display this help message\n"
      << "  --list-undefined-symbols|-u  display the list of "
         "undefined symbols of the application\n"
      << "  --show-base-names|b  in the report, only show the base names "
         " of the files; not the full paths\n"
      << "  --app-debug-info-dir <path-to-app-debug-info>  set the path "
         "to the debug information directory for the application\n"
      << "  --lib-debug-info-dir1 <path-to-lib-debug-info1>  set the path "
         "to the debug information directory for the first library\n"
      << "  --lib-debug-info-dir2 <path-to-lib-debug-info2>  set the path "
         "to the debug information directory for the second library\n"
      <<  "--suppressions <path> specify a suppression file\n"
      << "--no-redundant  do not display redundant changes\n"
      << "--redundant  display redundant changes (this is the default)\n"
      << "--weak-mode  check compatibility between the application and "
         "just one version of the library."
    ;
}

static bool
parse_command_line(int argc, char* argv[], options& opts)
{
  if (argc < 2)
    return false;

  for (int i = 1; i < argc; ++i)
    {
      if (argv[i][0] != '-')
	{
	  if (opts.app_path.empty())
	    opts.app_path = argv[i];
	  else if (opts.lib1_path.empty())
	    opts.lib1_path = argv[i];
	  else if (opts.lib2_path.empty())
	    opts.lib2_path = argv[i];
	  else
	    return false;
	}
      else if (!strcmp(argv[i], "--list-undefined-symbols")
	       || !strcmp(argv[i], "-u"))
	opts.list_undefined_symbols_only = true;
      else if (!strcmp(argv[i], "--show-base-names")
	       || !strcmp(argv[i], "-b"))
	opts.show_base_names = true;
      else if (!strcmp(argv[i], "--app-debug-info-dir"))
	{
	  if (argc <= i + 1
	      || argv[i + 1][0] == '-')
	    return false;
	  // elfutils wants the root path to the debug info to be
	  // absolute.
	  opts.app_di_root_path =
	    abigail::tools_utils::make_path_absolute(argv[i + 1]);
	  ++i;
	}
      else if (!strcmp(argv[i], "--lib-debug-info-dir1"))
	{
	  if (argc <= i + 1
	      || argv[i + 1][0] == '-')
	    return false;
	  // elfutils wants the root path to the debug info to be
	  // absolute.
	  opts.lib1_di_root_path =
	    abigail::tools_utils::make_path_absolute(argv[i + 1]);
	  ++i;
	}
      else if (!strcmp(argv[i], "--lib-debug-info-dir2"))
	{
	  if (argc <= i + 1
	      || argv[i + 1][0] == '-')
	    return false;
	  // elfutils wants the root path to the debug info to be
	  // absolute.
	  opts.lib2_di_root_path =
	    abigail::tools_utils::make_path_absolute(argv[i + 1]);
	  ++i;
	}
      else if (!strcmp(argv[i], "--suppressions"))
	{
	  int j = i + 1;
	  if (j >= argc)
	    return false;
	  opts.suppression_paths.push_back(argv[j]);
	  ++i;
	}
      else if (!strcmp(argv[i], "--redundant"))
	opts.show_redundant = true;
      else if (!strcmp(argv[i], "--no-redundant"))
	opts.show_redundant = false;
      else if (!strcmp(argv[i], "--help")
	       || !strcmp(argv[i], "-h"))
	{
	  opts.display_help = true;
	  return true;
	}
      else if (!strcmp(argv[i], "--weak-mode"))
	opts.weak_mode = true;
      else
	{
	  opts.unknow_option = argv[i];
	  return false;
	}
    }

  if (!opts.list_undefined_symbols_only)
    {
      if (opts.app_path.empty()
	  || opts.lib1_path.empty())
	return false;
      if (!opts.weak_mode && opts.lib2_path.empty())
	opts.weak_mode = true;
    }

  return true;
}

using abigail::tools_utils::check_file;
using abigail::tools_utils::base_name;
using abigail::tools_utils::abidiff_status;
using abigail::corpus;
using abigail::corpus_sptr;
using abigail::ir::elf_symbols;
using abigail::ir::demangle_cplus_mangled_name;
using abigail::ir::type_base_sptr;
using abigail::ir::function_type_sptr;
using abigail::ir::function_decl;
using abigail::ir::var_decl;
using abigail::dwarf_reader::status;
using abigail::dwarf_reader::read_corpus_from_elf;
using abigail::comparison::diff_context_sptr;
using abigail::comparison::diff_context;
using abigail::comparison::diff_sptr;
using abigail::comparison::corpus_diff;
using abigail::comparison::corpus_diff_sptr;
using abigail::comparison::function_type_diff_sptr;
using abigail::comparison::compute_diff;
using abigail::comparison::suppression_sptr;
using abigail::comparison::suppressions_type;
using abigail::comparison::read_suppressions;

/// Perform a compatibility check of an application corpus linked
/// against a first version of library corpus, with a second version
/// of the same library.
///
/// @param opts the options the tool got invoked with.
///
/// @param app_corpus the application corpus to consider.
///
/// @param lib1_corpus the library corpus that got linked with the
/// application which corpus is @p app_corpus.
///
/// @param lib2_corpus the second version of the library corpus @p
/// lib1_corpus.  This function checks that the functions and
/// variables that @p app_corpus expects from lib1_corpus are still
/// present in @p lib2_corpus and that their types mean the same
/// thing.
///
/// @return a status bitfield.
static abidiff_status
perform_compat_check_in_normal_mode(options& opts,
				    corpus_sptr app_corpus,
				    corpus_sptr lib1_corpus,
				    corpus_sptr lib2_corpus)
{
  assert(lib1_corpus);
  assert(lib2_corpus);
  assert(app_corpus);

  abidiff_status status = abigail::tools_utils::ABIDIFF_OK;

  // compare lib1 and lib2 only by looking at the functions and
  // variables which symbols are those undefined in the app.

  for (elf_symbols::const_iterator i =
	 app_corpus->get_sorted_undefined_fun_symbols().begin();
       i != app_corpus->get_sorted_undefined_fun_symbols().end();
       ++i)
    {
      string id = (*i)->get_id_string();
      lib1_corpus->get_sym_ids_of_fns_to_keep().push_back(id);
      lib2_corpus->get_sym_ids_of_fns_to_keep().push_back(id);
    }
  for (elf_symbols::const_iterator i =
	 app_corpus->get_sorted_undefined_var_symbols().begin();
       i != app_corpus->get_sorted_undefined_var_symbols().end();
       ++i)
    {
      string id = (*i)->get_id_string();
      lib1_corpus->get_sym_ids_of_vars_to_keep().push_back(id);
      lib2_corpus->get_sym_ids_of_vars_to_keep().push_back(id);
    }

  if (!app_corpus->get_sorted_undefined_var_symbols().empty()
      || !app_corpus->get_sorted_undefined_fun_symbols().empty())
    {
      lib1_corpus->maybe_drop_some_exported_decls();
      lib2_corpus->maybe_drop_some_exported_decls();
    }

  diff_context_sptr ctxt(new diff_context());
  ctxt->show_added_fns(false);
  ctxt->show_added_vars(false);
  ctxt->show_added_symbols_unreferenced_by_debug_info(false);
  ctxt->show_linkage_names(true);
  ctxt->show_redundant_changes(opts.show_redundant);
  ctxt->switch_categories_off
    (abigail::comparison::ACCESS_CHANGE_CATEGORY
     | abigail::comparison::COMPATIBLE_TYPE_CHANGE_CATEGORY
     | abigail::comparison::HARMLESS_DECL_NAME_CHANGE_CATEGORY
     | abigail::comparison::NON_VIRT_MEM_FUN_CHANGE_CATEGORY
     | abigail::comparison::STATIC_DATA_MEMBER_CHANGE_CATEGORY
     | abigail::comparison::HARMLESS_ENUM_CHANGE_CATEGORY
     | abigail::comparison::HARMLESS_SYMBOL_ALIAS_CHANGE_CATEORY);

  // load the suppression specifications
  // before starting to diff the libraries.
  suppressions_type supprs;
  for (vector<string>::const_iterator i = opts.suppression_paths.begin();
       i != opts.suppression_paths.end();
       ++i)
    if (check_file(*i, cerr))
      read_suppressions(*i, supprs);

  if (!supprs.empty())
    ctxt->add_suppressions(supprs);

  // Now really do the diffing.
  corpus_diff_sptr changes = compute_diff(lib1_corpus, lib2_corpus, ctxt);

  const corpus_diff::diff_stats& s =
    changes->apply_filters_and_suppressions_before_reporting();

  if (changes->soname_changed()
      || s.num_func_removed() != 0
      || s.num_vars_removed() != 0
      || s.num_func_syms_removed() != 0
      || s.num_var_syms_removed() != 0
      || s.net_num_func_changed() != 0
      || s.net_num_vars_changed() != 0)
    {
      string app_path = opts.app_path,
	lib1_path = opts.lib1_path,
	lib2_path = opts.lib2_path;

      if (opts.show_base_names)
	{
	  base_name(opts.app_path, app_path);
	  base_name(opts.lib1_path, lib1_path);
	  base_name(opts.lib2_path, lib2_path);
	}

      status |= abigail::tools_utils::ABIDIFF_ABI_CHANGE;

      bool abi_broke_for_sure = changes->soname_changed()
	|| s.num_vars_removed()
	|| s.num_func_removed()
	|| s.num_var_syms_removed()
	|| s.num_func_syms_removed();

      cout << "ELF file '" << app_path << "'";
      if (abi_broke_for_sure)
	{
	  cout << " is not ";
	  status |= abigail::tools_utils::ABIDIFF_ABI_INCOMPATIBLE_CHANGE;
	}
      else
	  cout << " might not be ";

      cout << "ABI compatible with '" << lib2_path
	   << "' due to differences with '" << lib1_path
	   << "' below:\n";
      changes->report(cout);
    }

  return status;
}

/// An description of a change of the type of a function.  It contains
/// the declaration of the function we are interested in, as well as
/// the differences found in the type of that function.
struct fn_change
{
  function_decl* decl;
  function_type_diff_sptr diff;

  fn_change()
    : decl()
  {}

  fn_change(function_decl* decl,
	    function_type_diff_sptr difference)
    : decl(decl),
      diff(difference)
  {}
}; // end struct fn_change

/// An description of a change of the type of a variable.  It contains
/// the declaration of the variable we are interested in, as well as
/// the differences found in the type of that variable.
struct var_change
{
  var_decl* decl;
  diff_sptr diff;

  var_change()
    : decl()
  {}

  var_change(var_decl* var,
	     diff_sptr difference)
    : decl(var),
      diff(difference)
  {}
}; // end struct var_change

/// Perform a compatibility check of an application corpus and a
/// library corpus.
///
/// @param opts the options the tool got invoked with.
///
/// @param app_corpus the application corpus to consider.
///
/// @param lib_corpus the library corpus to consider.  The types of
/// the variables and functions exported by this library and consumed
/// by the application are compared with the types expected by the
/// application @p app_corpus.  This function checks that the types
/// mean the same thing; otherwise it emits on standard output type
/// layout differences found.
///
/// @return a status bitfield.
static abidiff_status
perform_compat_check_in_weak_mode(options& opts,
				  corpus_sptr app_corpus,
				  corpus_sptr lib_corpus)
{
  assert(lib_corpus);
  assert(app_corpus);

  abidiff_status status = abigail::tools_utils::ABIDIFF_OK;

  for (elf_symbols::const_iterator i =
	 app_corpus->get_sorted_undefined_fun_symbols().begin();
       i != app_corpus->get_sorted_undefined_fun_symbols().end();
       ++i)
    {
      string id = (*i)->get_id_string();
      lib_corpus->get_sym_ids_of_fns_to_keep().push_back(id);
    }
  for (elf_symbols::const_iterator i =
	 app_corpus->get_sorted_undefined_var_symbols().begin();
       i != app_corpus->get_sorted_undefined_var_symbols().end();
       ++i)
    {
      string id = (*i)->get_id_string();
      lib_corpus->get_sym_ids_of_vars_to_keep().push_back(id);
    }

  if (!app_corpus->get_sorted_undefined_var_symbols().empty()
      || !app_corpus->get_sorted_undefined_fun_symbols().empty())
    lib_corpus->maybe_drop_some_exported_decls();

  diff_context_sptr ctxt(new diff_context());
  ctxt->show_added_fns(false);
  ctxt->show_added_vars(false);
  ctxt->show_added_symbols_unreferenced_by_debug_info(false);
  ctxt->show_linkage_names(true);
  ctxt->show_redundant_changes(opts.show_redundant);
  ctxt->switch_categories_off
    (abigail::comparison::ACCESS_CHANGE_CATEGORY
     | abigail::comparison::COMPATIBLE_TYPE_CHANGE_CATEGORY
     | abigail::comparison::HARMLESS_DECL_NAME_CHANGE_CATEGORY
     | abigail::comparison::NON_VIRT_MEM_FUN_CHANGE_CATEGORY
     | abigail::comparison::STATIC_DATA_MEMBER_CHANGE_CATEGORY
     | abigail::comparison::HARMLESS_ENUM_CHANGE_CATEGORY
     | abigail::comparison::HARMLESS_SYMBOL_ALIAS_CHANGE_CATEORY);

  suppressions_type supprs;
  for (vector<string>::const_iterator i = opts.suppression_paths.begin();
       i != opts.suppression_paths.end();
       ++i)
    if (check_file(*i, cerr))
      read_suppressions(*i, supprs);

  if (!supprs.empty())
    ctxt->add_suppressions(supprs);

  {
    function_type_sptr lib_fn_type, app_fn_type;
    function_type_diff_sptr fn_type_diff;
    vector<fn_change> fn_changes;
    for (corpus::functions::const_iterator i =
	   lib_corpus->get_functions().begin();
	 i != lib_corpus->get_functions().end();
	 ++i)
      {
	lib_fn_type = (*i)->get_type();
	assert(lib_fn_type);
	app_fn_type = lookup_function_type_in_corpus(lib_fn_type, *app_corpus);
	fn_type_diff = compute_diff(app_fn_type, lib_fn_type, ctxt);
	if (fn_type_diff && fn_type_diff->to_be_reported())
	  fn_changes.push_back(fn_change(*i, fn_type_diff));
      }

    string lib1_path = opts.lib1_path, app_path = opts.app_path;
    if (opts.show_base_names)
      {
	base_name(opts.lib1_path, lib1_path);
	base_name(opts.app_path, app_path);
      }

    if (!fn_changes.empty())
      {
	cout << "functions defined in library\n    "
	     << "'" << lib1_path << "'\n"
	     << "have sub-types that are different from what application\n    "
	     << "'" << app_path << "'\n"
	     << "expects:\n\n";
	for (vector<fn_change>::const_iterator i = fn_changes.begin();
	     i != fn_changes.end();
	     ++i)
	  {
	    cout << "  "
		 << i->decl->get_pretty_representation()
		 << ":\n";
	    i->diff->report(cout, "    ");
	    cout << "\n";
	  }
      }

    if (!fn_changes.empty())
      status |= abigail::tools_utils::ABIDIFF_ABI_CHANGE;

    type_base_sptr lib_var_type, app_var_type;
    diff_sptr type_diff;
    vector<var_change> var_changes;
    for (corpus::variables::const_iterator i =
	   lib_corpus->get_variables().begin();
	 i != lib_corpus->get_variables().end();
	 ++i)
      {
	lib_var_type = (*i)->get_type();
	assert(lib_var_type);
	app_var_type = lookup_type_in_corpus(lib_var_type, *app_corpus);
	type_diff = compute_diff(app_var_type, lib_var_type, ctxt);
	if (type_diff && type_diff->to_be_reported())
	  var_changes.push_back(var_change(*i, type_diff));
      }
    if (!var_changes.empty())
      {
	cout << "variables defined in library\n    "
	     << "'" << lib1_path << "'\n"
	     << "have sub-types that are different from what application\n    "
	     << "'" << app_path << "'\n"
	     << "expects:\n\n";
	for (vector<var_change>::const_iterator i = var_changes.begin();
	     i != var_changes.end();
	     ++i)
	  {
	    cout << "  "
		 << i->decl->get_pretty_representation()
		 << ":\n";
	    i->diff->report(cout, "    ");
	    cout << "\n";
	  }
      }
  }
  return status;
}

int
main(int argc, char* argv[])
{
  options opts;

  if (!parse_command_line(argc, argv, opts))
    {
      if (!opts.unknow_option.empty())
	{
	  cerr << "unrecognized option: " << opts.unknow_option << "\n"
	       << "try the --help option for more information\n";
	  return (abigail::tools_utils::ABIDIFF_USAGE_ERROR
		  | abigail::tools_utils::ABIDIFF_ERROR);
	}

      cerr << "wrong invocation\n"
	   << "try the --help option for more information\n";
      return (abigail::tools_utils::ABIDIFF_USAGE_ERROR
	      | abigail::tools_utils::ABIDIFF_ERROR);
    }

  if (opts.display_help)
    {
      display_usage(argv[0], cout);
      return (abigail::tools_utils::ABIDIFF_USAGE_ERROR
		  | abigail::tools_utils::ABIDIFF_ERROR);
    }

  assert(!opts.app_path.empty());
  if (!abigail::tools_utils::check_file(opts.app_path, cerr))
    return abigail::tools_utils::ABIDIFF_ERROR;

  abigail::tools_utils::file_type type =
    abigail::tools_utils::guess_file_type(opts.app_path);
  if (type != abigail::tools_utils::FILE_TYPE_ELF)
    {
      cerr << opts.app_path << " is not an ELF file\n";
      return abigail::tools_utils::ABIDIFF_ERROR;
    }

  // Read the application ELF file.
  corpus_sptr app_corpus;
  char * app_di_root = opts.app_di_root_path.get();
  status status =
    read_corpus_from_elf(opts.app_path,
			 &app_di_root,
			 /*load_all_types=*/opts.weak_mode,
			 app_corpus);

  if (status & abigail::dwarf_reader::STATUS_NO_SYMBOLS_FOUND)
    {
      cerr << "could not read symbols from " << opts.app_path << "\n";
      return abigail::tools_utils::ABIDIFF_ERROR;
    }
  if (!(status & abigail::dwarf_reader::STATUS_OK))
    {
      cerr << "could not read file " << opts.app_path << "\n";
      return abigail::tools_utils::ABIDIFF_ERROR;
    }

  if (opts.list_undefined_symbols_only)
    {
      for (elf_symbols::const_iterator i =
	     app_corpus->get_sorted_undefined_fun_symbols().begin();
	   i != app_corpus->get_sorted_undefined_fun_symbols().end();
	   ++i)
	{
	  string id = (*i)->get_id_string();
	  string sym_name = (*i)->get_name();
	  string demangled_name = demangle_cplus_mangled_name(sym_name);
	  if (demangled_name != sym_name)
	    cout << demangled_name << "  {" << id << "}\n";
	  else
	    cout << id << "\n";
	}
      return abigail::tools_utils::ABIDIFF_OK;
    }

  // Read the first version of the library.
  assert(!opts.lib1_path.empty());
  if (!abigail::tools_utils::check_file(opts.lib1_path, cerr))
    return abigail::tools_utils::ABIDIFF_ERROR;
  type = abigail::tools_utils::guess_file_type(opts.lib1_path);
  if (type != abigail::tools_utils::FILE_TYPE_ELF)
    {
      cerr << opts.lib1_path << " is not an ELF file\n";
      return abigail::tools_utils::ABIDIFF_ERROR;
    }

  corpus_sptr lib1_corpus;
  char * lib1_di_root = opts.lib1_di_root_path.get();
  status = read_corpus_from_elf(opts.lib1_path,
				&lib1_di_root,
				/*load_all_types=*/false,
				lib1_corpus);
  if (status & abigail::dwarf_reader::STATUS_DEBUG_INFO_NOT_FOUND)
    cerr << "could not read debug info for " << opts.lib1_path << "\n";
  if (status & abigail::dwarf_reader::STATUS_NO_SYMBOLS_FOUND)
    {
      cerr << "could not read symbols from " << opts.lib1_path << "\n";
      return abigail::tools_utils::ABIDIFF_ERROR;
    }
  if (!(status & abigail::dwarf_reader::STATUS_OK))
    {
      cerr << "could not read file " << opts.lib1_path << "\n";
      return abigail::tools_utils::ABIDIFF_ERROR;
    }

  // Read the second version of the library.
  corpus_sptr lib2_corpus;
  if (!opts.weak_mode)
    {
      assert(!opts.lib2_path.empty());
      char * lib2_di_root = opts.lib2_di_root_path.get();
      status = read_corpus_from_elf(opts.lib2_path,
				    &lib2_di_root,
				    /*load_all_types=*/false,
				    lib2_corpus);
      if (status & abigail::dwarf_reader::STATUS_DEBUG_INFO_NOT_FOUND)
	cerr << "could not read debug info for " << opts.lib2_path << "\n";
      if (status & abigail::dwarf_reader::STATUS_NO_SYMBOLS_FOUND)
	{
	  cerr << "could not read symbols from " << opts.lib2_path << "\n";
	  return abigail::tools_utils::ABIDIFF_ERROR;
	}
      if (!(status & abigail::dwarf_reader::STATUS_OK))
	{
	  cerr << "could not read file " << opts.lib2_path << "\n";
	  return abigail::tools_utils::ABIDIFF_ERROR;
	}
    }

  abidiff_status s = abigail::tools_utils::ABIDIFF_OK;

  if (opts.weak_mode)
    s = perform_compat_check_in_weak_mode(opts,
					  app_corpus,
					  lib1_corpus);
  else
    s = perform_compat_check_in_normal_mode(opts,
					    app_corpus,
					    lib1_corpus,
					    lib2_corpus);

  return s;
}
