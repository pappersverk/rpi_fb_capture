#!/bin/sh

set -e

make

echo "put priv/rpi_fb_capture /root" | sftp -b - nerves.local
