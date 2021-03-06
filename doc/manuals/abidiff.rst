.. _abidiff_label:

=======
abidiff
=======

abidiff compares the Application Binary Interfaces (ABI) of two shared
libraries in `ELF`_ format.  It emits a meaningful report describing the
differences between the two ABIs.

For a comprehensive ABI change report that includes changes about
function and variable sub-types, the two input shared libraries must
be accompanied with their debug information in `DWARF`_ format.
Otherwise, only `ELF`_ symbols that were added or removed are
reported.

.. _abidiff_invocation_label:

Invocation
==========

::

  abidiff [options] <first-shared-library> <second-shared-library>

.. _abidiff_options_label:

Options
=======

  * --help

    Display a short help about the command and exit.

  * --debug-info-dir1 <*di-path1*>

    For cases where the debug information for *first-shared-library*
    is split out into a separate file, tells ``abidiff`` where to find
    that separate debug information file.

    Note that *di-path* must point to the root directory under which
    the debug information is arranged in a tree-like manner.  Under
    Red Hat based systems, that directory is usually
    ``<root>/usr/lib/debug``.

    Note also that this option is not mandatory for split debug
    information installed by your system's package manager because
    then ``abidiff`` knows where to find it.

  * --debug-info-dir2 <*di-path2*>

    Like ``--debug-info-dir1``, this options tells ``abidiff`` where
    to find the split debug information for the
    *second-shared-library* file.

  * --stat

    Rather than displaying the detailed ABI differences between
    *first-shared-library* and *second-shared-library*, just display
    some summary statistics about these differences.

  * --symtabs

    Only display the symbol tables of the *first-shared-library* and
    *second-shared-library*.

  * --deleted-fns

    In the resulting report about the differences between
    *first-shared-library* and *second-shared-library*, only display
    the globally defined functions that got deleted from
    *first-shared-library*.

  * --changed-fns

    In the resulting report about the differences between
    *first-shared-library* and *second-shared-library*, only display
    the changes in sub-types of the global functions defined in
    *first-shared-library*.

  * --added-fns

    In the resulting report about the differences between
    *first-shared-library* and *second-shared-library*, only display
    the globally defined functions that were added to
    *second-shared-library*.

  * --deleted-vars

    In the resulting report about the differences between
    *first-shared-library* and *second-shared-library*, only display
    the globally defined variables that were deleted from
    *first-shared-library*.

  * --changed-vars

    In the resulting report about the differences between
    *first-shared-library* and *second-shared-library*, only display
    the changes in the sub-types of the global variables defined in
    *first-shared-library*

  * --added-vars

    In the resulting report about the differences between
    *first-shared-library* and *second-shared-library*, only display
    the global variables that were added (defined) to
    *second-shared-library*.

  * --no-linkage-name 

    In the resulting report, do not display the linkage names of
    the added, removed, or changed functions or variables.

  * --no-unreferenced-symbols

    In the resulting report, do not display change information about
    function and variable symbols that are not referenced by any debug
    information.  Note that for these symbols not referenced by any
    debug information, the change information displayed is either
    added or removed symbols.


  * --suppressions <*path-to-suppressions*>

    Use a :ref:`suppression specification <suppr_spec_label>` file located
    at *path-to-suppressions*.

  * --drop <*regex*>

    When reading the *first-shared-library* and
    *second-shared-library* ELF input files, drop the globally defined
    functions and variables which name match the regular expression
    *regex*.  As a result, no change involving these functions or
    variables will be emitted in the diff report.

  * --drop-fn <*regex*>

    When reading the *first-shared-library* and
    *second-shared-library* ELF input files, drop the globally defined
    functions which name match the regular expression *regex*.  As a
    result, no change involving these functions will be emitted in the
    diff report.

  * --drop-var <*regex*>

    When reading the *first-shared-library* and
    *second-shared-library* ELF input files, drop the globally defined
    variables matching a the regular expression *regex*.

  * --keep <*regex*>

    When reading the *first-shared-library* and
    *second-shared-library* ELF input files, keep the globally defined
    functions and variables which names match the regular expression
    *regex*.  All other functions and variables are dropped on the
    floor and will thus not appear in the resulting diff report.

  * --keep-fn <*regex*>

    When reading the *first-shared-library* and
    *second-shared-library* ELF input files, keep the globally defined
    functions which name match the regular expression *regex*.  All
    other functions are dropped on the floor and will thus not appear
    in the resulting diff report.

  * --keep-var <*regex*>

    When reading the *first-shared-library* and
    *second-shared-library* ELF input files, keep the globally defined
    which names match the regular expression *regex*.  All other
    variables are dropped on the floor and will thus not appear in the
    resulting diff report.

  * --harmless

    In the diff report, display only the :ref:`harmless
    <harmlesschangeconcept_label>` changes.  By default, the harmless
    changes are filtered out of the diff report keep the clutter to a
    minimum and have a greater change to spot real ABI issues.

  * --no-harmful

    In the diff report, do not display the :ref:`harmful
    <harmfulchangeconcept_label>` changes.  By default, only the
    harmful changes are displayed in diff report.

  * --redundant

    In the diff report, do display redundant changes.  A redundant
    change is a change that has been displayed elsewhere in the
    report.

  * --no-redundant

    In the diff report, do *NOT* display redundant changes.  A
    redundant change is a change that has been displayed elsewhere in
    the report.  This option is switched on by default.

  *  --dump-diff-tree

    After the diff report, emit a textual representation of the diff
    nodes tree used by the comparison engine to represent the changed
    functions and variables.  That representation is emitted to the
    error output for debugging purposes.  Note that this diff tree is
    relevant only to functions and variables that have some sub-type
    changes.  Added or removed functions and variables do not have any
    diff nodes tree associated to them.

.. _abidiff_return_value_label:

Return values
=============

The exit code of the ``abidiff`` command is either 0 if the ABI of the
binaries being compared are equal, or non-zero if they differ or if
the tool encountered an error.

In the later case, the exit code is a 8-bits-wide bit field in which
each bit has a specific meaning.

The first bit, of value 1, named ``ABIDIFF_ERROR`` means there was an
error.

The second bit, of value 2, named ``ABIDIFF_USAGE_ERROR`` means there
was an error in the way the user invoked the tool.  It might be set,
for instance, if the user invoked the tool with an unknown command
line switch, with a wrong number or argument, etc.  If this bit is
set, then the ``ABIDIFF_ERROR`` bit must be set as well.

The third bit, of value 4, named ``ABIDIFF_ABI_CHANGE`` means the ABI
of the binaries being compared are different.  

The fourth bit, of value 8, named ``ABIDIFF_ABI_INCOMPATIBLE_CHANGE``
means the ABI of the binaries compared are different in an
incompatible way.  If this bit is set, then the ``ABIDIFF_ABI_CHANGE``
bit must be set as well.  If the ``ABIDIFF_ABI_CHANGE`` is set and the
``ABIDIFF_INCOMPATIBLE_CHANGE`` is *NOT* set, then it means that the
ABIs being compared might or might not be compatible.  In that case, a
human being needs to review the ABI changes to decide if they are
compatible or not.

The remaining bits are not used for the moment.

.. _abidiff_usage_example_label:

Usage examples
==============

  1. Detecting a change in a sub-type of a function: ::

	$ cat -n test-v0.cc
		 1	// Compile this with:
		 2	//   g++ -g -Wall -shared -o libtest-v0.so test-v0.cc
		 3	
		 4	struct S0
		 5	{
		 6	  int m0;
		 7	};
		 8	
		 9	void
		10	foo(S0* /*parameter_name*/)
		11	{
		12	  // do something with parameter_name.
		13	}
	$ 
	$ cat -n test-v1.cc
		 1	// Compile this with:
		 2	//   g++ -g -Wall -shared -o libtest-v1.so test-v1.cc
		 3	
		 4	struct type_base
		 5	{
		 6	  int inserted;
		 7	};
		 8	
		 9	struct S0 : public type_base
		10	{
		11	  int m0;
		12	};
		13	
		14	void
		15	foo(S0* /*parameter_name*/)
		16	{
		17	  // do something with parameter_name.
		18	}
	$ 
	$ g++ -g -Wall -shared -o libtest-v0.so test-v0.cc
	$ g++ -g -Wall -shared -o libtest-v1.so test-v1.cc
	$ 
	$ ../build/tools/abidiff libtest-v0.so libtest-v1.so
	Functions changes summary: 0 Removed, 1 Changed, 0 Added function
	Variables changes summary: 0 Removed, 0 Changed, 0 Added variable

	1 function with some indirect sub-type change:

	  [C]'function void foo(S0*)' has some indirect sub-type changes:
		parameter 0 of type 'S0*' has sub-type changes:
		  in pointed to type 'struct S0':
		    size changed from 32 to 64 bits
		    1 base class insertion:
		      struct type_base
		    1 data member change:
		     'int S0::m0' offset changed from 0 to 32
	$


  2. Detecting another change in a sub-type of a function: ::

	$ cat -n test-v0.cc
		 1	// Compile this with:
		 2	//   g++ -g -Wall -shared -o libtest-v0.so test-v0.cc
		 3	
		 4	struct S0
		 5	{
		 6	  int m0;
		 7	};
		 8	
		 9	void
		10	foo(S0& /*parameter_name*/)
		11	{
		12	  // do something with parameter_name.
		13	}
	$ 
	$ cat -n test-v1.cc
		 1	// Compile this with:
		 2	//   g++ -g -Wall -shared -o libtest-v1.so test-v1.cc
		 3	
		 4	struct S0
		 5	{
		 6	  char inserted_member;
		 7	  int m0;
		 8	};
		 9	
		10	void
		11	foo(S0& /*parameter_name*/)
		12	{
		13	  // do something with parameter_name.
		14	}
	$ 
	$ g++ -g -Wall -shared -o libtest-v0.so test-v0.cc
	$ g++ -g -Wall -shared -o libtest-v1.so test-v1.cc
	$ 
	$ ../build/tools/abidiff libtest-v0.so libtest-v1.so
	Functions changes summary: 0 Removed, 1 Changed, 0 Added function
	Variables changes summary: 0 Removed, 0 Changed, 0 Added variable

	1 function with some indirect sub-type change:

	  [C]'function void foo(S0&)' has some indirect sub-type changes:
		parameter 0 of type 'S0&' has sub-type changes:
		  in referenced type 'struct S0':
		    size changed from 32 to 64 bits
		    1 data member insertion:
		      'char S0::inserted_member', at offset 0 (in bits)
		    1 data member change:
		     'int S0::m0' offset changed from 0 to 32


	$

  3. Detecting that functions got removed or added to a library: ::

	$ cat -n test-v0.cc
		 1	// Compile this with:
		 2	//   g++ -g -Wall -shared -o libtest-v0.so test-v0.cc
		 3	
		 4	struct S0
		 5	{
		 6	  int m0;
		 7	};
		 8	
		 9	void
		10	foo(S0& /*parameter_name*/)
		11	{
		12	  // do something with parameter_name.
		13	}
	$ 
	$ cat -n test-v1.cc
		 1	// Compile this with:
		 2	//   g++ -g -Wall -shared -o libtest-v1.so test-v1.cc
		 3	
		 4	struct S0
		 5	{
		 6	  char inserted_member;
		 7	  int m0;
		 8	};
		 9	
		10	void
		11	bar(S0& /*parameter_name*/)
		12	{
		13	  // do something with parameter_name.
		14	}
	$ 
	$ g++ -g -Wall -shared -o libtest-v0.so test-v0.cc
	$ g++ -g -Wall -shared -o libtest-v1.so test-v1.cc
	$ 
	$ ../build/tools/abidiff libtest-v0.so libtest-v1.so
	Functions changes summary: 1 Removed, 0 Changed, 1 Added functions
	Variables changes summary: 0 Removed, 0 Changed, 0 Added variable

	1 Removed function:
	  'function void foo(S0&)'    {_Z3fooR2S0}

	1 Added function:
	  'function void bar(S0&)'    {_Z3barR2S0}

	$

.. _ELF: http://en.wikipedia.org/wiki/Executable_and_Linkable_Format
.. _DWARF: http://www.dwarfstd.org

