#!/usr/bin/env bash
set -ex

KLOG=2 mkn clean build run -dtOa "-std=c++20 -fPIC"
