#!/bin/sh
# Builds and runs the console app. Stops at a failed build, so a stale
# mb_app is never run in its place.
set -e

# remove old executable (-f: a fresh clone has none yet)
rm -f ./mb_app

# compile
make mbapp

# run
./mb_app
