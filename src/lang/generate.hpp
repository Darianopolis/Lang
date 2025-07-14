#pragma once

#include "compile.hpp"

NAMESPACE_BEGIN(lang)

inline
std::string type_to_ctype(Annotator& annotator, Ptr<Type> type)
{
    if (type == annotator.compiler->core_types.i32_type) return "int";
    if (type == annotator.compiler->core_types.bool_type) return "bool";
    raise_error("no valid C type mapping for: {}", annotator.to_string(type));
}

struct CodeGenerator
{
    Ast* ast;
    TypeAnnotations* ans;
    AnnotatedFragment* annotated_fragment;
    Annotator annotator;

    u32 stack_depth = 1;

    void init(Compiler& compiler, AnnotatedFragment& _annotated_fragment)
    {
        annotated_fragment = &_annotated_fragment;
        ast = annotated_fragment->fragment.ast;
        annotator.compiler = &compiler;
        annotator.ast = ast;
        ans = &annotated_fragment->annotations;
    }

    std::string local_str(u32 index) {
        return std::format("l{}", index);
    }

    std::string temp_str(u32 index) {
        if (!index) {
            raise_error("tried to reference nonexistent temporary");
        }
        return std::format("t{}", index);
    }
    u32 declare_temporary(Ptr<Type> type) {
        auto depth = stack_depth++;
        std::println("{} {};", type_to_ctype(annotator, type), temp_str(depth));
        return depth;
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

    ankerl::unordered_dense::set<u32> requires_temporary;

    bool can_fold_block(AstOffset offset)
    {
        auto first = offset;
        bool foldable = true;
        while (offset) {
            foldable &= can_fold(offset + 1);
            offset = *ast->pointer_to(offset);
            if (offset) {
                foldable = false;
            }
        }
        if (!foldable) {
            requires_temporary.insert(first.value);
        }
        return foldable;
    }

    bool can_fold(AstOffset offset)
    {
        auto[ip, header] = annotator.get_ip_header(offset);

        switch (header) {
            case AstHeader::Identifier:
                return true;

            case AstHeader::Literal_Number:
                return true;

            case AstHeader::If:
            {
                requires_temporary.insert(offset.value);
                can_fold(ip[1]);
                can_fold_block(ip[2]);

                return false;
            }

            case AstHeader::IfElse:
            {
                bool foldable = true;
                foldable &= can_fold(ip[1]);
                foldable &= can_fold_block(ip[2]);
                foldable &= can_fold_block(ip[3]);

                if (!foldable) {
                    requires_temporary.insert(offset.value);
                }
                return foldable;
            }

            case AstHeader::Add:
            case AstHeader::Subtract:
            case AstHeader::Less:
            {
                auto foldable = can_fold(ip[1]) && can_fold(ip[2]);
                if (!foldable) {
                    requires_temporary.insert(offset.value);
                }
                return foldable;
            }

            case AstHeader::Call:
            {
                auto foldable = true;
                for_each(ip[2], [&](AstOffset offset) {
                    foldable &= can_fold(offset);
                });
                if (!foldable) {
                    requires_temporary.insert(offset.value);
                }
                return foldable;
            }

            default:
                raise_error("unhandled AST node type: {}", get_ast_header_name(header));
        }
    }

    void generate_block(AstOffset offset, u32 target) {
        while (offset) {
            auto next = *ast->pointer_to(offset);
            generate(offset + 1, next ? target : 0);
            offset = next;
        }
    }

    void generate(AstOffset offset, u32 target)
    {
        auto[ip, header] = annotator.get_ip_header(offset);

        switch (header) {
            case AstHeader::Identifier:
            {
                auto res = ans->get(offset).result;

                if (!requires_temporary.contains(offset.value)) {
                    std::print("{}", local_str(ans->get(offset).result.local_index.value()));
                    return;
                }
                else
                {
                    std::println("{} = {};", temp_str(target), local_str(ans->get(offset).result.local_index.value()));
                    return;
                }
            }

            case AstHeader::Literal_Number:
            {
                if (!requires_temporary.contains(offset.value)) {
                    std::print("{}", std::dynamic_pointer_cast<Integer32>(ans->get(offset).result.value)->value);
                    return;
                }
                else
                {
                    std::println("{} = {};", temp_str(target), std::dynamic_pointer_cast<Integer32>(ans->get(offset).result.value)->value);
                    return;
                }
            }

            case AstHeader::If:
            case AstHeader::IfElse:
            {
                if (!requires_temporary.contains(offset.value)) {
                    std::print("(");
                    generate(ip[1], 0);
                    std::print(" ? ");
                    generate_block(ip[2], 0);
                    std::print(" : ");
                    generate_block(ip[3], 0);
                    std::print(")");
                }
                else
                {
                    if (requires_temporary.contains(ip[1])) {
                        auto cond_index = declare_temporary(ans->get(ip[1]).result.type);
                        generate(ip[1], cond_index);
                        std::println("if ({}) {{", temp_str(cond_index));
                    } else {
                        std::print("if (");
                        generate(ip[1], 0);
                        std::println(") {{");
                    }

                    if (requires_temporary.contains(ip[2])) {
                        generate_block(ip[2], target);
                    } else {
                        std::print("{} = ", temp_str(target));
                        generate_block(ip[2], target);
                        std::println(";");
                    }

                    if (header == AstHeader::IfElse) {
                        std::println("}} else {{");

                        if (requires_temporary.contains(ip[2])) {
                            generate_block(ip[3], target);
                        } else {
                            std::print("{} = ", temp_str(target));
                            generate_block(ip[3], target);
                            std::println(";");
                        }
                    }

                    std::println("}}");
                }
                return;
            }

            case AstHeader::Add:
            case AstHeader::Subtract:
            case AstHeader::Less:
            {
                char op = {};
                switch (header) {
                    case AstHeader::Add:      op = '+'; break;
                    case AstHeader::Subtract: op = '-'; break;
                    case AstHeader::Less:     op = '<'; break;

                    default:
                        std::unreachable();
                }

                if (!requires_temporary.contains(offset.value)) {
                    std::print("(");
                    generate(ip[1], 0);
                    std::print(" {} ", op);
                    generate(ip[2], 0);
                    std::print(")");
                }
                else
                {
                    auto lhs_index = declare_temporary(ans->get(ip[1]).result.type);
                    generate(ip[1], lhs_index);
                    auto rhs_index = declare_temporary(ans->get(ip[2]).result.type);
                    generate(ip[2], rhs_index);

                    std::println("{} = {} {} {};", temp_str(target), temp_str(lhs_index), op, temp_str(rhs_index));
                }

                return;
            }

            case AstHeader::Call:
            {
                if (!requires_temporary.contains(offset.value)) {
                    std::print("{}(", ans->get(offset).function->symbol);
                    for_each(ip[2], [&](AstOffset offset) {
                        generate(offset, 0);
                    });
                    std::print(")");
                }
                else
                {
                    auto first_arg_index = stack_depth;
                    std::vector<u32> args;
                    for_each(ip[2], [&](AstOffset offset) {
                        auto arg_tmp = declare_temporary(ans->get(offset).result.type);
                        generate(offset, arg_tmp);
                        args.emplace_back(arg_tmp);
                    });

                    std::print("{} = {}(", temp_str(target), ans->get(offset).function->symbol);
                    for (u32 i = 0; i < args.size(); ++i) {
                        if (i) {
                            std::print(", ");
                        }
                        std::print("{}", temp_str(args[i]));
                    }
                    std::println(");");
                }

                return;
            }

            default:
                raise_error("generator :: unhandled AST node type: {}", get_ast_header_name(header));
        }
    }

    void generate_function()
    {
        auto offset = annotated_fragment->fragment.offset;
        auto[ip, header] = annotator.get_ip_header(offset);

        if (annotated_fragment->function) {
            std::print("{} ", type_to_ctype(annotator, ans->get(offset).result.type));
            std::print("{}(", annotated_fragment->function->symbol);
            annotator.for_each(ip[1], [&](AstOffset offset) {
                auto[ip, header] = annotator.get_ip_header(offset);
                auto annotation = ans->get(offset);
                std::print("{} {}", type_to_ctype(annotator, annotation.result.type), local_str(annotation.result.local_index.value()));
            });
            std::println(") {{");
            // std::println("return 0;\n}}");

            can_fold_block(ip[3]);

            if (!requires_temporary.contains(ip[3])) {
                std::print("return ");
                generate_block(ip[3], 0);
                std::println(";");
            }
            else
            {
                auto res = declare_temporary(ans->get(offset).result.type);
                generate_block(ip[3], res);
                std::println("return {};", temp_str(res));
            }

            std::println("}}");
            // ComptimeEvaluateResult res;
            // annotator.for_each(ip[3], [&](AstOffset offset) {
            //     auto[ip, header] = annotator.get_ip_header(offset);
            // });
        } else {
            log_debug("no code to generate for non function");
        }
    }
};

inline
void print_annotations(Compiler* compiler, AnnotatedFragment& annotated_fragment)
{
    auto& constants = compiler->constants;
    auto& ast = *annotated_fragment.fragment.ast;

    Annotator annotator;
    annotator.compiler = compiler;

    auto get_annotation_string = [&](AstOffset offset) -> std::string {
        auto iter = annotated_fragment.annotations.types.find(offset.value);
        if (iter == annotated_fragment.annotations.types.end()) {
            return "NoAnnotation";
        }
        auto& annotation = iter->second;
        if (annotation.result.value) {
            return std::format("Comptime({}){}", annotator.to_string(annotation.result.value), annotation.result.local_index ? std::format(" [{}]", *annotation.result.local_index) : "");
        }
        if (annotation.result.type) {
            return std::format("Runtime({}){}", annotator.to_string(annotation.result.type), annotation.result.local_index ? std::format(" [{}]", *annotation.result.local_index) : "");
        }
        return "ErrorInvalidAnnotation";
    };

    auto print_node_root = [&](this auto&& print_node, AstOffset offset, u64 depth) -> void {

        u32* ip = ast.pointer_to(offset);

        // #define PRINT(...)
        #define PRINT(fmt, ...) std::println("{}" fmt, indent_str(depth) __VA_OPT__(,) __VA_ARGS__)

        auto print_list = [&](AstOffset next, auto depth) {

            while (next) {
                print_node(next + 1, depth);
                next = *ast.pointer_to(next);
            }
            // compacted.write(AstOffset{});
        };

        auto header = AstHeader(*ip);

        PRINT("{}: {}", get_ast_header_name(header), get_annotation_string(offset));
        if (annotated_fragment.annotations.get(offset).result.value) {
            return;
        }

        // compacted.write(header);
        switch (header) {
            case AstHeader::File:
                print_list(ip[1], depth);
                break;
            case AstHeader::Block:
                print_list(ip[1], depth + 1);
                break;
            case AstHeader::Literal_Number:
                PRINT("  {}", constants.numbers.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
            case AstHeader::Literal_String:
                PRINT("  {}", constants.strings.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
            case AstHeader::Literal_True:
            case AstHeader::Literal_False:
            case AstHeader::Break_Empty:
            case AstHeader::Continue_Empty:
            case AstHeader::Return_Empty:
                break;
            case AstHeader::FieldAccess:
                PRINT("  {}", constants.identifiers.from_id(ip[1]));
                print_node(ip[2], depth + 1);
                // compacted.write(ip[1]);
                break;
            case AstHeader::Identifier:
                PRINT("  {}", constants.identifiers.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
// -----------------------------------------------------------------------------
            case AstHeader::Dereference:
            case AstHeader::Postincrement:
            case AstHeader::Postdecrement:
            case AstHeader::Break:
            case AstHeader::Continue:
            case AstHeader::Return:
            #define X(name, ...) case AstHeader::name:
                OPS_Prefix
            #undef X
                print_node(ip[1], depth + 1);
                break;
// -----------------------------------------------------------------------------
            #define X(name, ...) case AstHeader::name:
                OPS_Arithmetic
                OPS_Bitwise
                OPS_Logical
                OPS_Comparison
                OPS_Range
                OPS_Assign
            #undef X
                print_node(ip[1], depth + 1);
                print_node(ip[2], depth + 1);
                break;
// -----------------------------------------------------------------------------
            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
                print_node(ip[1], depth + 1);
                ip += 2;
                if (ast_vardecl_header_has_type(header)) {
                    PRINT(".TypeExpr");
                    print_node(*ip, depth + 1);
                    ip += 1;
                }
                if (ast_vardecl_header_has_init(header)) {
                    PRINT(".{}", ast_vardecl_header_is_constant(header) ? "ConstInit" : "MutableInit");
                    print_node(*ip, depth + 1);
                }
                break;
            case AstHeader::Function_Definition:
            case AstHeader::Function_Prototype:
                PRINT(".Params");
                print_list(ip[1], depth + 1);
                PRINT(".ReturnType");
                print_node(ip[2], depth + 1);
                if (header == AstHeader::Function_Definition) {
                    PRINT(".Body");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstHeader::Defer:
                print_list(ip[1], depth + 1);
                break;
            case AstHeader::Call:
            case AstHeader::Subscript:
                print_node(ip[1], depth + 1);
                PRINT(".Args");
                print_list(ip[2], depth + 1);
                break;
            case AstHeader::While:
            case AstHeader::WhileElse:
                print_node(ip[1], depth + 1);
                PRINT(".Body");
                print_list(ip[2], depth + 1);
                if (header == AstHeader::WhileElse) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstHeader::For:
            case AstHeader::ForElse:
                print_node(ip[1], depth + 1);
                PRINT(".Params");
                print_list(ip[2], depth + 1);
                PRINT(".Body");
                print_list(ip[3], depth + 1);
                if (header == AstHeader::ForElse) {
                    PRINT(".Else");
                    print_list(ip[4], depth + 1);
                }
                break;
            case AstHeader::If:
            case AstHeader::IfElse:
                print_node(ip[1], depth + 1);
                PRINT(".Then");
                print_list(ip[2], depth + 1);
                if (header == AstHeader::IfElse) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            default:
                raise_error("unhandled ast header: {}", get_ast_header_name(header));
        }

        #undef PRINT
    };

    print_node_root(annotated_fragment.fragment.offset, 0);
}

NAMESPACE_END
