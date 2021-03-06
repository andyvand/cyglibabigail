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

#ifndef __ABG_VIZ_DOT_H__
#define __ABG_VIZ_DOT_H__

#include <abg-viz-common.h>

namespace abigail
{

/// Base class for graph nodes.
struct node_base
{ 
  /// Possible derived types.
  enum type { child, parent }; 

  std::string		_M_id;
  static units_type 	_M_count_total;	// Start at zero.
  units_type 		_M_count;
  type			_M_type;
  float   		_M_x_space;	// Column spacing.
  float   		_M_y_space;	// Row spacing.
  const style&		_M_style;

  explicit
  node_base(std::string __id, type __t, const style& __sty)
    : _M_id(__id), _M_count(++_M_count_total), 
    _M_type(__t), _M_x_space(0.4), _M_y_space(0.2), _M_style(__sty)
  { }
};

/// Useful constants.
extern const style parent_sty;
extern const style child_sty;


/**
  Parent node.

  Some characteristics:
  - name (text anchor = start ie left).
  - background box x and y size
  - style info
  - (optional) template parameters

 */
struct parent_node : public node_base 
{
   parent_node(std::string __id) 
   : node_base(__id, node_base::parent, parent_sty)
   { }
};


/**
  Child node.

  Some characteristics:
  - horizontal name (text anchor = start ie left).
  - background box
  - (optional) template parameters

 */
struct child_node : public node_base 
{
   child_node(std::string __id) 
   : node_base(__id, node_base::child, child_sty)
   { }
};


/**
  DOT "graph" style notation for class inheritance.

  This is a compact DOT representation of a single class inheritance.

  It is composed of the following data points for each parent

  - parent classes
  - child classes
  - name

  Including typographic information to compute line length, and C++
  niceities like grouping and overload sets.

  It's constructed by creating a digraph, starting from the base node.
 */
struct dot
{

private:

  const std::string    	_M_title;
  const canvas&	       	_M_canvas;
  const typography&    	_M_typo;	

  std::ostringstream   	_M_sstream;
  
public:

  dot(const std::string __title, 
      const canvas& __cv = ansi_letter_canvas,
      const typography& __typo = arial_typo) 
  : _M_title(__title), _M_canvas(__cv), _M_typo(__typo)
  { }
  
  // Empty when the output buffer is.
  bool
  empty() { return _M_sstream.str().empty(); }

  void 
  start_element();
  
  void 
  finish_element();
  
  void 
  add_title();

  void
  add_node(const node_base&);

  void
  add_edge(const node_base&, const node_base&);

  void
  add_parent(const parent_node&);

  void
  add_child_to_node(const child_node&, const node_base&);

  void
  write();

  void 
  start()
  {
    this->start_element();
  }

  void 
  finish()
  {
    this->finish_element();
    this->write();
  }
};

// XXX connect external xml file to input. 
// parse input, pick apart elements, attributes.

}// end namespace abigail

#endif //__ABG_VIZ_DOT_H__
