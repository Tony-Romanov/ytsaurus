==============
pytest-timeout
==============

|python| |version| |anaconda| |ci|

.. |version| image:: https://img.shields.io/pypi/v/pytest-timeout.svg
  :target: https://pypi.python.org/pypi/pytest-timeout

.. |anaconda| image:: https://img.shields.io/conda/vn/conda-forge/pytest-timeout.svg
  :target: https://anaconda.org/conda-forge/pytest-timeout

.. |ci| image:: https://github.com/pytest-dev/pytest-timeout/workflows/build/badge.svg
  :target: https://github.com/pytest-dev/pytest-timeout/actions

.. |python| image:: https://img.shields.io/pypi/pyversions/pytest-timeout.svg
  :target: https://pypi.python.org/pypi/pytest-timeout/

This is a plugin which will terminate tests after a certain timeout,
assuming the test session isn't being debugged. When aborting a test
it will show a stack dump of all threads running at the time.
This is useful when running tests under a continuous
integration server or simply if you don't know why the test suite hangs.

.. note::

   The way this plugin detects whether or not a debugging session is
   active is by checking if a trace function is set and if one is, it
   check to see if the module it belongs to is present in a set of
   known debugging frameworks modules OR if pytest itself drops you
   into a pbd session.

.. note::

   While by default on POSIX systems pytest will continue to execute
   the tests after a test has timed out this is not always possible.
   Often the only sure way to interrupt a hanging test is by
   terminating the entire process.  As this is a hard termination
   (``os._exit()``) it will result in no teardown, JUnit XML output
   etc.  But the plugin will ensure you will have the debugging output
   on stderr nevertheless, which is the most important part at this
   stage.  See below for detailed information on the timeout methods
   and their side-effects.

The pytest-timeout plugin has been tested on python 2.7 or higher,
including 3.X, pypy and pypy3.  See tox.ini for currently tested
versions.


Usage
=====

Install is as simple as e.g.::

   pip install pytest-timeout

Now you can run tests using a timeout, in seconds, after which they
will be terminated::

   pytest --timeout=300

Alternatively you can mark individual tests as having a timeout::

   @pytest.mark.timeout(60)
   def test_foo():
       pass

By default the plugin will not time out any tests, you must specify a
valid timeout for the plugin to interrupt long-running tests.  A
timeout is always specified as a number of seconds, and can be
defined in a number of ways, from low to high priority:

1. You can set a global timeout in the `pytest configuration file`__
   using the ``timeout`` option.  E.g.::

      [pytest]
      timeout = 300

2. The ``PYTEST_TIMEOUT`` environment variable sets a global timeout
   overriding a possible value in the configuration file.

3. The ``--timeout`` command line option sets a global timeout
   overriding both the environment variable and configuration option.

4. Using the ``timeout`` marker_ on test items you can specify
   timeouts on a per-item basis::

      @pytest.mark.timeout(300)
      def test_foo():
          pass

__ https://docs.pytest.org/en/latest/reference.html#ini-options-ref

.. _marker: https://docs.pytest.org/en/latest/mark.html

Setting a timeout to 0 seconds disables the timeout, so if you have a
global timeout set you can still disable the timeout by using the
mark.

Timeout Methods
===============

Interrupting tests which hang is not always as simple and can be
platform dependent.  Furthermore some methods of terminating a test
might conflict with the code under test itself.  The pytest-timeout
plugin tries to pick the most suitable method based on your platform,
but occasionally you may need to specify a specific timeout method
explicitly.

   If a timeout method does not work your safest bet is to use the
   *thread* method.

thread
------

This is the surest and most portable method.  It is also the default
on systems not supporting the *signal* method.  For each test item the
pytest-timeout plugin starts a timer thread which will terminate the
whole process after the specified timeout.  When a test item finishes
this timer thread is cancelled and the test run continues.

The downsides of this method are that there is a relatively large
overhead for running each test and that test runs are not completed.
This means that other pytest features, like e.g. JUnit XML output or
fixture teardown, will not function normally.  The second issue might
be alleviated by using the ``--boxed`` option of the pytest-xdist_
plugin.

.. _pytest-xdist: https://pypi.org/project/pytest-xdist/

The benefit of this method is that it will always work.  Furthermore
it will still provide you debugging information by printing the stacks
of all the threads in the application to stderr.

signal
------

If the system supports the SIGALRM signal the *signal* method will be
used by default.  This method schedules an alarm when the test item
starts and cancels it when it finishes.  If the alarm expires during
the test the signal handler will dump the stack of any other threads
running to stderr and use ``pytest.fail()`` to interrupt the test.

The benefit of this method is that the pytest process is not
terminated and the test run can complete normally.

The main issue to look out for with this method is that it may
interfere with the code under test.  If the code under test uses
SIGALRM itself things will go wrong and you will have to choose the
*thread* method.

Specifying the Timeout Method
-----------------------------

The timeout method can be specified by using the ``timeout_method``
option in the `pytest configuration file`__, the ``--timeout_method``
command line parameter or the ``timeout`` marker_.  Simply set their
value to the string ``thread`` or ``signal`` to override the default
method.  On a marker this is done using the ``method`` keyword::

   @pytest.mark.timeout(method='thread')
   def test_foo():
       pass

__ https://docs.pytest.org/en/latest/reference.html#ini-options-ref

.. _marker: https://docs.pytest.org/en/latest/mark.html

The ``timeout`` Marker API
==========================

The full signature of the timeout marker is::

   pytest.mark.timeout(timeout=0, method=DEFAULT_METHOD)

You can use either positional or keyword arguments for both the
timeout and the method.  Neither needs to be present.

See the marker api documentation_ and examples_ for the various ways
markers can be applied to test items.

.. _documentation: https://docs.pytest.org/en/latest/mark.html

.. _examples: https://docs.pytest.org/en/latest/example/markers.html#marking-whole-classes-or-modules


Timeouts in Fixture Teardown
============================

The plugin will happily terminate timeouts in the finalisers of
fixtures.  The timeout specified applies to the entire process of
setting up fixtures, running the tests and finalising the fixtures.
However when a timeout occurs in a fixture finaliser and the test
suite continues, i.e. the signal method is used, it must be realised
that subsequent fixtures which need to be finalised might not have
been executed, which could result in a broken test-suite anyway.  In
case of doubt the thread method which terminates the entire process
might result in clearer output.


Changelog
=========

1.4.2
-----

- Fix compatibility when run with pytest pre-releases, thanks
  Bruno Oliveira,
- Fix detection of third-party debuggers, thanks Bruno Oliveira.

1.4.1
-----

- Fix coverage compatibility which was broken by 1.4.0.

1.4.0
-----

- Better detection of when we are debugging, thanks Mattwmaster58.

1.3.4
-----

- Give the threads a name to help debugging, thanks Thomas Grainger.
- Changed location to https://github.com/pytest-dev/pytest-timeout
  because bitbucket is dropping mercurial support.  Thanks Thomas
  Grainger and Bruno Oliveira.

1.3.3
-----

- Fix support for pytest >= 3.10.

1.3.2
-----

- This changelog was ommitted for the 1.3.2 release and was added
  afterwards.  Apologies for the confusion.
- Fix pytest 3.7.3 compatibility.  The capture API had changed
  slightly and this needed fixing.  Thanks Bruno Oliveira for the
  contribution.

1.3.1
-----

- Fix deprecation warning on Python 3.6.  Thanks Mickaël Schoentgen
- Create a valid tag for the release.  Somehow this didn't happen for
  1.3.0, that tag points to a non-existing commit.

1.3.0
-----

- Make it possible to only run the timeout timer on the test function
  and not the whole fixture setup + test + teardown duration.  Thanks
  Pedro Algarvio for the work!
- Use the new pytest marker API, Thanks Pedro Algarvio for the work!

1.2.1
-----

- Fix for pytest 3.3, thanks Bruno Oliveira.
- Update supported python versions:
  - Add CPython 3.6.
  - Drop CPyhon 2.6 (as did pytest 3.3)
  - Drop CPyhon 3.3
  - Drop CPyhon 3.4

1.2.0
-----

* Allow using floats as timeout instead of only integers, thanks Tom
  Myers.

1.1.0
-----

* Report (default) timeout duration in header, thanks Holger Krekel.

1.0.0
-----

* Bump version to 1.0 to commit to semantic versioning.
* Fix issue #12: Now compatible with pytest 2.8, thanks Holger Krekel.
* No longer test with pexpect on py26 as it is no longer supported
* Require pytest 2.8 and use new hookimpl decorator

0.5
---

* Timeouts will no longer be triggered when inside an interactive pdb
  session started by ``pytest.set_trace()`` / ``pdb.set_trace()``.

* Add pypy3 environment to tox.ini.

* Transfer repository to pytest-dev team account.

0.4
---

* Support timeouts happening in (session scoped) finalizers.

* Change command line option --timeout_method into --timeout-method
  for consistency with pytest

0.3
---

* Added the PYTEST_TIMEOUT environment variable as a way of specifying
  the timeout (closes issue #2).

* More flexible marker argument parsing: you can now specify the
  method using a positional argument.

* The plugin is now enabled by default.  There is no longer a need to
  specify ``timeout=0`` in the configuration file or on the command
  line simply so that a marker would work.


0.2
---

* Add a marker to modify the timeout delay using a @pytest.timeout(N)
  syntax, thanks to Laurant Brack for the initial code.

* Allow the timeout marker to select the timeout method using the
  ``method`` keyword argument.

* Rename the --nosigalrm option to --method=thread to future proof
  support for eventlet and gevent.  Thanks to Ronny Pfannschmidt for
  the hint.

* Add ``timeout`` and ``timeout_method`` items to the configuration
  file so you can enable and configure the plugin using the ini file.
  Thanks to Holger Krekel and Ronny Pfannschmidt for the hints.

* Tested (and fixed) for python 2.6, 2.7 and 3.2.
