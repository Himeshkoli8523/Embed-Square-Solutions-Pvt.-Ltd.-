#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE  8u
#define BUFFER_MASK  (BUFFER_SIZE - 1u)
#define RB_OK        0
#define RB_FULL     -1
#define RB_EMPTY    -2
typedef struct { // define structure of buffer
    uint8_t buf[BUFFER_SIZE];
    uint8_t head;   
    uint8_t tail;   
    uint8_t count;
} RingBuf;

void rb_init(RingBuf *rb) // when count is zero then its omited out from array
    {
    rb->head = rb->tail = rb->count = 0;
    memset(rb->buf, 0, sizeof(rb->buf));
}

static uint8_t is_full(const RingBuf *rb)  { return rb->count == BUFFER_SIZE; }
static uint8_t is_empty(const RingBuf *rb) { return rb->count == 0; }

uint8_t rb_count(const RingBuf *rb) // checking the count of buffer elements
    { 
    return rb->count;
}

int8_t rb_write(RingBuf *rb, uint8_t byte) // writing each byte in the buffer
    {
    if (is_full(rb))
        return RB_FULL;
    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) & BUFFER_MASK;
    rb->count++;
    return RB_OK;
}

int8_t rb_read(RingBuf *rb, uint8_t *b) // when buffer is empty it leaves *b inorder to not store garbage value
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
    ret = rb_write(&rb, 0x99);
    printf("[WRITE] 0x99 -> %s\n",
           ret == RB_OK ? "OK (shouldn't happen)" : "FAIL (buffer full)");
    for (i = 0; i < 3; i++) 
        {
        ret = rb_read(&rb, &b);
        if (ret == RB_OK)
            printf("[READ]  -> 0x%02X (count=%u)\n", b, rb_count(&rb));
    }
    uint8_t more[] = { 0x49, 0x4A, 0x4B };
    for (i = 0; i < 3; i++) 
        {
        ret = rb_write(&rb, more[i]);
        if (ret == RB_OK)
            printf("[WRITE] 0x%02X -> OK (count=%u)%s\n",
                   more[i], rb_count(&rb),
                   is_full(&rb) ? " FULL" : "");
        else
            printf("[WRITE] 0x%02X -> FAIL (buffer full)\n", more[i]);
    }
    while (!is_empty(&rb)) 
        {
        rb_read(&rb, &b);
        printf("[READ]  -> 0x%02X (count=%u)\n", b, rb_count(&rb));
    }
    ret = rb_read(&rb, &b);
    printf("[READ] (empty) -> %s\n",
           ret == RB_EMPTY ? "FAIL (buffer empty)" : "OK (shouldn't happen)");

    return 0;
}
