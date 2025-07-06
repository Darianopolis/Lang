#if 0
#pragma once

#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

#define OPCODE_LIST(DO) \
    DO(LoadNil, 0) \
    DO(LoadNumber, 8) \
    DO(LoadString, 16) \
    DO(LoadBoolean, 1) \
    DO(LoadFunction, 8) \
    DO(LoadGlobal, 16) \
    DO(LoadLocal, 4) \
    DO(LoadImmediate, 0) \
    \
    DO(CallFunction, 4) \
    DO(CallSubscript, 4) \
    \
    DO(FieldAccess, 16) \
    DO(NamespaceVarDecl, 1) \
    \
    DO(Operator, 4) \
    \
    DO(Pop, 4) \
    DO(PushDown, 4) \
    \
    DO(JumpIfFalse, 4) \
    DO(Jump, 4) \
    \
    DO(Return) \
    DO(Halt) \

#define OPCODE_DECLARE(op, ...) op,

enum class Opcode
{
    OPCODE_LIST(OPCODE_DECLARE)
};

constexpr
std::string_view get_opcode_name(Opcode opcode)
{
#define OPCODE_SWITCH_RETURN_NAME(op, ...) case Opcode::op: return #op;

    switch (opcode) {
        OPCODE_LIST(OPCODE_SWITCH_RETURN_NAME)
    }
}

// struct Instruction
// {
//     Opcode opcode;
//     union {
//         Token token = {};
//         Operator op;
//         u32 arg_count;
//         bool decl_has_init;
//         u32 count;
//         u32 stack_index;
//         i32 jump_delta;
//         f64 number;
//         bool boolean;
//         AstFunction* ast_function;
//     };
// };

using Instruction = uint64_t;

struct BytecodeChunk
{
    std::vector<Instruction> bytecode;
};

BytecodeChunk* bytecode_generate(AstNode* node);
BytecodeChunk* bytecode_generate_function_body(AstFunction* node);
void bytecode_debug_print(BytecodeChunk*);

struct Frame
{
    Instruction* ip;
    u32 stack_idx_offset;
};

struct BytecodeExecutionContext : ExecutionContext
{
    VmStack stack;

    std::vector<Frame> frames;

    ExecutionResult run() final;
};

BytecodeExecutionContext* bytecode_execution_context_create(FileContext* file, BytecodeChunk* chunk);

NAMESPACE_END
#endif
