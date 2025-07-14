#pragma once

#include "lang.hpp"
#include "scan.hpp"

#include <source_location>

NAMESPACE_BEGIN(lang)

template<typename T>
using Ptr = std::shared_ptr<T>;

struct Type;
struct Annotator;
struct ComptimeEvaluateResult;

struct ComptimeValue : std::enable_shared_from_this<ComptimeValue>
{
    Ptr<Type> type;

    virtual std::string to_string(Annotator*) = 0;

    virtual ~ComptimeValue() = default;
};

struct Type : ComptimeValue
{
    std::string name;

    std::string to_string(Annotator*) final { return std::format("Type({})", name); }
};

struct ComptimeEvaluateResult
{
    Ptr<ComptimeValue> value;
    Ptr<Type> type;
    std::optional<u32> local_index;

    constexpr operator bool() const noexcept
    {
        return value || type;
    }
};

using Id = u32;

struct Namespace : ComptimeValue
{
    ankerl::unordered_dense::map<Id, ComptimeEvaluateResult> contents;

    std::string to_string(Annotator*) final { return std::format("Namespace({})", (void*)this); }

    ComptimeEvaluateResult find(Id name)
    {
        auto iter = contents.find(name);
        if (iter == contents.end()) {
            log_warn("NOT FOUND");
            return {};
        }
        return iter->second;
    }
};

struct NamespaceEmptySlot : ComptimeValue
{
    Namespace _namespace;
    Id id;

    std::string to_string(Annotator* annotator) final;
};

struct UnknownIdentifier : ComptimeValue
{
    Id id;

    std::string to_string(Annotator* annotator) final;
};

struct SourceFragment
{
    Ast* ast;
    AstOffset offset;
};

struct Function;

struct Annotation
{
    ComptimeEvaluateResult result;
    Ptr<Function> function;
};

struct TypeAnnotations
{
    ankerl::unordered_dense::map<u32, Annotation> types;

    auto get(AstOffset offset) -> Annotation {
        auto iter = types.find(offset.value);
        if (iter == types.end()) {
            return {};
        }
        return iter->second;
    };
};

struct Annotator;

struct Function : ComptimeValue
{
    SourceFragment fragment;

    // TODO: Support reification per args
    TypeAnnotations annotations;
    ComptimeEvaluateResult cached_result;
    std::string symbol;

    std::string to_string(Annotator*) final { return std::format("Function({})", fragment.offset.value); }

    ComptimeEvaluateResult reify(Annotator& caller, std::span<const ComptimeEvaluateResult> args);
};

// TODO: Comptime integers will be 128 bit and convert to any other numeric type safely
struct Integer32 : ComptimeValue
{
    i32 value;

    std::string to_string(Annotator*) final { return std::format("Integer32({})", value); }
};

struct Boolean : ComptimeValue
{
    bool value;

    std::string to_string(Annotator*) final { return std::format("Boolean({})", value); }
};

struct CoreTypes
{
    Ptr<Type> type_type;
    Ptr<Type> function_type;
    Ptr<Type> i32_type;
    Ptr<Type> bool_type;
};

struct Locals
{
    std::vector<std::pair<Id, ComptimeEvaluateResult>> locals;

    ComptimeEvaluateResult find(u32 min_depth, Id name)
    {
        for (usz i = locals.size(); i-- > min_depth;) {
            if (locals[i].first == name) {
                auto res = locals[i].second;
                res.local_index = i;
                return res;
            }
        }
        return {};
    }
};

struct AnnotatedFragment
{
    SourceFragment fragment;
    TypeAnnotations annotations;
    ComptimeEvaluateResult result;
    Ptr<Function> function;
};

struct Compiler
{
    std::vector<SourceFragment> file_fragments;

    std::vector<AnnotatedFragment> annotated_fragments;

    CoreTypes core_types;
    Namespace globals;
    Constants constants;

    usz next_function_id = 0;

    ~Compiler();

    void init();
    void add(SourceFragment);
    void run();
};

struct Annotator
{
    Compiler* compiler = nullptr;
    TypeAnnotations* annotations = nullptr;
    Ast* ast = nullptr;

    Locals locals;

    u32 block_depth = 0;

    u32 print_depth = 0;
    template<typename... Args>
    void println(std::format_string<Args...> fmt, Args&& ...args)
    {
        for (u32 i = 0; i < print_depth; ++i) std::print(". ");
        std::println("{}", std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    template<std::derived_from<ComptimeValue> T>
    std::string to_string(const Ptr<T>& value)
    {
        return value ? value->to_string(this) : "nullptr";
    }

    std::pair<u32*, AstHeader> get_ip_header(AstOffset offset)
    {
        auto ip = ast->pointer_to(offset);
        return { ip, AstHeader{*ip} };
    }

    ComptimeEvaluateResult annotate_this(AstOffset offset, ComptimeEvaluateResult result, Ptr<Function> function = {})
    {
        // TOOD: Handle unrolled loops
        if (result.value) println(".Comptime = {}", to_string(result.value));
        else if (result.type) println(".AnnotateType = {}", to_string(result.type));
        annotations->types[offset.value] = {result, function};
        return result;
    }

    template<typename Fn>
    void for_each(AstOffset next, Fn&& fn)
    {
        while (next) {
            // println("--- {}", (next + 1).value);
            fn(next + 1);
            next = *ast->pointer_to(next);
        }
    }

    ComptimeEvaluateResult annotate_block(AstOffset next, u32 locals_depth)
    {
        block_depth++;
        ComptimeEvaluateResult res;
        for_each(next, [&](AstOffset i) { res = annotate(i, locals_depth); });
        block_depth--;
        return res;
    }

    void annotate_vardecl(AstOffset offset, u32 locals_depth, ComptimeEvaluateResult override = {})
    {
        log_warn("vardecl offset = {}", offset.value);

        auto[ip, header] = get_ip_header(offset);

        auto target = annotate(*(++ip)++, locals_depth);

        println(".Target = {}", to_string(target.value));

        Ptr<Type> type;
        if (ast_vardecl_header_has_type(header)) {
            auto res = annotate(*ip++, locals_depth);
            if (!res.value) {
                raise_error("type expression must have constant value");
            }
            type = std::dynamic_pointer_cast<Type>(res.value);
            if (!type) {
                raise_error("type expression must resolve to a type");
            }

            println(".Type = {}", to_string(type));
        }

        ComptimeEvaluateResult init;
        if (ast_vardecl_header_has_init(header)) {
            init = annotate(*ip++, locals_depth);
            if (!init.type) {
                raise_error("init does not return valid type");
            }
            if (type) {
                if (type != init.type) {
                    raise_error("type declaration does not match init expression type");
                }
            } else {
                type = init.type;
            }

            println(".Init = {}", to_string(init.value));
            println(".InitType = {}", to_string(init.type));
        }

        if (!type) {
            raise_error("failed to resolve type for variable declaration");
        }

        std::optional<u32> local_index;
        if (block_depth == 0 /* file scope */) {
            if (!target.value) {
                raise_error("target of variable declaration must be compile time known");
            }
            if (!init.value) {
                raise_error("namespace entry declaration must have a compile time known initializer");
            }

            if (auto namespace_slot = std::dynamic_pointer_cast<NamespaceEmptySlot>(target.value)) {
                println("declaring entry in specified namespace");
                namespace_slot->_namespace.contents[namespace_slot->id] = init;
            } else if (auto unknown_ident = std::dynamic_pointer_cast<UnknownIdentifier>(target.value)) {
                println("declaring entry in global namespace");
                compiler->globals.contents[unknown_ident->id] = init;
            } else {
                raise_error("non-empty target of variable declaration must be an empty namespace slot or unknown local identifier");
            }
        } else {
            auto unknown_local = std::dynamic_pointer_cast<UnknownIdentifier>(target.value);
            if (!unknown_local) {
                raise_error("local variable declaration target must be an UnknownLocal comptime value");
            }
            local_index = locals.locals.size();
            locals.locals.emplace_back(unknown_local->id, ComptimeEvaluateResult{nullptr, type});
        }

        annotations->types[offset.value] = {{init.value, type, local_index}};
    }

    ComptimeEvaluateResult annotate(AstOffset offset, u32 locals_depth)
    {
        auto[ip, header] = get_ip_header(offset);

        println("{} @ {}", get_ast_header_name(header), offset.value);
        print_depth++;
        defer { print_depth--; };

        switch (header) {
            case AstHeader::Function_Definition:
            {
                auto function = std::make_shared<Function>();
                function->fragment = {ast, offset};
                function->type = compiler->core_types.function_type;

                return annotate_this(offset, {function, function->type});
            }

            case AstHeader::Identifier:
            {
                auto ident = ip[1];
                if (auto local = locals.find(locals_depth, ident)) {
                    return annotate_this(offset, local);
                } else if (auto global = compiler->globals.find(ident)) {
                    return annotate_this(offset, global);
                } else {
                    auto unknown_ident = std::make_shared<UnknownIdentifier>();
                    unknown_ident->id = ident;
                    return annotate_this(offset, {unknown_ident});
                }

                break;
            }

            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
            {
                annotate_vardecl(offset, locals_depth);
                return {};
            }

            case AstHeader::If:
            case AstHeader::IfElse:
            {
                auto cond = annotate(ip[1], locals_depth);

                if (cond.type != compiler->core_types.bool_type) {
                    raise_error("if condition must be boolean");
                }

                if (cond.value) {
                    log_trace("compile known branch");
                    // log_warn("TODO: compile time branching, falling back to runtime");
                    // bool cond_value = cond.value->convert_to_bool();
                    auto cond_value_ptr = std::dynamic_pointer_cast<Boolean>(cond.value);
                    if (!cond_value_ptr) {
                        raise_error("ICE: Expected Boolean");
                    }
                    auto cond_value = cond_value_ptr->value;
                    if (cond_value) {
                        return annotate_this(offset, annotate(ip[2], locals_depth));
                    } else if (header == AstHeader::IfElse) {
                        return annotate_this(offset, annotate(ip[3], locals_depth));
                    } else {
                        return annotate_this(offset, {});
                    }
                }

                log_trace("runtime branch");
                auto then_eval = annotate_block(ip[2], locals_depth);
                auto result = ComptimeEvaluateResult{nullptr, then_eval.type};

                if (header == AstHeader::IfElse) {
                    auto else_eval = annotate_block(ip[3], locals_depth);
                    if (then_eval.type != else_eval.type) {
                        log_warn("if/else branch results do not match, will evaluate to nothing");
                        result = {};
                    }
                }

                return annotate_this(offset, result);
            }

            case AstHeader::Call:
            {
                auto callee = annotate(ip[1], locals_depth);

                if (!callee.value) {
                    log_error("TODO: support dynamic dispatch");
                    raise_error("function target must be compile time known");
                }

                auto function = std::dynamic_pointer_cast<Function>(callee.value);
                if (!function) {
                    raise_error("target of call must be a function");
                }

                std::vector<ComptimeEvaluateResult> args;
                for_each(ip[2], [&](AstOffset i) {
                    args.emplace_back(annotate(i, locals_depth));
                });

                return annotate_this(offset, function->reify(*this, args), function);
            }

            // TODO: Rest of operators
            case AstHeader::Add:
            case AstHeader::Subtract:
            case AstHeader::Less:
            {
                auto lhs = annotate(ip[1], locals_depth);
                auto rhs = annotate(ip[2], locals_depth);

                // TODO: Handle operator dispatch via table
                ComptimeEvaluateResult result;
                if (lhs.type != rhs.type) {
                    raise_error("operator types must match");
                } else {
                    result = lhs;
                }

                if (header == AstHeader::Less) {
                    return annotate_this(offset, {nullptr, compiler->core_types.bool_type});
                } else {
                    return annotate_this(offset, result);
                }
            }

            case AstHeader::Literal_Number:
            {
                auto num_str = compiler->constants.numbers.from_id(ip[1]);
                i32 value;
                auto res = std::from_chars(num_str.begin(), num_str.end(), value);
                if (!res) {
                    raise_error("failed to parse literal number");
                }

                auto integer = std::make_shared<Integer32>();
                integer->type = compiler->core_types.i32_type;
                integer->value = value;

                return annotate_this(offset, {integer, integer->type});
            }

            default:
                raise_error("unhandled AST node type: {}", get_ast_header_name(header));
        }
    }
};

inline
ComptimeEvaluateResult Function::reify(Annotator& caller, std::span<const ComptimeEvaluateResult> args)
{
    if (cached_result) return cached_result;

    Annotator annotator;
    annotator.compiler = caller.compiler;
    TypeAnnotations annotations;
    annotator.annotations = &annotations;
    annotator.ast = fragment.ast;

    symbol = std::format("LANG_func{}", caller.compiler->next_function_id++);

    log_warn("reifying function: {}", to_string(&annotator));

    // Block depth for functions starts at one. Zero is file scope
    annotator.block_depth = 1;

    auto[ip, header] = annotator.get_ip_header(fragment.offset);

    annotator.for_each(ip[1], [&](AstOffset offset) {
        auto[ip, header] = annotator.get_ip_header(offset);
        annotator.println("{}", get_ast_header_name(header));
        annotator.print_depth++;
        defer { annotator.print_depth--; };
        switch (header) {
            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
            {
                // TODO: Compile time arguments and argument type checking
                annotator.annotate_vardecl(offset, annotator.locals.locals.size());

                break;
            }

            default:
                raise_error("parameter list can only contain variable declarations");
        }
    });

    // TODO: Handle void functions

    auto return_type_res = annotator.annotate(ip[2], 0);
    if (!return_type_res.value) {
        raise_error("return type must be a constant expression");
    }
    auto return_type = std::dynamic_pointer_cast<Type>(return_type_res.value);
    if (!return_type) {
        raise_error("return type must evaluate to a type");
    }

    // TODO: How to handle self recursive calls
    cached_result = {nullptr, return_type};

    auto result = annotator.annotate_block(ip[3], 0);
    if (return_type != result.type) {
        raise_error("mismatched return type from block return");
    }

    cached_result = result;

    log_warn("finished reifying function: {}", to_string(&annotator));

    annotator.annotate_this(fragment.offset, cached_result);

    annotator.compiler->annotated_fragments.emplace_back(
        fragment,
        std::move(annotations),
        cached_result,
        std::static_pointer_cast<Function>(shared_from_this()));

    return cached_result;
}

inline
std::string NamespaceEmptySlot::to_string(Annotator* annotator)
{
    return std::format("NamespaceEmptySlot({}:{})", annotator->compiler->constants.identifiers.from_id(id), (void*)this);
}

inline
std::string UnknownIdentifier::to_string(Annotator* annotator)
{
    return std::format("UnknownIdentifier({})", annotator->compiler->constants.identifiers.from_id(id));
}

inline
Compiler::~Compiler()
{
    // NOTE: Breaks the reference cycle of type on itself
    core_types.type_type->type.reset();
}

inline
void Compiler::init()
{
    core_types.type_type = std::make_shared<Type>();
    core_types.type_type->name = "type";
    core_types.type_type->type = core_types.type_type;

    core_types.function_type = std::make_shared<Type>();
    core_types.function_type->name = "function";
    core_types.function_type->type = core_types.type_type;

    core_types.i32_type = std::make_shared<Type>();
    core_types.i32_type->name = "i32";
    core_types.i32_type->type = core_types.type_type;

    core_types.bool_type = std::make_shared<Type>();
    core_types.bool_type->name = "bool";
    core_types.bool_type->type = core_types.type_type;

    globals.contents[constants.identifiers.get_id("i32")] = {core_types.i32_type, core_types.type_type};
}

inline
void Compiler::add(SourceFragment file_fragment)
{
    Annotator annotator;
    annotator.ast = file_fragment.ast;

    annotator.for_each(file_fragment.ast->at(file_fragment.offset + 1), [&](AstOffset offset) {
        log_info("adding fragment: {}", offset.value);
        file_fragments.emplace_back(file_fragment.ast, offset);
    });
}

inline
void Compiler::run()
{
    for (auto& fragment : file_fragments) {
        log_info("annotating fragment with offset: {}", fragment.offset.value);
        Annotator annotator;
        annotator.compiler = this;
        TypeAnnotations annotations;
        annotator.annotations = &annotations;
        annotator.ast = fragment.ast;
        auto res = annotator.annotate(fragment.offset, 0);
        annotated_fragments.emplace_back(fragment, std::move(annotations), res);
    }
}

NAMESPACE_END
