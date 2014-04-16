////////////////////////////////////////////////////////////////////////
// index.h
// Author: hoxnox <hoxnox@gmail.com>
////////////////////////////////////////////////////////////////////////

/**

@image html main_logo.png
@author hoxnox <hoxnox@gmail.com>
@date 20140413 08:15:17

@mainpage csio

@tableofcontents

@section dzip_structure DZIP file structure

	+=============+=============+ ... +=============+
	|DZIP_MEMBER_1|DZIP_MEMBER_2|     |DZIP_MEMBER_N|
	+=============+=============+ ... +=============+

DZIP_MEMBER:

	+---+---+---+---+---+---+---+---+---+---+---+---+
	|x1F|x8B|x08|FLG|     MTIME     |XFL|OS | XLEN  |->
	+---+---+---+---+---+---+---+---+---+---+---+---+
	+===========+===========+======+
	| RA_EXTRA  | FNAME     | BODY |
	+===========+===========+======+

	FLG      - flags. FEXTRA|FNAME is used
	MTIME    - modification time of the original file (filled only
	           for first member, other members has 0)
	XFL      - extra flags about the compression.
	OS       - operating system
	XLEN     - total extra fields length (RA_EXTRA)
	RA_EXTRA - RFC1952 formated Random Access header's extra field (later)
	FNAME    - zero terminated string - base (without directory)
	           file name (filled only for the first member, others
	           has zero-length FNAME)
	BODY     - see below
	CRC32    - CRC-32
	SIZE     - data size in this member (unpacked)

RA_EXTRA:

	+---+---+---+---+---+---+---+---+---+---+============+
	|x52|x41| EXLEN | VER=1 | CHLEN | CHCNT | CHUNK_DATA |
	+---+---+---+---+---+---+---+---+---+---+============+

	EXLEN      - length of VER, CHLEN, CHCNT and CHUNK_DATA summary
	CHUNK_DATA - CHCNT 2-bytes lengths of compressed chunks
	CHLEN      - the length of one uncompressed chunk
	CHCNT      - count of 2-bytes lengths in CHUNK_DATA

Only first member has valid MTIME and FNAME.

BODY:

	+==========+=...=+==========+==========+---+---+---+---+---+---+---+---+
	| CCHUNK_1 |     | CCHUNK_N | Z_FINISH | CRC32         | SIZE          |
	+==========+=...=+==========+==========+---+---+---+---+---+---+---+---+

	CCHUNK - Z_NO_FLUSH compressed chunk of file (size - CHLEN), then
	         Z_FULL_FLUSH zlib data. So we can work with the chunk
	         (inflate/deflate) independently.
	CRC32  - CRC32 check sum of uncompressed member data.
	SIZE   - size of the uncompressed member data.


@section compressor_arch Compressor architecture

Compressor includes only one CompressManager
(responds for file reading, message transfer between Compressors
and Writer, general management) - the heart of the compressor,
several Compressors and one Writer. Instances of these types works in
separated threads (it can be easily modified to work on separate
processes and even computers) and communicate only through the message
system [zeromq](http://zeromq.org).

Communication diagram:

	+-------------------+              +--------+
	| CompressManager   |PAIR------PAIR| Writer |
	+-------------------+              +--------+
	 PULL(feedback) PUSH(jobs)
	  |              |
	  |     +--------+--- ... ---+
	  |     |        |           |
	  |    PULL     PULL       PULL
	  |  +------+ +--*---+   +------+
	  |  |Cmprs | |Cmprs |   |Cmprs |
	  |  +------+ +------+   +------+
	  |    PUSH     PUSH       PUSH
	  |     |        |          |
	  +-----+--------+----------+

CompressManager has two boxes - inbox and outbox to communicate with
Compressors pool. Compressors can take messages from the outbox
and put into inbox. Between Writer and CompressManager there are
duplex communication channel. Message bodies has the following structure:

	+---+---...---+
	|TYP| PAYLOAD |
	+---+---...---+

There are several message types:

- TYPE_INFO    messages are:
               - MSG_STOP  - used to indicate instance stopping
               - MSG_READY - used by Writer and Compressors to indicate,
                             that they have been started
               - MSG_ERROR - used to indicate error in the instance
- TYPE_MCLOSE  informs Writer to finish member block and fill skipped
               chunks lengths in the header. Message contains Z_FINISH,
               crc32 and member size in it's body.
- TYPE_MHEADER contains member header where chunks lengths are filled
               with zero values.
- TYPE_FCHUNKS contains compressed file chunk.

- TYPE_FCHUNKS messages includes SEQ number in it's body. This number
               helps order messages in the ordering set.
 
TYPE_FCHUNK message structure 

	+---+---+---+======+
	|TYP| SEQ   | DATA |
	+---+---+---+======+

	TYP  - Messages::TYPE_FCHUNK
	SEQ  - Sequence number (little endian)

@section comprssor_logic Compressor logic

CompressManager starts several Compressors and the Writer. When child
threads ready, they send MSG_READY to CompressManager and the
compression begins. Manager makes initial push - puts into the outbox
as many data messages as many Compressors are and sends service data
message with the member header to the Writer. The member header has
zeroes in place, where must be chunks lengths. Writer must fill it
later - when TYPE_MCLOSE arrives. When initial push finished,
CompressManger just reacts to the incoming messages. When arrives
MSG_STOP, or MSG_ERROR it must broadcast MSG_STOP to all
children. When there are some messages in the inbox, CompressManager
makes general cycle.

General cycle includes:

#. Put compressed file chunk into the ordering set. There is
   additional logic that will prevent CompressManager from
   producing data messages into the outbox (pushing messages)
   while the ordering set is very big.
#. If ordering set is ready, flush it into the Writer.
#. If the end of member reached, CompressManager stops pushing
   messages to Compressors until they all finish their current
   jobs, flushes ordering set and sends to the Writer 
   TYPE_MCLOSE and TYPE_MHEADER.
#. If CompressManager is not prevented, it pushes data messages
   into the outbox. The count of pushed messages depends on how
   many free Compressors are in the compressors pool (there may
   be not one, because of prevented pushing mechanism).

Compressors and Writer are very simple. Compressor must inflate
incoming file chunk. Writer must write data into the file, carry on
the array of the current member file chunks lengths and write it into
header by the signal.

Because CompressManger do his work only as reaction to the incoming
message, the situation when all Compressors are free and there is no
messages in the outbox will hang compression. Current compressors load
can be controlled though the msg_pushed_ variable, which indicates
count of messages in the compression process.

@li @subpage pagename

*/
