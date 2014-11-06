#!/bin/bash

FILENAME=$1
echo $FILENAME ${@:2} | tr [:blank:] =
