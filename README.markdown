# CSIO

Compressed Stream Input Output library brings you random access through
gzip file. And effective multithread gzip compressor.

The main aim of the library - bring transparent stdio-like interface for
compressed files. So you can open compressed file and use fseek, fread,
fgetc, ... as it is not compressed.

ZLIB has something similar (`gz_seek`, `gz_read`, ...), but it is
extremely slow and non-efficient.

As a bonus comes ability to compress data in many threads. Resulting
file has valid gzip structure, so you can decompress it with gzip.

Sources includes dzip utility - it is **multithreaded gzip**. With
option `-j` you can specify count of threads.

# Easy to use

You just need to replace FILE with CFILE and all stdio functions with
it's csio analogues:

- fread -> cfread
- fseeko -> cfseek
- ftello -> cftello
...

Library works with uncompressed data too, so you don't need to compress
all your legacy data.

# Benchmark

When you are working with compressed file it is easier for operation
system to cache it (it is smaller), so when you use traditional HDD (not
SSD) csio can be even faster in multiply random access, than stdio.

I used 3 files. Every file 1585741824 bytes (~1.5GB):

1. File with random data. It doesn't compressible. Let's call it bin.
2. Random letters and numbers. It is compressible, but not good. Let's
   call it letter.
3. ietf HTML archive. It is very compressible. Let's call it HTML.

## Compression ratio

Name   | Uncompressed size | Compressed dzip size and ratio  | Compressed gzip size and ratio
------ | ----------------- | ------------------------------- | ------------------------------
bin    | 1585741824        | 1586476074(1.00046)             | 1585999044(1.00016)
letter | 1585741824        | 1192731126(0.75215)             | 1191369320(0.75130) 
html   | 1585741824        | 376986553(0.23773)              | 344144149(0.21702)

As you can see compression ratio is very close to native gzip.

## Random access speed compare with stdio

I wrote test program. It generates random access map (random reads count
between 9000 and 10000, random block with size between 10 and 10000
bytes, from random part of file). Than it reads parts of file by this
map with fread and cfread. We could compare how long does it takes to
read the whole map by stdio and csio. Source code is in `test` directory.

Machine:  Intel i5, 16Gb, Linux, ext4.

Before test I flushes buffers: 

	echo 3 >> /proc/sys/vm/drop_caches

Every test I had launched three times and took arithmetic mean:

### SSD (Intel)

- bin - csio is slower then stdio in 1.96368 times
- letter  - csio is slower then stdio in 1.85832 times
- html - csio is slower then stdio in 1.08563 times 

### HDD (2.5'' 5200)

- bin - csio is faster in 1.24917 times
- letter - csio is faster in 1.51602 times
- html - csio is faster in 1.84706 times 

CSIO is definitely useful for compressible data.
