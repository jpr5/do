## 0.10.17 2016-01-24

No changes

## 0.10.16 2015-05-17

No changes

## 0.10.15 2015-02-15

No changes

## 0.10.14 2014-02-13

* Don't do DNS lookup in transaction loading

## 0.10.13 2013-05-27

No changes

## 0.10.12 2013-01-21

* jdbc-h2 driver loading fix

## 0.10.11 2012-12-29

No changes

## 0.10.10 2012-10-11

No changes

## 0.10.9 2012-08-13

No changes

## 0.10.7 2011-10-13

No changes

## 0.10.6 2011-05-22

No changes

## 0.10.5 2011-05-03

No changes

## 0.10.4 2011-04-28

New features
* Add save point to transactions (all)
* JRuby 1.9 mode support (encodings etc.)

Bugfixes
* Fix bug when using nested transactions in concurrent scenarios (all)
* Use column aliases instead of names (jruby)

Other
* Switch back to RSpec

## 0.10.3 2011-01-30
* No changes

## 0.10.2 2010-05-19
* No changes

## 0.10.1 2010-01-08

* Switch to Jeweler for Gem building tasks (this change may be temporary).
* Switch to using Bacon for running specs: This should make specs friendlier to
  new Ruby implementations that are not yet 100% MRI-compatible, and in turn,
  pave the road for our own IronRuby and MacRuby support.
* Switch to the newly added rake-compiler `JavaExtensionTask` for compiling
  JRuby extensions, instead of our (broken) home-grown solution.

## 0.10.0 2009-09-15

Initial release of H2 driver (using *do_jdbc*).

* Known Issues
  * JRuby-only
