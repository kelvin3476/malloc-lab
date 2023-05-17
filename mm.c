/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 5",
    /* First member's full name */
    "SeungWoo Lee",
    /* First member's email address */
    "d.rose3476@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 //word and header footer size (byte)
#define DSIZE 8 //duoble word size (byte)
#define CHUNKSIZE (1<<12) // initial free block size and default size for expanding the heap (byte)

#define MAX(x,y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) //header, footer에 들어가는 value값 return 

/* address p위치에 words를 read와 write를 한다. */
#define GET(p)  (*(unsigned int *)(p)) // return p값, (unsigned int *) 형변환으로 한 칸당 단위.
#define PUT(p, val)  (*(unsigned int*)(p) = (val)) // (unsigned int *) 형변환으로 한 칸당 단위.

#define GET_SIZE(p) (GET(p) & ~0x7) // ~0x7 = 11111000
#define GET_ALLOC(p) (GET(p) & 0x1) // 0x7 = 00000111

#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE) // bp : block pointer로 payload 시작 주소, -WSIZE는 바로 직전의 Header 주소를 구하기 위함

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //Header 정보를 확인하고 다음 bp를 찾는다. 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //Footer 정보를 확인하고 이전 bp를 찾는다. 

#define PRED_P(bp)  (*(void **)(bp))
#define SUCC_P(bp)  (*(void **)((bp)+WSIZE))

static void *heap_listp;

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *p, size_t size);
static void list_add(void *p);
static void list_remove(void *p);

/* 
 * mm_init - initialize the malloc package.s
 */
int mm_init(void)
{
    /* Create the initial empty heap
     * heap_listp : old_brk (void *) -> start point of block
     * mem_sbrk : add incr to the kernel' brk and return old brk (void *). It is start point
     * CHUNKSIZE : 1<<12 => 4096byte, CHUNKSIZE/WSIZE => 4096 byte/4 byte = 1024 words (2^10 words)
     */

    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); //prologue header
    PUT(heap_listp + (2*WSIZE), heap_listp+(3*WSIZE)); //prologue footer
    PUT(heap_listp + (3*WSIZE), heap_listp+(2*WSIZE)); //epilogue header
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1));
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));
    heap_listp += 2*WSIZE; // heap_listp point the position behind prologue header
    
    /*Extend the empty heap with a free blcok if CHUNKING bytes*/
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1; 
    return 0;
}

static void *find_fit(size_t asize)
{
    /* next-fit search : always search free block from first of heap area*/
    void *bp;
    for (bp = SUCC_P(heap_listp); !GET_ALLOC(HDRP(bp)); bp = SUCC_P(bp)) {
        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;


    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * Util functions
 */ 

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //previous block status
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //next block status
    size_t size = GET_SIZE(HDRP(bp)); //current block size

    if (prev_alloc && !next_alloc) { /* Case 1 : prev block allocated only */
        list_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) { /* Case 2 : next block allocated only */
        list_remove(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if (!prev_alloc && !next_alloc) { /* Case 3 : both of two free */
        list_remove(PREV_BLKP(bp));
        list_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    list_add(bp);
    return bp;
}

/*
 * extend_heap - extens the heap with a new free block
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1)*WSIZE : words*WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); //Free block header
    PUT(FTRP(bp), PACK(size, 0)); //Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); //New epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void place(void *p, size_t size)
{
    size_t free_block = GET_SIZE(HDRP(p));
    list_remove(p);
    // minimum size of block : 2 * DSIZE (Header(0.5D) + Payload(0.5D) + Padding(0.5D) + Footer(0.5D))
    if ((free_block - size) >= (2*DSIZE)) {
        PUT(HDRP(p), PACK(size, 1));
        PUT(FTRP(p), PACK(size, 1));
        // split block
        p = NEXT_BLKP(p);
        PUT(HDRP(p), PACK(free_block-size, 0));
        PUT(FTRP(p), PACK(free_block-size, 0));
        list_add(p);
    } else {
        PUT(HDRP(p), PACK(free_block, 1));
        PUT(FTRP(p), PACK(free_block, 1));
    }
}

static void list_add(void *p) {
    SUCC_P(p) = SUCC_P(heap_listp);
    PRED_P(p) = heap_listp;
    PRED_P(SUCC_P(heap_listp)) = p;
    SUCC_P(heap_listp) = p;
}

static void list_remove(void *p) {
    SUCC_P(PRED_P(p)) = SUCC_P(p);
    PRED_P(SUCC_P(p)) = PRED_P(p);
}

