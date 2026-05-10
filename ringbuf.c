/*
 * ringbuf.c - circular buffer for UART byte buffering
 *
 * head = next write slot, tail = next read slot
 * using & instead of % for wrap since BUFFER_SIZE is power of 2
 * (no hardware divider on most MCUs, % compiles to ~100 cycle software divide)
 *
 * gcc -Wall -std=c99 ringbuf.c -o ringbuf
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE  8u
/*
 * BUFFER_MASK = 0x07
 * (head + 1) & BUFFER_MASK wraps 0..7 back to 0.
 * only works when BUFFER_SIZE is a power of 2 -- the mask lines up
 * perfectly with the modulus boundary. try it with 6 and it breaks.
 */
#define BUFFER_MASK  (BUFFER_SIZE - 1u)

#define RB_OK        0
#define RB_FULL     -1
#define RB_EMPTY    -2

typedef struct {
    uint8_t buf[BUFFER_SIZE];
    uint8_t head;   /* write index */
    uint8_t tail;   /* read index  */
    uint8_t count;
} RingBuf;

void rb_init(RingBuf *rb)
{
    rb->head = rb->tail = rb->count = 0;
    memset(rb->buf, 0, sizeof(rb->buf));
}

static uint8_t is_full(const RingBuf *rb)  { return rb->count == BUFFER_SIZE; }
static uint8_t is_empty(const RingBuf *rb) { return rb->count == 0; }

uint8_t rb_count(const RingBuf *rb) { return rb->count; }

int8_t rb_write(RingBuf *rb, uint8_t byte)
{
    if (is_full(rb))
        return RB_FULL;

    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) & BUFFER_MASK;
    rb->count++;
    return RB_OK;
}

int8_t rb_read(RingBuf *rb, uint8_t *b)
{
    if (is_empty(rb))
        return RB_EMPTY;

    *b = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & BUFFER_MASK;
    rb->count--;
    return RB_OK;
}

int main(void)
{
    RingBuf rb;
    uint8_t b;
    int8_t  ret;
    uint8_t i;

    rb_init(&rb);

    /* fill it up -- 0x41 ('A') through 0x48 ('H') */
    uint8_t fill[] = { 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48 };
    for (i = 0; i < 8; i++) {
        ret = rb_write(&rb, fill[i]);
        if (ret == RB_OK)
            printf("[WRITE] 0x%02X -> OK (count=%u)%s\n",
                   fill[i], rb_count(&rb),
                   is_full(&rb) ? " FULL" : "");
        else
            printf("[WRITE] 0x%02X -> FAIL (buffer full)\n", fill[i]);
    }

    /* this one should fail */
    ret = rb_write(&rb, 0x99);
    printf("[WRITE] 0x99 -> %s\n",
           ret == RB_OK ? "OK (shouldn't happen)" : "FAIL (buffer full)");

    /* drain 3 slots */
    for (i = 0; i < 3; i++) {
        ret = rb_read(&rb, &b);
        if (ret == RB_OK)
            printf("[READ]  -> 0x%02X (count=%u)\n", b, rb_count(&rb));
    }

    /* write into the freed slots -- should wrap around in the array */
    uint8_t more[] = { 0x49, 0x4A, 0x4B };
    for (i = 0; i < 3; i++) {
        ret = rb_write(&rb, more[i]);
        if (ret == RB_OK)
            printf("[WRITE] 0x%02X -> OK (count=%u)%s\n",
                   more[i], rb_count(&rb),
                   is_full(&rb) ? " FULL" : "");
        else
            printf("[WRITE] 0x%02X -> FAIL (buffer full)\n", more[i]);
    }

    /* drain everything */
    while (!is_empty(&rb)) {
        rb_read(&rb, &b);
        printf("[READ]  -> 0x%02X (count=%u)\n", b, rb_count(&rb));
    }

    /* read on empty -- should fail */
    ret = rb_read(&rb, &b);
    printf("[READ] (empty) -> %s\n",
           ret == RB_EMPTY ? "FAIL (buffer empty)" : "OK (shouldn't happen)");

    return 0;
}
