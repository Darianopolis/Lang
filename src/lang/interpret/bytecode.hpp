#pragma once

#include "interpret.hpp"

NAMESPACE_BEGIN(lang)

#define OPCODE_LIST(DO) \
    DO(LoadNil) \
    DO(LoadNumber) \
    DO(LoadString) \
    DO(LoadGlobal) \
    DO(LoadLocal) \
    DO(LoadImmediate) \
    \
    DO(CallFunction) \
    DO(CallSubscript) \
    \
    DO(FieldAccess) \
    DO(NamespaceVarDecl) \
    \
    DO(Operator) \
    \
    DO(Pop) \
    DO(PushDown) \
    \
    DO(JumpIfFalse) \
    DO(Jump)

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

struct Instruction
{
    Opcode opcode;
    union {
        Token token = {};
        Operator op;
        u32 arg_count;
        bool decl_has_init;
        u32 count;
        u32 stack_index;
        u32 target_instruction;
    };
};

struct BytecodeChunk
{
    std::vector<Instruction> bytecode;
};

BytecodeChunk* bytecode_generate(AstNode* node);
void bytecode_debug_print(BytecodeChunk*);

struct BytecodeExecutionContext : ExecutionContext
{
    BytecodeChunk* chunk;
    std::vector<Value*> stack;
    u32 ip;

    ExecutionResult run() final;
};

BytecodeExecutionContext* bytecode_execution_context_create(FileContext* file, BytecodeChunk* chunk);

NAMESPACE_END
