//
//  Thread.c
//  Emojicode
//
//  Created by Theo Weidmann on 05/04/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#include "Thread.hpp"
#include "Memory.hpp"
#include "Processor.hpp"
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace Emojicode;

Thread::Thread() {
#define stackSize (sizeof(StackFrame) * 1000000)
    stackLimit_ = static_cast<StackFrame *>(calloc(stackSize, 1));
    if (!stackLimit_) {
        error("Could not allocate stack!");
    }
    stackBottom_ = reinterpret_cast<StackFrame *>(reinterpret_cast<Byte *>(stackLimit_) + stackSize);
    this->futureStack_ = this->stack_ = this->stackBottom_;
}

Thread::~Thread() {
    free(stackLimit_);
}

StackFrame* Thread::reserveFrame(Value self, int size, Function *function, Value *destination,
                            EmojicodeInstruction *executionPointer) {
    size_t fullSize = sizeof(StackFrame) + sizeof(Value) * size;
    auto *sf = (StackFrame *)((Byte *)futureStack_ - (fullSize + (fullSize % alignof(StackFrame))));
    if (sf < stackLimit_) {
        error("Your program triggerd a stack overflow!");
    }

    sf->thisContext = self;
    sf->returnPointer = stack_;
    sf->returnFutureStack = futureStack_;
    sf->executionPointer = executionPointer;
    sf->destination = destination;
    sf->function = function;

    futureStack_ = sf;

    return sf;
}

void Thread::pushReservedFrame() {
    futureStack_->argPushIndex = UINT32_MAX;
    stack_ = futureStack_;
}

void Thread::pushStack(Value self, int frameSize, int argCount, Function *function, Value *destination,
                       EmojicodeInstruction *executionPointer) {
    StackFrame *sf = reserveFrame(self, frameSize, function, destination, executionPointer);

    sf->argPushIndex = 0;

    for (int i = 0; i < argCount; i++) {
        EmojicodeInstruction copySize = consumeInstruction();
        produce(this, sf->variableDestination(0) + sf->argPushIndex);
        sf->argPushIndex += copySize;
    }

    pushReservedFrame();
}

void Thread::popStack() {
    futureStack_ = stack_->returnFutureStack;
    stack_ = stack_->returnPointer;
}

void Thread::markStack() {
    for (auto frame = futureStack_; frame < stackBottom_; frame = frame->returnFutureStack) {
        unsigned int delta = frame->executionPointer ? frame->executionPointer - frame->function->block.instructions : 0;
        switch (frame->function->context) {
            case ContextType::Object:
                mark(&frame->thisContext.object);
                break;
            case ContextType::ValueReference:
                markValueReference(&frame->thisContext.value);
                break;
            default:
                break;
        }
        for (unsigned int i = 0; i < frame->function->objectVariableRecordsCount; i++) {
            auto record = frame->function->objectVariableRecords[i];
            if (record.from <= delta && delta <= record.to && record.variableIndex < frame->argPushIndex) {
                markByObjectVariableRecord(record, frame->variableDestination(0), i);
            }
        }
    }
}
