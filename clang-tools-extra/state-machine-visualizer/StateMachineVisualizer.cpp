#include <fstream>
#include <sstream>
#include <vector>

// Declares clang::SyntaxOnlyAction.
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/TemplateBase.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

class LoopPrinter : public MatchFinder::MatchCallback {
public:
  void run(const MatchFinder::MatchResult &Result) override {
    Transition transition;

    // llvm::outs() << '\n';
    const auto* state_machine = Result.Nodes.getNodeAs<CXXRecordDecl>("state_machine");
    state_machine_ = state_machine->getNameAsString();
    // llvm::outs() << "state_machine: " << state_machine_ << '\n';
    const auto* source_state = Result.Nodes.getNodeAs<FunctionDecl>("source_state");
    transition.source_state_ = source_state->getNameAsString();
    // llvm::outs() << "source_state: " << transition.source_state_ << '\n';
    if (const auto *message = Result.Nodes.getNodeAs<TemplateArgument>("message")) {
      PrintingPolicy pp{LangOptions{}};
      pp.SuppressTagKeyword = true;
      transition.message_ = message->getAsType().getAsString(pp);
      // llvm::outs() << "message: " << transition.message_ << '\n';

      if (const auto *target_state = Result.Nodes.getNodeAs<DeclRefExpr>("target_state")) {
        transition.target_state_ = target_state->getDecl()->getName();
      } else {
        transition.target_state_ = transition.source_state_;
      }
      // llvm::outs() << "target_state: " << transition.target_state_ << '\n';
    }

    transitions_.emplace_back(std::move(transition));
  }

  void encode(std::string& data) {
      std::string buffer;
      buffer.reserve(data.size());
      for(size_t pos = 0; pos != data.size(); ++pos) {
          switch(data[pos]) {
              // case '&':  buffer.append("&amp;");       break;
              // case '\"': buffer.append("&quot;");      break;
              // case '\'': buffer.append("&apos;");      break;
              case ':':  buffer.append("#colon;");     break;
              case '<':  buffer.append("#lt;");        break;
              case '>':  buffer.append("#gt;");        break;
              default:   buffer.append(&data[pos], 1); break;
          }
      }
      data.swap(buffer);
  }

  std::string toMermaid() {
    std::ostringstream ss;
    ss << "```mermaid\n";
    ss << "---\n";
    ss << "title: " << state_machine_ << '\n';
    ss << "---\n";
    ss << "stateDiagram-v2\n";
    for (auto& transition : transitions_) {
      ss << transition.source_state_;
      encode(transition.message_);
      if (!transition.message_.empty()) {
        ss << " --> " << transition.target_state_ << ": " << transition.message_;
      }
      ss << '\n';
    }
    ss << "```\n";
    return ss.str();
  }

  std::string state_machine_;
  struct Transition {
    std::string source_state_;
    std::string message_;
    std::string target_state_;
  };
  std::vector<Transition> transitions_;
};

auto callToState = functionDecl(
  hasParent(
    cxxRecordDecl(
      isDerivedFrom(
        cxxRecordDecl(
          hasName("::StateMachine"),
          isTemplateInstantiation()
        )
      )
    ).bind("state_machine")
  ),
  hasDescendant(
    cxxMemberCallExpr(
      thisPointerType(
        cxxRecordDecl(
          hasName("::StateMachine"),
          isTemplateInstantiation()
        )
      ),
      callee(
        cxxMethodDecl(
          hasName("state")
        )
      )
    )
  ),
  optionally(
    forEachDescendant(
      cxxMemberCallExpr(
        callee(
          cxxMethodDecl(
            hasName("on"),
            isTemplateInstantiation(),
            hasTemplateArgument(0, templateArgument().bind("message"))
          )
        ),
        hasArgument(0, expr(
          optionally(
            hasDescendant(
              unaryOperator(
                has(
                  declRefExpr().bind("target_state")
                )
              )
            )
          )
        ))
      )
    )
  )
).bind("source_state");

// cd ~/git/llvm-project/build
// ninja
// bin/state-machine-visualizer ../clang-tools-extra/state-machine/StateMachine.cpp
int main(int argc, const char **argv) {
  auto Parser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!Parser) {
    llvm::errs() << Parser.takeError();
    return 1;
  }

  LoopPrinter Printer;
  MatchFinder Finder;
  Finder.addMatcher(callToState, &Printer);

  const auto Ret = ClangTool(
    Parser->getCompilations(),
    Parser->getSourcePathList()
  ).run(newFrontendActionFactory(&Finder).get());
  if (!Ret) {
    const std::string Mermaid = Printer.toMermaid();
    llvm::outs() << Mermaid;
    std::ofstream("StateMachine.md", std::ofstream::trunc) << Mermaid;
  }

  return Ret;
}
