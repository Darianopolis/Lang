#include "vm.hpp"

NAMESPACE_BEGIN(lang::vm)

struct Compiler::Impl : Compiler
{
    GlobalContext context;

    std::vector<AstNode*> fragments;

    void scan_dependencies(AstNode* root) {

        std::vector<std::string_view> locals;
        ankerl::unordered_dense::set<std::string_view> referenced;

        [&](this auto&& self, AstNode* _node, bool is_root = false) -> void {
            u32 local_index = locals.size();
            defer { locals.resize(local_index); };

            return visit(overload_set {
                [&](AstVarDecl& var_decl) {
                    if (is_root) {
                        std::println("produces:");
                        auto target = var_decl.target;
                        // bool first = true;
                        for (;; /*first = false*/) {
                            if (target->type == AstNodeType::NamedReference) {
                                auto named_ref = static_cast<AstNamedReference*>(target);
                                // if (!first) {
                                //     referenced.emplace(named_ref->token.raw);
                                // }
                                std::println("  named: {}", named_ref->token.raw);
                                break;
                            } else if (target->type == AstNodeType::FieldAccess) {
                                auto field_access = static_cast<AstFieldAccess*>(target);
                                std::println("  field: {}", field_access->field.raw);
                                target = field_access->lhs;
                            } else {
                                raise_error("Illegal AST node in var declaration target: {}", get_ast_node_type_name(target->type));
                            }
                        }
                    } else {
                        auto name = ast_expect_node<AstNamedReference>(var_decl.target);
                        locals.emplace_back(name->token.raw);
                    }
                },
                [&](AstNamedReference& named_ref) {
                    bool found = false;
                    for (u32 i = locals.size(); i--;) {
                        if (locals[i] == named_ref.token.raw) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        referenced.emplace(named_ref.token.raw);
                    }
                },
                [&](auto&&) {
                    raise_error("TODO");
                }
            }, _node);
        }(root, true);

        if (!referenced.empty()) {
            std::println("consumes:");
            for (auto named : referenced) {
                std::println("  {}", named);
            }
        }
    }
};

void Compiler::add_fragment(AstNode* node)
{
    get_impl(this)->fragments.emplace_back(node);
}

void Compiler::run()
{

}

NAMESPACE_END
