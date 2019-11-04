# Changelog

## v0.3.0

* Improvements
  * Add dithering support for monochrome conversions. See
    `RpiFbCapture.set_dithering/2`. Thanks to Gabriel Roldán for this feature!
  * Add regression tests for all conversions

* Bug fixes
  * Fixed port crash when sending back-to-back requests.

## v0.2.1

* Improvements
  * Move C build products under `_build`

## v0.2.0

* New features
  * Add configurable threshold for monochrome conversion
  * Add save utility function

* Bug fixes
  * Fixed RGB24 conversion - the ppm files were greenish

## v0.1.0

Initial release to hex.
