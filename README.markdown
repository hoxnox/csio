CSIO is Compressed Stream Input Output library.

The main aim of the library - bring transparent stdio-like interface for
compressed files. So you can open compressed file and use fseek, fread,
fgetc, ... as it is not compressed.

ZLIB has something similar (`gz_seek`, `gz_read`, ...), but it is
extremely slow and non-efficient.

First version of the library will support dictzip (will be described in
the documentation) and plain texts only? then bzip2 anf gzip will be added.
