# EmbedKit_Himesh Koli

**Author:** Himesh Koli

## Build Instructions

```bash
gcc -Wall -std=c99 ringbuf.c -o ringbuf
./ringbuf
```

## Module

### ringbuf.c

A circular (ring) buffer for uint8_t data with a fixed capacity of 8 bytes,
demonstrating ISR-safe FIFO queue behaviour for embedded UART receive buffering.

---

### Line-by-line explanation

```c
#include <stdio.h>
```
Gives us printf() to print output to the terminal.

```c
#include <stdint.h>
```
Gives us fixed-width types like uint8_t and int8_t so the code works correctly
on any MCU, whether int is 8-bit, 16-bit, or 32-bit.

```c
#include <string.h>
```
Gives us memset(), used to zero out the buffer array on initialisation.

```c
#include <stdlib.h>
```
Included for completeness as a standard embedded C dependency.

```c
#define BUFFER_SIZE  8u
```
The capacity of the ring buffer -- 8 bytes. Must be a power of 2 for the
bitwise wrap trick to work. The `u` suffix makes it an unsigned literal.

```c
#define BUFFER_MASK  (BUFFER_SIZE - 1u)
```
Equals 0x07 in binary: 00000111. Used to wrap the head and tail indices back
to 0 without using the % (modulo) operator. ANDing any index with 0x07 keeps
only the lower 3 bits, which is identical to index % 8 -- but takes 1 cycle
instead of ~100 on MCUs without a hardware divider.

```c
#define RB_OK     0
#define RB_FULL  -1
#define RB_EMPTY -2
```
Return codes for rb_write() and rb_read(). Using named constants instead of
raw numbers makes the caller code readable and avoids magic numbers.

```c
typedef struct {
    uint8_t buf[BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} RingBuf;
```
The ring buffer struct. `buf` is the actual storage array. `head` tracks where
the next write goes. `tail` tracks where the next read comes from. `count`
tracks how many bytes are currently stored -- this avoids the head==tail
ambiguity between full and empty states.

```c
void rb_init(RingBuf *rb)
{
    rb->head = rb->tail = rb->count = 0;
    memset(rb->buf, 0, sizeof(rb->buf));
}
```
Resets the buffer to an empty state. Sets head, tail, and count all to zero,
then zeroes out the storage array. Must be called once before any read or write.

```c
static uint8_t is_full(const RingBuf *rb)  { return rb->count == BUFFER_SIZE; }
static uint8_t is_empty(const RingBuf *rb) { return rb->count == 0; }
```
Internal helpers that return 1 (true) or 0 (false). Marked static so they are
invisible outside this file -- they are not part of the public API. The const
keyword means these functions promise not to modify the buffer.

```c
uint8_t rb_count(const RingBuf *rb) { return rb->count; }
```
Public function that returns how many bytes are currently in the buffer.

```c
int8_t rb_write(RingBuf *rb, uint8_t byte)
{
    if (is_full(rb))
        return RB_FULL;

    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) & BUFFER_MASK;
    rb->count++;
    return RB_OK;
}
```
Writes one byte into the buffer. First checks if full -- if so, returns RB_FULL
immediately and does nothing, so existing data is never overwritten. Otherwise,
stores the byte at the current head index, advances head using the AND-mask wrap
(so it circles back to 0 after index 7), increments count, and returns RB_OK.

```c
int8_t rb_read(RingBuf *rb, uint8_t *b)
{
    if (is_empty(rb))
        return RB_EMPTY;

    *b = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & BUFFER_MASK;
    rb->count--;
    return RB_OK;
}
```
Reads one byte out of the buffer into the variable pointed to by b. First checks
if empty -- if so, returns RB_EMPTY and leaves *b untouched, so garbage is never
returned. Otherwise, copies the byte at the current tail index into *b, advances
tail with the AND-mask wrap, decrements count, and returns RB_OK.

```c
RingBuf rb;
uint8_t b;
int8_t  ret;
uint8_t i;
rb_init(&rb);
```
Inside main(): declares the buffer and working variables, then initialises the
buffer to a clean empty state before anything else touches it.

```c
uint8_t fill[] = { 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48 };
for (i = 0; i < 8; i++) { ... }
```
Writes 8 bytes (ASCII 'A' through 'H') one at a time. After all 8 writes the
buffer is full and count equals 8.

```c
ret = rb_write(&rb, 0x99);
```
Tries to write a 9th byte into a full buffer. rb_write() returns RB_FULL and
the byte is discarded -- existing data is untouched.

```c
for (i = 0; i < 3; i++) { rb_read(&rb, &b); ... }
```
Reads 3 bytes out (0x41, 0x42, 0x43 in order). Count drops from 8 to 5 and
those 3 slots are now free for reuse.

```c
uint8_t more[] = { 0x49, 0x4A, 0x4B };
for (i = 0; i < 3; i++) { rb_write(&rb, more[i]); ... }
```
Writes 3 new bytes into the 3 freed slots. Head has wrapped back to array
index 0, so these bytes physically sit at indices 0, 1, 2 -- but they still
come out in the correct FIFO order because tail is tracking independently.
This is the wrap-around behaviour that makes it circular.

```c
while (!is_empty(&rb)) { rb_read(&rb, &b); ... }
```
Drains all 8 remaining bytes one at a time until the buffer is empty.

```c
ret = rb_read(&rb, &b);
```
Attempts one final read on the now-empty buffer. rb_read() returns RB_EMPTY
and prints the failure message, confirming the guard works correctly.

```c
return 0;
```
Returns 0 to the OS, indicating the program exited successfully.
