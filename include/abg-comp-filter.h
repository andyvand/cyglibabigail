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
/// This header declares filters for the diff trees resulting from
/// comparing ABI Corpora.

#ifndef __ABG_COMP_FILTER_H__
#define __ABG_COMP_FILTER_H__

#include "abg-comparison.h"

namespace abigail
{
namespace comparison
{
/// Facilities to walk, categorize and possibly filter nodes of the
/// diff tree.
namespace filtering
{

bool
has_harmless_name_change(decl_base_sptr f, decl_base_sptr s);

class filter_base;
/// Convenience typedef for a shared pointer to filter_base
typedef shared_ptr<filter_base> filter_base_sptr;
/// Convenience typedef for a vector of filter_base_sptr
typedef std::vector<filter_base_sptr> filters;

/// The base class for the diff tree node filter.
///
/// It's intended to walk a tree of diff nodes and tag each relevant
/// name into one or several categories depending on well choosen
/// properties of the diff nodes.
struct filter_base : public diff_node_visitor
{
  friend void
  apply_filter(filter_base_sptr f, diff_sptr deef);
}; //end class filter_base

void
apply_filter(filter_base& filter, diff_sptr d);

void
apply_filter(filter_base& filter, corpus_diff_sptr d);

void
apply_filter(filter_base_sptr filter, diff_sptr d);

class harmless_filter;
/// Convenience typedef for a shared pointer to a harmless_filter.
typedef shared_ptr<harmless_filter> harmless_filter_sptr;

/// A filter that walks the diff nodes tree and tags relevant diff
/// nodes into categories considered to represent harmless changes.
class harmless_filter : public filter_base
{
  virtual bool
  visit(diff*, bool);

  virtual void
  visit_end(diff*);
}; // end class harmless_filter

class harmful_filter;
/// A convenience typedef for a shared pointer to harmful_filter.
typedef shared_ptr<harmful_filter> harmful_filter_sptr;

/// A filter that walks the diff nodes tree and tags relevant diff
/// nodes into categories considered to represent potentially harmful
/// changes.
class harmful_filter : public filter_base
{
  virtual bool
  visit(diff*, bool);

  virtual void
  visit_end(diff*);
}; // end class harmful_filter

} // end namespace filtering
} // end namespace comparison
} // end namespace abigail

#endif // __ABG_COMP_FILTER_H__
