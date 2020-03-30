#!/bin/bash
#
# Install development build dependencies for different Linux distributions
#

set -e

[ -f /etc/os-release ] || (echo "/etc/os-release doesn't exist."; exit 1)
. /etc/os-release

SUDO_CMD=""
if [ $(id -u) -ne 0 ]; then
  SUDO_CMD="sudo "
fi

case "$ID-$VERSION_ID" in
  ubuntu-16.04)
    # Ubuntu seems to have a rather strange and inconsistent naming for the
    # ZeroMQ packages ...
    $SUDO_CMD apt-get install -y \
      check \
      doxygen \
      python3 python3-venv python3-pip \
      tox \
      lcov valgrind \
      libzmq5 \
      libzmq3-dev \
      libzmq5-dbg \
      libczmq-dev \
      libczmq-dbg \
      xsltproc \
      libelf1 libelf-dev zlib1g zlib1g-dev \
      lsb-release
    $SUDO_CMD pip3 install -r src/python/dev-requirements.txt
    ;;

  ubuntu-18.04)
    # Ubuntu seems to have a rather strange and inconsistent naming for the
    # ZeroMQ packages ...
    $SUDO_CMD apt-get install -y \
      check \
      doxygen \
      python3 python3-venv python3-pip \
      tox \
      lcov valgrind \
      libzmq5 \
      libzmq3-dev \
      libczmq4 \
      libczmq-dev \
      xsltproc \
      libelf1 libelf-dev zlib1g zlib1g-dev \
      lsb-release
    $SUDO_CMD pip3 install -r src/python/dev-requirements.txt
    ;;

  *suse*)
    $SUDO_CMD zypper install \
      libcheck0 check-devel libcheck0-debuginfo \
      doxygen \
      python3 python3-pip \
      python3-tox \
      lcov valgrind \
      zeromq-devel zeromq \
      czmq-devel czmq-debuginfo \
      libxslt-tools \
      libelf1 libelf-devel \
      lsb-release
    $SUDO_CMD pip3 install -r src/python/dev-requirements.txt
    ;;

  *)
    echo Unknown distribution. Please extend this script!
    exit 1
    ;;
esac
