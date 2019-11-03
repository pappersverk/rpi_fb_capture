#!/bin/sh

# Helper to convert output to .png files for easier viewing

for mono_file in $(ls *.mono); do
    echo Converting $mono_file...
    convert -size 64x48 -depth 1 $mono_file $mono_file.png
done

for rgb24_file in $(ls *.rgb24); do
    echo Converting $rgb24_file...
    convert -size 64x48 -depth 8 rgb:$rgb24_file $rgb24_file.png
done

for rgb565_file in $(ls *.rgb565); do
    echo Converting $rgb565_file...
    convert -size 64x48 $rgb565_file $rgb565_file.png
done

