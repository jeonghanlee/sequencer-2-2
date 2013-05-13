The Sequencer is a component of the *Experimental Physics and Industrial
Controls System*, short `EPICS`_, a set of tools, libraries and applications
developed collaboratively and used worldwide to create distributed soft
real-time control systems for large scale scientific instruments.

The State Notation Language, short *SNL*, is a `domain specific
programming language`_ that smoothly integrates with and depends and builds
on `EPICS base`_. This project defines SNL and provides
an implementation, consisting of the SNL compiler and runtime system.

The Sequencer is `free software`_, licensed under the
`EPICS Open License`_.

This `talk about version 2.1 of the Sequencer`_ contains a short
introduction to SNL, including a walk through a simple example program to
illustrate the basic concepts. A (slightly out-dated) :doc:`Tutorial` is
also available, as well as a detailed :doc:`Reference` and instructions for
download and :doc:`Installation`; all of these are part of the
:doc:`Manual`.

The following gives a very short overview of the Sequencer's features.

In SNL you structure your program as a set of concurrently running finite
state machines, called :token:`state_sets`. State sets are declared by
listing their :token:`states`, which in turn define under which condition
(:token:`when`) the state set reaches another state and what the program
should do once the transition is triggered.

SNL allows you to bind program variables to externally defined *process
variables* (PVs), such as provided by an EPICS runtime database. This can be
done in such a way that the value of the program variable gets continuously
updated whenever the value of the acssociated PV changes. Such variables can
then be used inside the state transition conditions, and the runtime system
takes care that the conditions are evaluated when and only when changes to
the associated PV occur.

In the code that gets executed when a state transition takes place, you can
explicitly read the value of the PV into the associated program variable
(:c:func:`pvGet`), or to send the value of the variable to update the PV
(:c:func:`pvPut`). (These are just the two most important of many more
:ref:`BuiltinFunctions`.)

The mechanism behind all this PV magic is the EPICS `Channel Access`_
network protocol with its support for subscriptions. However, when
programming in SNL you need not concern yourself with the details of the
mechanism. The SNL compiler and runtime system manage all this behind the
scenes, and also take care that the integrity of your variables is
maintained, even in the presence of multiple concurrent state sets inside a
single program (at least in :ref:`safe mode`). *Programming in SNL is free
of locks and therefore also free of deadlocks.*

Despite all this, SNL is emphatically *not* a `high-level language`_: most
of its syntax and semantics are directly inherited from C. The language is
designed for seamless integration with C code; for instance, you can
directly call C procedures in SNL action blocks and state transition
conditions, and the types of variables you can declare in SNL map directly
to C types. The compiler for SNL (called *snc*, see :doc:`Compiling`) takes
a very minimalistic approach to compilation by generating portable standard
C89/90 code that works on all platforms supported by EPICS. In fact, most of
the SNL action code is compiled almost verbatim to C, and the generated code
is quite human readable. The SNL compiler also delegates all type checking
to the C compiler (and some care has been taken to generate the code in such
a way that the C type checker has actually enough information to *do* useful
type checking).

Compilation of the generated code and linking to the SNL runtime system is
also *not* done by snc. Instead this task is delegated to the EPICS build
system by making the Sequencer an EPICS *support module* that automatically
adds appropriate build rules for SNL source files.

.. _domain specific programming language: http://en.wikipedia.org/wiki/Domain-specific_programming_language
.. _EPICS: http://www.aps.anl.gov/epics/
.. _EPICS base: http://www.aps.anl.gov/epics/base/index.php
.. _Channel Access: http://www.aps.anl.gov/epics/base/R3-14/12-docs/CAref.html
.. _talk about version 2.1 of the Sequencer: http://www-csr.bessy.de/control/SoftDist/sequencer/talks/epics-meeting-2011-PSI/sequencer-2-1-slides.pdf
.. _free software: http://www.gnu.org/philosophy/free-sw.html
.. _EPICS Open License: http://www.aps.anl.gov/epics/license/open.php
.. _high-level language: http://www.haskell.org/
