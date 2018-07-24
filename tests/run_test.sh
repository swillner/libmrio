#!/bin/sh -e
if "$@" settings.yml
then
    diff output_indices.csv correct_indices.csv
    diff output_data.csv correct_data.csv
else
    return 1
fi
