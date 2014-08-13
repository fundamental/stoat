#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Pass.h>
#include <cassert>
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

std::string typeToString(llvm::Type *type)
{
    std::string data_str;
    llvm::raw_string_ostream ss(data_str);
    type->getScalarType()->getScalarType()->print(ss);
    string sss = ss.str();
    if(sss[1] == '"')
        return sss.substr(2,sss.length()-4);
    else
        return sss.substr(1,sss.length()-2);
}

struct ExtractCallGraph : public FunctionPass {
    static char ID;
    ExtractCallGraph() : FunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    std::string runOnCallInst(CallInst *call)
    {
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
                    if(bitcast)
                        vtable_call_name = typeToString(bitcast->getSrcTy()) + to_s(offset_val);
                }
            }
        }
        string s;
        if(fn2) {
            s = fn2->getName().str();
        } else if(!vtable_call_name.empty()) {
            s = vtable_call_name;
        } else {
            //fprintf(stderr, "right here...\n");
            //call->dump();
            //fprintf(stderr, "FN: ");
            //call->getCalledValue()->dump();
            //fprintf(stderr, "ARG0: ");
            //call->getOperand(0)->dump();
            //fprintf(stderr, "ARG1: ");
            //call->getOperand(1)->dump();
            return "";
        }
        if(s == "llvm.dbg.value" || s == "llvm.var.annotation"
                || s == "llvm.stackrestore" || s == "llvm.stacksave"
                || s == "llvm.va_start"
                || s == "llvm.va_end"
                || s == "llvm.lifetime.start"
                || s == "llvm.lifetime.end"
                || strstr(s.c_str(), "llvm.memcpy")
                || strstr(s.c_str(), "llvm.memmove")
                || strstr(s.c_str(), "llvm.memset")
                || strstr(s.c_str(), "llvm.pow")
                || strstr(s.c_str(), "llvm.sqrt")
                || strstr(s.c_str(), "llvm.") //TODO check for false captures
                || strstr(s.c_str(), "llvm.umul"))
            return "";
        return s;
    }

    bool runOnFunction(Function &Fn) override {
        std::vector<string> v;
        for(auto &bb:Fn) {
            for(auto &i:bb) {
                if(i.getOpcode() == Instruction::Call) {
                    auto s = runOnCallInst(dyn_cast<CallInst>(&i));
                    if(!s.empty())
                        v.push_back(s);
                } else if(i.getOpcode() == Instruction::Invoke) {
                    auto invoke = dyn_cast<InvokeInst>(&i);
                    if(invoke->getCalledFunction())
                        v.push_back(invoke->getCalledFunction()->getName().str());
                    //else
                    //    fprintf(stderr, "Oh no, not this again\n");
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

struct ExtractAnnotations : public ModulePass {
    static char ID; // Pass ID, replacement for typeid
    ExtractAnnotations() : ModulePass(ID) {}

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
                    handleNested(o->getOperand(0));
            }
        }
        return false;
    }
};

//Extract Class Hierarcy
struct ExtractClassHierarchy : public FunctionPass {
    static char ID; // Pass ID, replacement for typeid
    ExtractClassHierarchy() : FunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    string getMethodName(char *name)
    {
        char buffer[1024];
        memset(buffer, 0, 1024);
        if(!name)
            return "";
        char *pos = rindex(name, ':');
        char *btmp = buffer;
        if(!pos)
            return "";
        pos++;
        while(*pos && *pos != '(')
        {
            *btmp++ = *pos++;
        }
        return buffer;
    }

    bool isConstructorp(char *name)
    {
        //Isolate Class Name
        string cname = getMethodName(name);
        if(cname.empty())
            return false;
        if(strstr(name, (cname + "::" + cname + "(").c_str()))
            return true;
        return false;
    }

    string extractSuperClass(Instruction *this_ptr, BitCastInst *possible)
    {
        if(auto load = dyn_cast<LoadInst>(possible->getOperand(0))) {
            if(this_ptr == dyn_cast<Instruction>(load->getOperand(0))) {
                std::string data_str;
                llvm::raw_string_ostream ss(data_str);
                possible->getDestTy()->getScalarType()->getScalarType()->print(ss);
                if(ss.str().substr(0,6) == "%class")
                    return ss.str();
            }
        }
        return "";
    }

    void findSuperClasses(Function &Fn, string name)
    {
        //The first IR Element is an alloca for the this pointer
        //later there is a load of this %this.addr
        //if this load is cast into anything other than i8***, then that's a
        //super class

        std::vector<string> class_list;
        Instruction *this_ptr = NULL;
        for(auto &bb:Fn) {
            for(auto &I:bb) {
                if(!this_ptr && dyn_cast<AllocaInst>(&I))
                    this_ptr = &I;

                if(auto bitcast = dyn_cast<BitCastInst>(&I))
                {
                    auto super = extractSuperClass(this_ptr, bitcast);
                    if(!super.empty())
                    {
                        bool not_already_here = true;
                        for(auto x:class_list)
                            if(x == super)
                                not_already_here = false;
                        if(not_already_here)
                            class_list.push_back(super.substr(7, super.length()-8));
                    }
                }
            }
        }

        std::unique(class_list.begin(), class_list.end());
        if(!class_list.empty()) {
            fprintf(stderr, "%s:\n", name.c_str());
            for(auto x:class_list)
                fprintf(stderr, "    - %s\n", x.c_str());
        }


    }

    bool runOnFunction(Function &Fn) override {
        int status = 0;
        char *realname = abi::__cxa_demangle(Fn.getName().str().c_str(), 0, 0, &status);
        if(isConstructorp(realname)) {
            //fprintf(stderr, "I'm in a constructor '%s'\n", realname);
            findSuperClasses(Fn, getMethodName(realname));
        }
        return false;
    }
};

//Extract Vtable contents
struct ExtractVtables : public ModulePass {
    static char ID; // Pass ID, replacement for typeid
    ExtractVtables() : ModulePass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    void handleVtable(const char *name, ConstantArray *v)
    {
        //Field 0 - NULL?
        //Field 1 - Typeinfo
        //Field 2 - Offset 0 virtual method
        //Field N - Offset N-2 virtual method
        unsigned ops = v->getNumOperands();
        if(ops >= 2)
            fprintf(stderr, "%s:\n", name);
        for(unsigned i=2; i<ops; ++i)
        {
            auto op       = v->getOperand(i);
            char *fname = NULL;
            if(!dyn_cast<ConstantPointerNull>(op)) {
                Function *function = NULL;
                auto alias    = dyn_cast<GlobalAlias>(op->getOperand(0));
                auto someth   = op->getOperand(0);
                if(alias)
                    function = dyn_cast<Function>(alias->getOperand(0));
                else
                    function = dyn_cast<Function>(someth);
                if(function)
                    fname = strdup(function->getName().str().c_str());
            }
            if(fname && strlen(fname) == 0)
                fname = NULL;
            char *tmp = fname;
            while(tmp && *tmp && isprint(*tmp))
                tmp++;
            if(tmp)
                *tmp = 0;
            fprintf(stderr, "    %d: %s\n", i-2, fname);
        }
    }

    bool runOnModule(Module &m) override {
        auto &gl = m.getGlobalList();
        for(auto &g:gl) {
            if(g.getName().str().substr(0,4) == "_ZTV") {
                const char *vtable_name = g.getName().str().c_str();
                int status = 0;
                char *realname = abi::__cxa_demangle(vtable_name, 0, 0, &status);
                if(!realname || strstr(realname, "__cxxabi"))
                    continue;
                char *tmp = realname;
                while(*tmp && isprint(*tmp))
                    tmp++;
                *tmp = 0;

                if(g.getNumOperands())
                    handleVtable(realname+11, dyn_cast<ConstantArray>(g.getOperand(0)));
            }
        }
        return false;
    }
};
}

char ExtractCallGraph::ID = 0;
char ExtractAnnotations::ID = 0;
char ExtractClassHierarchy::ID = 0;
char ExtractVtables::ID = 0;
static RegisterPass<ExtractCallGraph> P1("extract-callgraph", "Print YAML Representation of Callgraph including vTable Calls");
static RegisterPass<ExtractAnnotations> P2("extract-annotations", "Print YAML Representation of Function Annotations");
static RegisterPass<ExtractClassHierarchy> P3("extract-class-hierarchy", "Print YAML Representation of Class Hierarchy Derived From Constructors");
static RegisterPass<ExtractVtables> P4("extract-vtables", "Print YAML Representation of VTable's Function Pointer Contents");
