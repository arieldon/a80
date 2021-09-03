# a80
a80 is a simple and naive assembler for the Intel 8080.


## Obligatory Disclaimer
a80 is a toy. It lacks otherwise obvious data structures, i.e.  hash
tables, opting for absurdly long conditional chains instead.
Nonetheless, it works (by and large) -- I built it to learn about
assembly, less so data structures. It follows that this assembler is not
inherently fast, as nothing about this assembler is optimized to be
fast. That being said, it functions at a reasonable pace on most modern
machines.


## Description
As noted above, a80 is an assembler for the Intel 8080 -- a
little-endian 8-bit microprocessor. Its instruction set architecture
maps mnemonics to machine code, each no greater than 3 bytes in size.

An assembler such as a80 translates (almost) English instructions into
object code.

- `nop` becomes `0x00`.
- `mov a, b` becomes `0x78`.
- `jnz 0x0107` becomes `0xc2 0x07 0x01`.

And so on.

An assembly program consists of a series of these instructions, where
each line represents an independent instruction. The 8080 supports
programs no larger than 64 KB -- an 4x increase compared to its
predecessor the Intel 8008.

a80 requires two passes of the input assembly. During the first pass,
a80 parses each instruction and stores the address of any labels it
encounters. During the second pass, a80 again parses each instruction,
translating each piece to executable machine code and storing it in an
array for later output.

Upon failure, a80 reports the line number of the source of error in the
assembly file along with a terse diagnosis. Otherwise, a80 outputs an
executable file that requires an 8080 or an 8080 emulator to execute.


## Credit
a80 is heavily inspired by, well, [a80](https://github.com/ibara/a80) --
an assembler written in D by [Dr. Robert Brian
Callahan](https://briancallahan.net/). I followed his series of blog
posts to create a similar assembler in C.
