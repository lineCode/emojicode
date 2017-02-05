//
//  Object.c
//  Emojicode
//
//  Created by Theo Weidmann on 01.03.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include "Emojicode.hpp"
#include "Thread.hpp"

size_t memoryUse = 0;
bool zeroingNeeded = false;
Byte *currentHeap;
Byte *otherHeap;

void gc();

size_t gcThreshold = heapSize / 2;

int pausingThreadsCount = 0;
bool pauseThreads = false;
pthread_mutex_t pausingThreadsCountMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t allocationMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pauseThreadsFalsedCondition = PTHREAD_COND_INITIALIZER;
pthread_cond_t threadsCountCondition = PTHREAD_COND_INITIALIZER;

Object* emojicodeMalloc(size_t size) {
    pthread_mutex_lock(&allocationMutex);
    pauseForGC(&allocationMutex);
    if (memoryUse + size > gcThreshold) {
        if (size > gcThreshold) {
            error("Allocation of %zu bytes is too big. Try to enlarge the heap. (Heap size: %zu)", size, heapSize);
        }

        gc();
    }
    Byte *block = currentHeap + memoryUse;
    memoryUse += size;
    pthread_mutex_unlock(&allocationMutex);
    return reinterpret_cast<Object *>(block);
}

Object* emojicodeRealloc(Object *ptr, size_t oldSize, size_t newSize) {
    pthread_mutex_lock(&allocationMutex);
    // Nothing has been allocated since the allocation of ptr
    if (ptr == reinterpret_cast<Object *>(currentHeap + memoryUse - oldSize)) {
        memoryUse += newSize - oldSize;
        pthread_mutex_unlock(&allocationMutex);
        return ptr;
    }
    pthread_mutex_unlock(&allocationMutex);

    Object *block = emojicodeMalloc(newSize);
    memcpy(block, ptr, oldSize);
    return block;
}

static Object* newObjectWithSizeInternal(Class *klass, size_t size) {
    size_t fullSize = sizeof(Object) + size;
    Object *object = emojicodeMalloc(fullSize);
    object->size = fullSize;
    object->klass = klass;
    object->value = ((Byte *)object) + sizeof(Object) + klass->instanceVariableCount * sizeof(Value);

    return object;
}

Object* newObject(Class *klass) {
    return newObjectWithSizeInternal(klass, klass->size);
}

size_t sizeCalculationWithOverflowProtection(size_t items, size_t itemSize) {
    size_t r = items * itemSize;
    if (r / items != itemSize) {
        error("Integer overflow while allocating memory. It’s not possible to allocate objects of this size due to hardware limitations.");
    }
    return r;
}

Object* newArray(size_t size) {
    size_t fullSize = sizeof(Object) + size;
    Object *object = emojicodeMalloc(fullSize);
    object->size = fullSize;
    object->klass = CL_ARRAY;
    object->value = ((Byte *)object) + sizeof(Object);

    return object;
}

Object* resizeArray(Object *array, size_t size) {
    size_t fullSize = sizeof(Object) + size;
    Object *object = emojicodeRealloc(array, array->size, fullSize);
    object->size = fullSize;
    object->value = ((Byte *)object) + sizeof(Object);
    return object;
}

void allocateHeap() {
    currentHeap = static_cast<Byte *>(calloc(heapSize, 1));
    if (!currentHeap) {
        error("Cannot allocate heap!");
    }
    otherHeap = currentHeap + (heapSize / 2);
}

void mark(Object **oPointer) {
    Object *o = *oPointer;
    if (currentHeap <= (Byte *)o->newLocation && (Byte *)o->newLocation < currentHeap + heapSize / 2) {
        *oPointer = o->newLocation;
        return;
    }

    o->newLocation = (Object *)(currentHeap + memoryUse);
    memoryUse += o->size;

    memcpy(o->newLocation, o, o->size);
    *oPointer = o->newLocation;

    o->newLocation->value = ((Byte *)o->newLocation) + sizeof(Object) + o->klass->instanceVariableCount * sizeof(Value);

    // This class can lead the GC to other objects.
    if (o->klass->mark) {
        o->klass->mark(o->newLocation);
    }

    //    for (int i = 0; i < o->klass->instanceVariableCount; i++) {
    //        Value *s = (Value *)(((Byte *)o->newLocation) + sizeof(Object) + i * sizeof(Value));
    //        if (isRealObject(*s)) {
    //            mark(&s->object);
    //        }
    //    }
}

void gc() {
    pauseThreads = true;
    pthread_mutex_unlock(&allocationMutex);

    pthread_mutex_lock(&pausingThreadsCountMutex);
    pausingThreadsCount++;

    while (pausingThreadsCount < Thread::threads())
        pthread_cond_wait(&threadsCountCondition, &pausingThreadsCountMutex);

    Byte *tempHeap = currentHeap;
    currentHeap = otherHeap;
    otherHeap = tempHeap;
    size_t oldMemoryUse = memoryUse;
    memoryUse = 0;

    for (Thread *thread = Thread::lastThread(); thread != nullptr; thread = thread->threadBefore()) {
        thread->markStack();
    }

    for (uint_fast16_t i = 0; i < stringPoolCount; i++) {
        mark(stringPool + i);
    }

    if (oldMemoryUse == memoryUse) {
        error("Terminating program due to too high memory pressure.");
    }

    if (zeroingNeeded) {
        memset(currentHeap + memoryUse, 0, (heapSize / 2) - memoryUse);
    }
    else {
        zeroingNeeded = true;
    }

    pausingThreadsCount--;
    pthread_mutex_unlock(&pausingThreadsCountMutex);
    pauseThreads = false;
    pthread_cond_broadcast(&pauseThreadsFalsedCondition);
}

void pauseForGC(pthread_mutex_t *mutex) {
    if (pauseThreads) {
        if (mutex) pthread_mutex_unlock(mutex);

        pthread_mutex_lock(&pausingThreadsCountMutex);
        pausingThreadsCount++;
        pthread_cond_signal(&threadsCountCondition);
        while (pauseThreads) pthread_cond_wait(&pauseThreadsFalsedCondition, &pausingThreadsCountMutex);
        pausingThreadsCount--;
        pthread_mutex_unlock(&pausingThreadsCountMutex);

        if (mutex) pthread_mutex_lock(mutex);
    }
}

void allowGC() {
    pthread_mutex_lock(&pausingThreadsCountMutex);
    pausingThreadsCount++;
    pthread_cond_signal(&threadsCountCondition);
    pthread_mutex_unlock(&pausingThreadsCountMutex);
}

void disallowGCAndPauseIfNeeded() {
    pthread_mutex_lock(&pausingThreadsCountMutex);
    while (pauseThreads) pthread_cond_wait(&pauseThreadsFalsedCondition, &pausingThreadsCountMutex);
    pausingThreadsCount--;
    pthread_cond_signal(&threadsCountCondition);
    pthread_mutex_unlock(&pausingThreadsCountMutex);
}

bool isPossibleObjectPointer(void *s) {
    return (Byte *)s < currentHeap + heapSize/2 && s >= (void *)currentHeap;
}