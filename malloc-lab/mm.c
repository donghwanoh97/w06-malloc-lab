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
    "3",
    /* First member's full name */
    "Donwhwan Oh",
    /* First member's email address */
    "odh9568@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) //8의 배수로 올림

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Custom macros
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define IS_EPILOUGE(bp) ((GET_SIZE(HDRP(bp)) == 0) && GET_ALLOC(HDRP(bp)))

// Global variables
static char * heap_listp;
static char * rover; // 마지막으로 탐색을 마친 가용 블록

// Function Prototypes
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(char *bp, size_t asize);
static void *find_first_fit(size_t size);
static void *find_next_fit(size_t asize);

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *bp, size_t size);


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); // alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prolouge header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prolouge footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // epilouge header
    heap_listp += (2*WSIZE); // 첫 bp, 프롤로그에도 일관된 NEXT_BLKP 동작

    rover = heap_listp;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the ㅡㅡalignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;
    
    if (size < DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);
    if ((bp = find_next_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
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
void *mm_realloc(void *bp, size_t size)
{
    if (bp == NULL) return mm_malloc(size);

    if (size == 0) 
    {
        mm_free(bp);
        return NULL;
    }

    size_t asize;    
    if (size <= DSIZE) asize = 2 * DSIZE;
    else asize = ALIGN(size + DSIZE);


    size_t old_size = GET_SIZE(HDRP(bp));

    // 1. 기존 공간 충분하면 그대로 사용
    if (old_size >= asize)
    {
        return bp;
    }

    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));

    // 2. 기존 공간 불충분, 다음 블록이 free하고 공간이 충분하다면 병합
    if (!next_alloc && old_size + next_size >= asize)
    {
        size_t merged_size = old_size + next_size;

        // PUT(HDRP(bp), PACK(merged_size, 1));
        // PUT(FTRP(bp), PACK(merged_size, 1));
        // split
        PUT(HDRP(bp), PACK(merged_size, 0));
        PUT(FTRP(bp), PACK(merged_size, 0));

        if ((char *)rover >= (char *)bp && (char *)rover < NEXT_BLKP(bp)) {
            rover = bp;
        }

        place(bp, asize);
        return bp;
    }

    // 3. 기존 공간, 다음 블록 공간 불충분시 malloc 호출
    void *newbp = mm_malloc(size);
    if (newbp == NULL)
    {
        return NULL;
    }

    size_t copySize = old_size - DSIZE;
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newbp, bp, copySize);
    mm_free(bp); // 기존 공간 해제
    
    return newbp;
}


/*
 * extend_heap- Extend the heap and set new free block 
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); // Free block header
    PUT(FTRP(bp), PACK(size, 0)); // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilouge header

    return coalesce(bp);
}


/*
 * coalesce- Merge adjacent blocks if available
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // 이전, 이후 블록 모두 할당
    if (prev_alloc && next_alloc) {
        return bp;
    }
    // 이전 블록은 할당, 이후 블록은 프리
    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // 이전 블록은 프리, 이후 블록은 할당
    if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // 이전, 이후 블록 모두 프리
    if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
            
    if ((char *)rover >= (char *)bp && (char *)rover < NEXT_BLKP(bp)) {
        rover = bp;
    }
    
    return bp;
}

/*
 * find_first_fit - Traverse the heap from the start and find the first fitting block
 */
static void *find_first_fit(size_t asize) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }

    return NULL;
}

static void *find_next_fit(size_t asize) {
    // 1. rover 다음부터 검색 시작
    void *old_rover = rover;
    void *bp;
    
    // 2. 에필로그 도착할 때까지 순회
    for (bp = NEXT_BLKP(rover); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            // 가용 공간을 찾게 되면 rover 리턴, rover 업데이트
            rover = bp;
            return bp;
        }
    }

    // 3. 에필로그까지 못 찾으면 다시 heap_listp부터 rover까지 검색
    for (bp = heap_listp; bp <= old_rover; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            rover = bp;
            return bp;
        }
    }
    return NULL;
}

/*
 * place - Allocate block and split if needed
 */
static void place(char *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) { // splittable
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),  PACK(csize - asize, 0));
        PUT(FTRP(bp),  PACK(csize - asize, 0));

    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
