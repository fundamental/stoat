#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Pass.h>
#include <cxxabi.h>
#include <sstream>

using namespace llvm;
using std::string;

namespace {

template<class T>
std::string stringFrom(T x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}

template<class T>
std::string to_s(T x)
{
    return stringFrom(x);
}

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
                    auto it   = call->getCalledValue();
                    auto it2  = dyn_cast<GlobalAlias>(it);
                    auto load = dyn_cast<LoadInst>(it);
                    string vtable_call_name = "";
                    if(!fn2 && it2)
                        fn2 = dyn_cast<Function>(it2->getOperand(0));
                    if(!fn2 && load) { //This might be a vtable call
                        auto getelm = dyn_cast<GetElementPtrInst>(load->getOperand(0));
                        if(getelm && getelm->getNumOperands() == 2) { //this is at least a function pointer at an offset
                            auto offset = dyn_cast<ConstantInt>(getelm->getOperand(1));
                            auto load2  = dyn_cast<LoadInst>(getelm->getOperand(0));
                            if(offset && load) {
                                size_t offset_val = offset->getZExtValue();
                                auto bitcast = dyn_cast<BitCastInst>(load2->getOperand(0));
                                if(bitcast) {
                                    //fprintf(stderr, "Surely a vtable call at off '%d' for : ", offset_val);
                                    //bitcast->getSrcTy()->dump();
                                    //fprintf(stderr, "\n");
                                    //fprintf(stderr, "Setting vtable_call_name\n");
                                    auto type = bitcast->getSrcTy();
                                    //fprintf(stderr,"<<<<<<<<<<");
                                    std::string data_str;
                                    llvm::raw_string_ostream ss(data_str);
                                    type->getScalarType()->getScalarType()->print(ss);
                                    //errs() << ss.str();
                                    //fprintf(stderr,">>>>>>>>>>");
                                    vtable_call_name = ss.str() + to_s(offset_val);
                                    //fprintf(stderr, "Vtable_call_name = '%s'\n", vtable_call_name.c_str());
                                }
                            }
                        }
                    }
                    string s;
                    if(fn2) {
                        s = fn2->getName().str();
                    } else if(!vtable_call_name.empty()) {
                        s = vtable_call_name;
                    } else {
                        fprintf(stderr, "right here...\n");
                        call->dump();
                        fprintf(stderr, "FN: ");
                        call->getCalledValue()->dump();
                        fprintf(stderr, "ARG0: ");
                        call->getOperand(0)->dump();
                        fprintf(stderr, "ARG1: ");
                        call->getOperand(1)->dump();
                        continue;
                    }
                    if(s == "llvm.dbg.value" || s == "llvm.var.annotation"
                            || s == "llvm.stackrestore" || s == "llvm.stacksave"
                            || s == "llvm.va_start"
                            || strstr(s.c_str(), "llvm.memcpy")
                            || strstr(s.c_str(), "llvm.memset"))
                        continue;
                    if(!s.empty())
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

struct DummyPass3 : public FunctionPass {
    static char ID; // Pass ID, replacement for typeid
    DummyPass3() : FunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    bool runOnFunction(Function &Fn) override {
        int status = 0;
        const char *realname = abi::__cxa_demangle(Fn.getName().str().c_str(), 0, 0, &status);
        fprintf(stderr, "Realname = '%s'\n", realname);

        Fn.dump();
        return false;
        //auto attr = Fn.getAttributes();
        //attr.dump();
        std::vector<string> v;
        //Fn.dump();
        for(auto &bb:Fn) {
            for(auto &i:bb) {
                if(i.getOpcode() == Instruction::Call) {
                    auto call = dyn_cast<CallInst>(&i);
                    auto fn2  = call->getCalledFunction();
                    if(!fn2) {
                        fprintf(stderr, "right here...\n");
                        call->dump();
                        fprintf(stderr, "ARG0: ");
                        call->getOperand(0)->dump();
                        fprintf(stderr, "ARG1: ");
                        call->getOperand(1)->dump();
                        continue;
                    }
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
        //if(!v.empty()) {
        //    fprintf(stderr, "%s :\n", Fn.getName().str().c_str());
        //    for(auto x:v)
        //        fprintf(stderr, "    - %s\n", x.c_str());
        //} else {
        //    fprintf(stderr, "%s :\n", Fn.getName().str().c_str());
        //    fprintf(stderr, "    - nil\n");
        //}
        return false;
    }
};
}

char DummyPass::ID = 0;
char DummyPass2::ID = 0;
char DummyPass3::ID = 0;
static RegisterPass<DummyPass> P1("dummy1", "do nothin");
static RegisterPass<DummyPass2> P2("dummy2", "do nothin");
static RegisterPass<DummyPass3> P3("dummy3", "do nothin");
