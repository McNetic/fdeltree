fdeltree
========

Simple, fast recursive file deletion utility for Windows

Copyright (C) 2015 Nicolai Ehemann (en@enlightened.de)

This program is distributed WITHOUT AND WARRANTY.
Licensed under the GNU GPL. See COPYING for more information.

Short introduction
------------------

All existing utilities I could find either could not handle long pathnames (for example del /s /f /q) or are very slow. If you need to delete a few hundreds of thousands (or even millions) of files within reasonable time, this utility is the way to go.

USAGE:

fdeltree [options] <directory>
options:
  /h  -  print this help
  /v  -  verbose (print statistics every few seconds)
  /f  -  force deleting of read-only files

