Compile for Windows on Linux / OS X
===================================

Prerequistes
------------

Have a look at the [tutorial](http://mxe.cc).

	git clone -b stable https://github.com/mxe/mxe.git
	cd mxe
	make gcc curl

Add MXE to your path, e.g. by adding the following lines
to your $HOME/.bashrc:

	# Update PATH for MXE (MINGW cross development environment)
	export PATH=<path-where-you-installed-mxe>/usr/bin:$PATH


Compiling
---------

To compile and generate a distribution zip-file:

	make WIN=y dist

