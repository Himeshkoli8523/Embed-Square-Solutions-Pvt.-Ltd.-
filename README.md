# EmbedKit_Himesh - Ring Buffer

A circular buffer for `uint8_t` data, written in C99.
Built around the classic UART pattern: ISR writes bytes in, main loop drains them out.

---

## Table of Contents

1. [Project Status](#project-status)
2. [Background](#background)
3. [Dependencies](#dependencies)
4. [Build and Run](#build-and-run)
5. [API Reference](#api-reference)
6. [Design Decisions](#design-decisions)
7. [Expected Output](#expected-output)
8. [Platform Notes](#platform-notes)
9. [Known Limitations](#known-limitations)
10. [License](#license)

---

## Project Status

Works. Compiles clean with `gcc -Wall -std=c99` -- zero errors, zero warnings.
Tested on Linux (gcc 11) and Windows (MinGW / WSL).
No cross-compiler or embedded hardware needed to run the demo.

---

## Background

The problem this solves: a UART ISR fires every time a byte arrives and needs
somewhere to put it. The main loop is busy doing other things and can't always
read immediately. Without a buffer in between, bytes get dropped.

```
Producer (UART ISR)          Consumer (main loop)
      |                              |
      v                              v
[ 0x41 | 0x42 | 0x43 | ... | 0x48 ]   <-- 8-slot circular array
   ^tail                       ^head
```

Two indices move through the array:

- head -- where the next write lands
- tail -- where the next read comes from

Both wrap back to 0 when they hit the end, which is why it's called circular.
The buffer absorbs bursts from the ISR and lets the application drain at its
own pace without losing anything.

---

## Dependencies

| Requirement | Detail |
|-------------|--------|
| C standard  | C99    |
| Compiler    | gcc 9+ |
| Headers     | `<stdio.h>` `<stdint.h>` `<string.h>` `<stdlib.h>` |

No external libraries. No RTOS. No hardware.

---

## Build and Run

```bash
# compile
gcc -Wall -std=c99 ringbuf.c -o ringbuf

# run
./ringbuf          # Linux / macOS / WSL
ringbuf.exe        # Windows (MinGW)
```

On Windows if gcc is not on your PATH yet:

```powershell
$env:PATH += ";C:\MinGW\bin"
gcc -Wall -std=c99 ringbuf.c -o ringbuf
.\ringbuf.exe
```

---

## API Reference

All functions take a pointer to a `RingBuf`. Return codes: `RB_OK (0)`,
`RB_FULL (-1)`, `RB_EMPTY (-2)`.

### rb_init

```c
void rb_init(RingBuf *rb);
```

Zeroes out head, tail, and count. Call this once before anything else.

```c
RingBuf rb;
rb_init(&rb);
```

### rb_write

```c
int8_t rb_write(RingBuf *rb, uint8_t byte);
```

Puts one byte into the buffer. Returns `RB_FULL` and does nothing if there's
no room -- it will never silently clobber data that hasn't been read yet.

```c
if (rb_write(&rb, 0x41) == RB_FULL) {
    /* handle it -- set an error flag, blink an LED, whatever makes sense */
}
```

### rb_read

```c
int8_t rb_read(RingBuf *rb, uint8_t *b);
```

Pulls one byte out into `*b`. Returns `RB_EMPTY` if there's nothing to read --
it will never hand back stale or uninitialised data.

```c
uint8_t byte;
if (rb_read(&rb, &byte) == RB_OK) {
    process(byte);
}
```

### rb_count

```c
uint8_t rb_count(const RingBuf *rb);
```

Returns how many bytes are currently sitting in the buffer (0 to BUFFER_SIZE).

```c
printf("bytes waiting: %u\n", rb_count(&rb));
```

### is_full / is_empty

```c
static uint8_t is_full(const RingBuf *rb);
static uint8_t is_empty(const RingBuf *rb);
```

Internal helpers, not exposed outside `ringbuf.c`. Return 1 or 0.

---

## Design Decisions

### & instead of %

```c
/* what you'd write first -- works, but % is slow */
head = (head + 1) % BUFFER_SIZE;

/* what's actually in the code -- one AND instruction */
head = (head + 1) & BUFFER_MASK;   /* BUFFER_MASK = 0x07 */
```

Most small MCUs (Cortex-M0, AVR) have no hardware divider. The compiler has to
fake division using a software routine that takes roughly 20-100 clock cycles.
A bitwise AND is always a single instruction. In a UART ISR that has to finish
in microseconds, that difference matters.

The catch: it only works when BUFFER_SIZE is a power of 2. When size is 2^N,
the mask (2^N - 1) is exactly N ones in binary, so ANDing with it is the same
as taking the remainder. Try it with a non-power-of-2 size and you get the
wrong index.

### Separate count field

The obvious alternative is to infer full/empty by comparing head and tail.
The problem: both conditions look the same -- head == tail -- so you can't tell
them apart without adding an extra flag anyway. A dedicated count field sidesteps
the whole issue and keeps is_full() and is_empty() as readable one-liners.

### static helpers

is_full() and is_empty() are marked static so they stay invisible outside this
translation unit. They're just there to make rb_write() and rb_read() easier to
read -- not meant to be called directly.

### Fixed-width integer types

Everything uses uint8_t and int8_t from stdint.h instead of plain int or char.
On a 16-bit target, a plain int costs twice the register space it needs to.
Using the explicitly-sized types means the code means what it says on any arch.

---

## Expected Output

```
[WRITE] 0x41 -> OK (count=1)
[WRITE] 0x42 -> OK (count=2)
[WRITE] 0x43 -> OK (count=3)
[WRITE] 0x44 -> OK (count=4)
[WRITE] 0x45 -> OK (count=5)
[WRITE] 0x46 -> OK (count=6)
[WRITE] 0x47 -> OK (count=7)
[WRITE] 0x48 -> OK (count=8) FULL
[WRITE] 0x99 -> FAIL (buffer full)
[READ]  -> 0x41 (count=7)
[READ]  -> 0x42 (count=6)
[READ]  -> 0x43 (count=5)
[WRITE] 0x49 -> OK (count=6)
[WRITE] 0x4A -> OK (count=7)
[WRITE] 0x4B -> OK (count=8) FULL
[READ]  -> 0x44 (count=7)
[READ]  -> 0x45 (count=6)
[READ]  -> 0x46 (count=5)
[READ]  -> 0x47 (count=4)
[READ]  -> 0x48 (count=3)
[READ]  -> 0x49 (count=2)
[READ]  -> 0x4A (count=1)
[READ]  -> 0x4B (count=0)
[READ] (empty) -> FAIL (buffer empty)
```

The interesting part is step 4. After reading three bytes, the three freed
slots are reused for 0x49, 0x4A, 0x4B -- head has wrapped back to array
index 0. They still come out in the right order because tail is tracking
independently. That's the whole point of the circular design.

---

