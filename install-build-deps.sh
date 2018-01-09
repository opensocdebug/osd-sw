#!/bin/bash
#
# Install development build dependencies for different Linux distributions
#

case $(lsb_release -is) in
  Ubuntu)
    # Ubuntu seems to have a rather strange and inconsistent naming for the
    # ZeroMQ packages ...
    sudo apt-get install \
      check \
      doxygen \
      python3 python3-venv python3-pip \
      lcov valgrind \
      libzmq5 \
      libzmq3-dev \
      libzmq5-dbg \
      libczmq-dev \
      libczmq-dbg \
      xsltproc \
      libelf1 libelf-dev
    ;;

  *SUSE*)
    sudo zypper install \
      libcheck0 check-devel libcheck0-debuginfo \
      doxygen \
      python3 python3-pip \
      lcov valgrind \
      zeromq-devel zeromq \
      czmq-devel czmq-debuginfo \
      xsltproc \
      libelf1 libelf-devel
    ;;

  *)
    echo Unknown distribution. Please extend this script!
    exit 1
    ;;
esac
