# miniz

Deflate and inflate for sysl, bound to [miniz](https://github.com/richgel999/miniz) — with no
allocator anywhere, including for the compressor's own working state.

*An independent sysl package for miniz. It is not affiliated with or endorsed by miniz's authors or trademark holders.*

```sysl
import sh.sysl.miniz.{DEFLATE_BYTES, INFLATE_BYTES, deflate, inflate, bound}

var work: [DEFLATE_BYTES]u8 = [0; DEFLATE_BYTES]
var packed: []u8 = [0; 512]

val n = deflate(work, text.bytes, packed).unwrap()
```

Everything is a caller-supplied slice: the input, the output, and the state the codec runs in. That
is what makes the package usable where there is no heap, and it is also what makes its shape unusual
— a `Deflater` is not something you create, it is something you *place* in storage you already have.

**The one exception is `Decoder`**, for a stream whose length nobody knows — an HTTP body, gzip or
zlib, decoded as it arrives. It keeps the last 32 KB of output to resolve back references against,
and it owns that memory itself. A program that never makes one allocates nothing.

## The two halves are not the same size

This is the most useful thing to know about the library, and it decides how a project should be
arranged:

| | state | fits an RP2040 (264 KB)? | fits an RP2350 (520 KB)? |
|---|---|---|---|
| inflate | **8,383 bytes** | comfortably | comfortably |
| deflate | **167,807 bytes** | no | yes, at a third of RAM |

So the expected arrangement is asymmetric: **compress on a host, inflate on the device.** A firmware
image, a font, a level, a page of help text — packed by a build step, unpacked where it is used.
Both halves are here because both are sometimes wanted, not because they are equals.

The compressor is already the smaller of miniz's two configurations. `TDEFL_LESS_MEMORY` is on,
which trades slightly worse ratios for 151,552 fewer bytes; without it the state is 319,352.

## What you get

| | |
|---|---|
| `deflate(work, src, dst, level, format)` | one call, answers bytes written |
| `inflate(work, src, dst, format)` | one call, answers bytes written |
| `deflater(work, …)` / `Deflater.push(src, dst, finish)` | feed input in chunks |
| `inflater(work, …)` / `Inflater.push(src, dst, more)` | take the compressed stream in chunks |
| `decoder(wrapper)` / `Decoder.feed(chunk, out)` / `.finish()` | chunks in, chunks out — raw, zlib or gzip, or detected |
| `bound(len)` | the largest a compressed stream can be |
| `crc32(bytes)` / `adler32(bytes)` | useful without compressing anything |

`Level` is `Fastest`, `Fast`, `Default` or `Best` — miniz spends its effort looking for earlier
matches, and the count of attempts is the whole knob. `Format` is `Zlib` (a two-byte header and a
trailing adler-32, which the inflator verifies for you) or `Raw` (neither, six bytes shorter, for a
container that already frames and checks its contents).

### The streaming shape is the one an update over a wire wants

`Inflater` takes the destination whole and the *input* in pieces, which is the way round that
matters: the size of what you are unpacking is known — it is in a header of your own, or it is the
fixed size of the thing — while the compressed bytes arrive a packet at a time.

```sysl
var work: [INFLATE_BYTES]u8 = [0; INFLATE_BYTES]
var image: []u8 = [0; 65536]

var inf = inflater(work).unwrap()

while val chunk = next_packet()
    inf.push(chunk, image, more_expected).unwrap()

assert(inf.done)
```

Passing `more` while further chunks are expected is what turns "waiting for input" into a
`Truncated` on the last one.

### A stream of unknown length: `Decoder`

A response body is the other way round from a firmware update: nothing says how large the answer is,
it may be gzip rather than zlib, and the program wants the decoded bytes as they come rather than
after the last packet. A `Decoder` appends what each chunk produces to a `Buf[u8]` of yours and
answers how many bytes that was:

```sysl
val d = decoder()
var body: Buf[u8] = buf()

for chunk in chunks do d.feed(chunk, body)?

d.finish()?
```

- **`Wrapper`** is `Raw`, `Zlib`, `Gzip`, or `Detect` (the default), which tells zlib from gzip by
  the first byte, as zlib's `windowBits = 15 + 32` does.
- **A chunk may end anywhere**: inside the gzip header, in the middle of a Huffman code, between the
  two halves of the trailer. Every split of every test stream is checked to decode identically, from
  one byte at a time to the whole thing at once.
- **gzip is parsed here, not by miniz**, which reads only raw deflate and zlib. The header's optional
  fields (FEXTRA, FNAME, FCOMMENT) are read past, FHCRC is checked when it is present, and the
  trailer's CRC-32 and ISIZE are compared with what was actually decoded.
- **`finish` is what turns a stream that stopped into `Truncated`.** `feed` cannot know, since more
  might come.
- **An error sticks**: every later `feed` and `finish` answers it again.
- It is a `&Decoder`, and its memory goes with the last reference to it. Nothing in it is a C
  allocation, so there is no destructor and nothing to close.

## What the errors say

A binding earns its keep here, because miniz reports a short output buffer and a corrupt stream as
two negative integers a caller has to know apart.

| | |
|---|---|
| `Short(needed)` | compressing: `dst` too small. `needed` is `bound(src.len)` — a ceiling, since miniz cannot say what it would really have taken |
| `Full` | inflating: `dst` too small, with no figure to give, because the length of what remains is exactly what a compressed stream does not carry |
| `Truncated` | the input stopped mid-stream. **Half a download, not a corrupt one** — the difference matters to whatever is retrying |
| `Corrupt` | not a valid stream, or the wrapper's checksum disagreed with the data |
| `NoRoom(needed)` | the storage slice is smaller than `DEFLATE_BYTES` or `INFLATE_BYTES` |
| `Trailing` | bytes after the end of the stream (for gzip, after a member and not starting another) — from `inflate` and from a `Decoder` |
| `BadHeader` | a gzip header that is not one: wrong magic, a method other than deflate, a reserved flag, or an FHCRC that disagrees |
| `BadCrc` / `BadLength` | a gzip trailer's CRC-32 or ISIZE disagrees with what was decoded |

**Multi-member gzip decodes (0.3.1).** `cat a.gz b.gz` is a valid gzip file, and node's
`gunzipSync`, `DecompressionStream("gzip")` and zlib's `gunzip` all answer the two outputs joined;
a gzip `Decoder` now does too, where 0.3.0 refused it as `Trailing`. Each member's header, CRC and
ISIZE are checked on their own, a chunk may end anywhere (between members included), and `is_done`
is true between members, since the stream may end there. `finish` mid-way through a later member is
`Truncated`.

**Other bytes after the end are an error, not something to skip.** A byte after a gzip member that
is not the start of another (node answers `Z_DATA_ERROR` for junk), and any byte after a zlib or raw
stream, is `Trailing`. Zero padding, which node and gzip(1) tolerate, is `Trailing` here too. The
whole-buffer `inflate` reads raw and zlib only, so it has no members and is unchanged.

`Error` derives `Eq`, so a result can be compared with `==`.

`Truncated` and `Corrupt` are genuinely distinguished, and each has a test that would fail if the
other were returned.

## What is still hosted about it

The package asks nothing of sysl — no allocator, no clock, no file, no operating system — and
`requires {}` would be exact, but for `Decoder`, which allocates its ring and state and costs nothing
to a program that does not make one. **That is not the same as compiling for a bare target, and the two get
conflated constantly.**

`miniz.h` includes `<assert.h>`, `<stdlib.h>` and `<string.h>` unconditionally. clang gives a
freestanding target `limits.h`, `stdint.h`, `stddef.h` and `stdbool.h` and none of those three, so:

```
sysl build-lib . --target thumb-freestanding
error: the C compiler refused this file's 'c const' block:
miniz.h:555:10: fatal error: 'assert.h' file not found
```

That was run, not reasoned about. A target **with a libc** is fine, and that covers the interesting
ones: the Pico SDK carries newlib, as do Zephyr and FreeRTOS applications.

## How it is put together

```
sh/sysl/miniz/
  miniz.sysl        what a program calls: Deflater, Inflater, Result
  stream.sysl       Decoder, and the gzip header and trailer
  tests.sysl
  stream_tests.sysl against streams zlib, Python and gzip(1) wrote
  c/
    miniz.c         upstream 3.0.2, unmodified
    miniz.h         upstream 3.0.2, one #define guarded
    shim.c          three functions sysl cannot reach
    c.sysl          the declarations, read out of the header
    tests.sysl      that sysl and clang agree about miniz
```

**The options are a `defines` block in `package.hocon`**, which needs sysl 0.0.65 or later:

```hocon
defines {
  "sh/sysl/miniz/c/{miniz,shim}.c" { MINIZ_NO_MALLOC = true, … }
}
```

Every one of them changes either the size of a state struct or which symbols exist, so the
implementation, the shim and the probe sysl synthesises for a `c const` block have to agree. Two
things arrange that. The braced key gives both C files **one** copy of the list, so there is nothing
to drift; and a probe inherits the macros of the carried C in its own directory, so it agrees with
the objects without being named at all.

Until 0.1.1 they lived in an `options.h` that all three included, because nothing supplied a `-D` to
a package's own C. The manifest does now, and the wrapper is gone with it.

**One line of upstream is still edited, and no mechanism removes it.** `miniz.h` defines
`TDEFL_LESS_MEMORY 0` unguarded, so no definition made before including it survives — not from a
manifest, not from a `-D`, which earns a `macro redefined` warning and no other effect. The vendored
copy carries an `#ifndef` around that line, and it is worth 151,552 bytes of compressor state.

Two layers, because they answer to different things. The one in `c/` has to be **faithful** — a
signature disagreeing with the header links perfectly and corrupts the call at run time — and the one
above it has to be **pleasant**, which is a different question that would otherwise get answered in
the same breath.

**Two things need the shim.** `tinfl_init` and `tinfl_get_adler32` are macros rather than functions,
so there is no symbol to link. Every other function in the deflate and inflate surface is declared
straight out of `c.sysl`.

There were three until 0.2.2, and the third is worth recording rather than quietly dropping: the
alignment arithmetic was written in C on the stated grounds that *sysl has no pointer-to-integer
cast*. **It has had one since v0.0.10** — `usize(p)` is an ordinary conversion, total and lossless,
`ptr_cast` reads an address back, and `alignof(T)` gives the boundary. The claim was wrong the day it
was written rather than made wrong by anything since, and it is the reason to probe a package
comment before repeating it as a language limit. The two functions are `sysl.slices.align_up` and
`sysl.slices.is_aligned` now, written in sysl.

What they are for has not changed, and it is why the exported byte counts are a `sizeof` plus seven:
a state holds pointers, a `[]u8` promises nothing about its address, so the state is placed at the
first aligned offset inside whatever slice it is given rather than trusting that the slice was
aligned. Every one observed while writing this package happened to be — which is not the same as
being guaranteed one, and the suite now feeds a deliberately misaligned slice to say so.

## Tests

`sysl test .` — 47 tests, both layers.

The `Decoder`'s oracle is the formats and the tools that write them: a zlib and a raw stream from
Python's `zlib`, a gzip file from the system's `gzip -9`, and a gzip header built by hand with
FEXTRA, FNAME, FCOMMENT and FHCRC all set, which `gzip -t` and Python's `gzip.decompress` both accept.
Each is decoded at every chunk size, every prefix of each is `Truncated`, and a megabyte of zeros
comes back through the 32 KB ring from a stream under 2 KB.

The one worth knowing about: miniz round-tripping its own output proves the binding is
self-consistent, not that it is *right*. So the suite also inflates 36 bytes **zlib** produced from
the same input, and cross-checks `crc32` against zlib's. Note the two implementations compress that
input to different 36 bytes — same length, different header level bits and different match choices —
which is why what is asserted is what a stream decompresses to, and never what a compressor emits.

## Licence

miniz is MIT, © 2013-2014 RAD Game Tools and Valve Software, © 2010-2014 Rich Geldreich and Tenacious
Software LLC. `LICENSE` is upstream's, and it covers the vendored `miniz.c` and `miniz.h`.
