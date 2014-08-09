#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Pass.h>

using namespace llvm;
using std::string;

namespace {
struct DummyPass : public FunctionPass {
    static char ID; // Pass ID, replacement for typeid
    DummyPass() : FunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    bool runOnFunction(Function &Fn) override {
        //Fn.dump();
        //auto attr = Fn.getAttributes();
        //attr.dump();
        std::vector<string> v;
        //Fn.dump();
        for(auto &bb:Fn) {
            for(auto &i:bb) {
                if(i.getOpcode() == Instruction::Call) {
                    auto call = dyn_cast<CallInst>(&i);
                    auto fn2  = call->getCalledFunction();
                    if(!fn2)
                        continue;
                    auto s    = fn2->getName().str();
                    if(s == "llvm.dbg.value" || s == "llvm.var.annotation"
                            || s == "llvm.stackrestore" || s == "llvm.stacksave"
                            || s == "llvm.va_start"
                            || strstr(s.c_str(), "llvm.memcpy")
                            || strstr(s.c_str(), "llvm.memset"))
                        continue;
                    if(fn2)
                        v.push_back(s);
                }
            }
        }
        if(!v.empty()) {
            fprintf(stderr, "%s :\n", Fn.getName().str().c_str());
            for(auto x:v)
                fprintf(stderr, "    - %s\n", x.c_str());
        } else {
            fprintf(stderr, "%s :\n", Fn.getName().str().c_str());
            fprintf(stderr, "    - nil\n");
        }
        return false;
    }
};

struct DummyPass2 : public ModulePass {
    static char ID; // Pass ID, replacement for typeid
    DummyPass2() : ModulePass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    void handleNested(Value *v)
    {
        if(auto o = dyn_cast<Function>(v))
        {
            fprintf(stderr, "%s :\n", o->getName().str().c_str());
        } else if(auto s = dyn_cast<GlobalVariable>(v)) {
            fprintf(stderr, "    - %s\n", dyn_cast<ConstantDataArray>(s->getOperand(0))->getAsString().str().c_str());
        } else
            fprintf(stderr, "We have a dinosaur\n");


    }

    bool runOnModule(Module &m) override {
        auto g = m.getNamedGlobal("llvm.global.annotations");
        if(g) {
            auto gg = dyn_cast<ConstantArray>(g->getOperand(0));
            auto ggg = gg->getOperand(0);
            unsigned ops = ggg->getNumOperands();
            if(ops > 2)
                ops = 2;
            for(unsigned i=0; i<ops; ++i) {
                auto op = ggg->getOperand(i);
                if(auto o = dyn_cast<ConstantExpr>(op))
                {
                    //fprintf(stderr, "Case 1[");
                    //o->getOperand(0)->dump();;
                    //fprintf(stderr, "]\n");
                    handleNested(o->getOperand(0));

                }
                //fprintf(stderr,"\n");
            }
            //fprintf(stderr, "I found it\n");
        }
        return false;
    }
};
}

char DummyPass::ID = 0;
char DummyPass2::ID = 0;
static RegisterPass<DummyPass> P1("dummy1", "do nothin");
static RegisterPass<DummyPass2> P2("dummy2", "do nothin");
