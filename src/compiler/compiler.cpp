#include "rift.h"
#include "compiler.h"
#include "types.h"
#include "pool.h"


/** Shorthand for calling runtime functions.  */
#define RUNTIME_CALL(NAME, ...) b->CreateCall(NAME(m.get()), vector<llvm::Value*>({ __VA_ARGS__ }), "")

namespace {

    llvm::Function * declareFunction(char const * name, llvm::FunctionType * signature, llvm::Module * m) {
        return llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, m);
    }


    llvm::Function * declarePureFunction(char const * name, llvm::FunctionType * signature, llvm::Module * m) {
        llvm::Function * f = declareFunction(name, signature, m);
        llvm::AttributeSet as;
        llvm::AttrBuilder b;
        b.addAttribute(llvm::Attribute::ReadNone);
        as = llvm::AttributeSet::get(rift::Compiler::context(),llvm::AttributeSet::FunctionIndex, b);
        f->setAttributes(as);
        return f;
    }

}

namespace rift {


Compiler::Compiler():
    result(nullptr),
    env(nullptr),
    m(new llvm::Module("module", context())),
    f(nullptr),
    b(nullptr) {
}

int Compiler::compile(ast::Fun * node) {
    // Backup context in case we are creating a nested function
    llvm::Function * oldF = f;
    llvm::IRBuilder<> * oldB = b;
    llvm::Value * oldEnv = env;

    // Create the function and its first BB
    f = llvm::Function::Create(type::NativeCode, llvm::Function::ExternalLinkage, "riftFunction", m.get());
    llvm::BasicBlock * entry = llvm::BasicBlock::Create(context(), "entry", f, nullptr);
    b = new llvm::IRBuilder<>(entry);

    // Get the (single) argument of the function and store is as the
    // environment
    llvm::Function::arg_iterator args = f->arg_begin();
    env = &*args;
    env->setName("env");

    llvm::Function * doubleVectorLiteral =
      declarePureFunction("doubleVectorLiteral", type::v_d, m.get());
    declarePureFunction("genericAdd", type::v_vv, m.get());
    
    node->accept(this);
    // TODO: return something else than returning 0
    //result = b->CreateCall(doubleVectorLiteral,
    //			   vector<llvm::Value*>(
    //			     {llvm::ConstantFP::get(context(), llvm::APFloat(0.0f))}), "");

    // Append return instruction of the last used value
    b->CreateRet(result);

    // Register and get index
    int result = Pool::addFunction(node, f);
    f->setName(STR(result));
    // Restore context
    f = oldF;
    delete b;
    b = oldB;
    env = oldEnv;
    return result;
}
  void Compiler::visit(ast::Fun* func) {
    //just visit the body atm ... args later
    func->body->accept(this);
  }
  void Compiler::visit(ast::Seq* seq) {
    //loop over all arguments
    for (auto elem : seq->body) {
      elem->accept(this);
    }
  }
  void Compiler::visit(ast::BinExp* bexp) {
    bexp->lhs->accept(this);
    llvm::Value* lhs = result;
    bexp->rhs->accept(this);
    llvm::Value* rhs = result;
    switch(bexp->op) {
    case (ast::BinExp::Op::add):
      result = b->CreateCall(m.get()->getFunction("genericAdd"),
			     vector<llvm::Value*>({lhs, rhs}),
			     "");
      return;
    default:
      result = lhs; //TODO
      return;
    }
  }
  void Compiler::visit(ast::Num* expr) {
    //for now, assume that there is only a single expr and that it is a number
    double val = expr->value;
    llvm::Function* dvl = m.get()->getFunction("doubleVectorLiteral");
    result = b->CreateCall(dvl,
			   vector<llvm::Value*>(
			     {llvm::ConstantFP::get(context(), llvm::APFloat(val))}), "");
  }

}
