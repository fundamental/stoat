//
// stoat - LLVM Based Static Analysis Tool
// Copyright (C) 2015 Mark McCurry
//
// This file is part of stoat.
//
// stoat is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// stoat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with stoat.  If not, see <http://www.gnu.org/licenses/>.
//

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

/*****************************************************************************
 *                         Utility Methods                                   *
 *****************************************************************************/


template<class T>
std::string to_s(T x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
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

string removeClassStruct(string s)
{
    const char *cs = s.c_str();
    if(cs==strstr(cs, "struct."))
        return cs+7;
    if(cs==strstr(cs, "class."))
        return cs+6;
    return cs;
};

//Print an escaped YAML string
void escapeOutput(string s)
{
    llvm::errs() << "\"";
    for(int i=0; i<(int)s.length(); ++i) {
        char c = s[i];
        if(c == 0)
            llvm::errs() << "\\0";
        else if(c == '"')
            llvm::errs() << "\\\"";
        else if(c == '\\')
            llvm::errs() << "\\\\";
        else
            llvm::errs() << c;


    }
    llvm::errs() << "\"";
}

//Determine if a load contains a reference to the start of a catch block
bool isCatchCall(llvm::LoadInst *inst)
{
    auto getelm   = dyn_cast<GetElementPtrInst>(inst->getOperand(0));
    if(!getelm) return false;

    auto loadinst = dyn_cast<LoadInst>(getelm->getOperand(0));
    if(!loadinst) return false;

    auto cast = dyn_cast<BitCastInst>(loadinst->getOperand(0));
    if(!cast) return false;

    auto call = dyn_cast<CallInst>(cast->getOperand(0));
    if(!call) return false;

    auto fn = call->getCalledFunction();
    if(!fn) return false;

    auto name = fn->getName().str();
    if(name == "__cxa_begin_catch") return true;

    return false;
}

//Identify a value by looking at the possible casts that it may undergo
string getTypeTheHardWay(llvm::Value *val, Function &Fn)
{
    //Ok, so normal means failed, but they traced it back to a value
    //Lets see if this value is cast into a '%class' in any other instruction
    for(auto &bb:Fn) {
        for(auto &inst:bb) {
            if(!dyn_cast<BitCastInst>(&inst) || inst.getOperand(0) != val)
                continue;
            auto cast = dyn_cast<CastInst>(&inst);
            string type = typeToString(cast->getDestTy());
            if(!type.empty() && type[0] == 'c')
                return type;
        }
    }
    return "unknown-type-name"; //failure...
}

struct ExtractCallGraph : public FunctionPass {
    static char ID;
    ExtractCallGraph() : FunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    //Occasionally there seems to be a class.realname.integer
    //The .integer doesn't seem to be needed, so this routine removes it
    std::string removeUniqueTail(std::string name)
    {
        std::string replacement;
        int dots = 0;
        for(int i=0; i<name.length(); ++i) {
            char cur = name[i];
            if(cur == '.')
                dots++;
            if(cur == '.' && dots == 2)
                replacement.append(".subtype");
            else
                replacement.push_back(cur);
        }
        return removeClassStruct(replacement);
    }

    std::string runOnCallInst(CallInst *call, Function &Fn)
    {
        auto fn2  = call->getCalledFunction();
        auto it   = call->getCalledValue();
        auto it2  = dyn_cast<GlobalAlias>(it);
        auto load = dyn_cast<LoadInst>(it);
        string vtable_call_name = "";
        if(!fn2 && it2)
            fn2 = dyn_cast<Function>(it2->getOperand(0));
        if(!fn2 && load && !isCatchCall(load)) {//Vtable call
            auto getelm = dyn_cast<GetElementPtrInst>(load->getOperand(0));
            if(getelm && getelm->getNumOperands() == 2) { //this is at least a function pointer at an offset
                auto offset = dyn_cast<ConstantInt>(getelm->getOperand(1));
                auto load2  = dyn_cast<LoadInst>(getelm->getOperand(0));
                if(offset && load) {
                    size_t offset_val = offset->getZExtValue();
                    auto bitcast = dyn_cast<BitCastInst>(load2->getOperand(0));
                    if(bitcast) {
                        string type = typeToString(bitcast->getSrcTy());
                        if(type.empty() || isdigit(type[0])) {
                            type = getTypeTheHardWay(bitcast->getOperand(0),Fn);
                        }
                        vtable_call_name = removeUniqueTail(type) + "$vtable" + to_s(offset_val);
                    }
                }
            }
        }
        string s;
        if(fn2) {
            s = fn2->getName().str();
        } else if(!vtable_call_name.empty()) {
            s = vtable_call_name;
            //Failure to obtain typename
            //(8 is a reference to an 8bit pointer)
            //(1 is an offset)
            if(s == "81") {
                exit(1);
            }
        } else {
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
                    auto s = runOnCallInst(dyn_cast<CallInst>(&i), Fn);
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
            fprintf(stderr, "%.1024s :\n", Fn.getName().str().c_str());
            for(auto x:v)
                fprintf(stderr, "    - %.1024s\n", x.c_str());
        } else {
            fprintf(stderr, "%.1024s :\n", Fn.getName().str().c_str());
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
            fprintf(stderr, "%.1024s :\n", o->getName().str().c_str());
        } else if(auto s = dyn_cast<GlobalVariable>(v)) {
            fprintf(stderr, "    - %.1024s\n", dyn_cast<ConstantDataArray>(s->getOperand(0))->getAsString().str().c_str());
        } else
            fprintf(stderr, "We have a dinosaur\n");
    }

    void doInner(Constant *ggg)
    {
        unsigned ops = ggg->getNumOperands();
        if(ops > 2)
            ops = 2;
        for(unsigned i=0; i<ops; ++i) {
            auto op = ggg->getOperand(i);
            if(auto o = dyn_cast<ConstantExpr>(op))
                handleNested(o->getOperand(0));
        }
    }

    bool runOnModule(Module &m) override {
        auto g = m.getNamedGlobal("llvm.global.annotations");
        if(g) {
            auto gg = dyn_cast<ConstantArray>(g->getOperand(0));
            //printf("orig ops = %d\n", gg->getNumOperands());
            unsigned ops = gg->getNumOperands();

            for(unsigned i=0; i<ops; ++i)
                doInner(gg->getOperand(i));
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

    string getFullName(string name)
    {
        if(name.empty())
            return "";
        char buffer1[1024] = {0};
        strncpy(buffer1, name.c_str(), 1023);

        int end = strlen(buffer1);
        int paren = 0;
        bool can_terminate = false;
        while(end > 0) {
            if(buffer1[end] == ')') {
                can_terminate = true;
                paren++;
            }
            if(buffer1[end] == '(') {
                paren--;
                if(paren == 0 && can_terminate)
                    break;
            }
            end--;
        }
        buffer1[end] = 0;
        return buffer1;
    }

    string getMethodName(const char *name)
    {
        if(!name)
            return "";
        char *name_ = strdup(name);

        char buffer[1024];
        char *pos = rindex(name_, ':');
        //printf("pos = '%s'\n", pos);
        char *btmp = name_;
        if(!pos)
            return "";
        pos++;
        while(*pos && *pos != '(')
        {
            *btmp++ = *pos++;
        }
        //if(*pos == '\0')
        *btmp = '\0';
        return name_;
    }

    bool isConstructorp(const char *name)
    {
        //Isolate Class Name
        string cname = getMethodName(name);
        //printf("cname = '%s'\n", cname.c_str());

        if(cname.empty())
            return false;
        if(strstr(name, (cname + "::" + cname).c_str())) //nice and simple case
            return true;
        return false;
    }

    //XXX make this more accurate
    bool isTemplateConstructorp(const char *name)
    {
        //printf("name = '%s'\n", name);
        string cname = getMethodName(name);
        //printf("cname = '%s'\n", cname.c_str());

        if(cname.empty())
            return false;
        if(strstr(name, (cname + "<").c_str())) //possible template case
            return true;
        return false;
    }

    string extractSuperClass(Instruction *this_ptr, BitCastInst *possible, bool &hasTrueClass)
    {
        //Single Inheritance
        if(auto load = dyn_cast<LoadInst>(possible->getOperand(0))) {
            if(this_ptr == dyn_cast<Instruction>(load->getOperand(0))) {
                std::string data_str;
                llvm::raw_string_ostream ss(data_str);
                possible->getDestTy()->getScalarType()->getScalarType()->print(ss);
                std::string s = ss.str();
                if(hasTrueClass)
                    return "";
                hasTrueClass = true;
                //fprintf(stderr, "norm name = %s\n", s.c_str());
                int l = s.length();
                if(s.substr(0,6) == "%class")
                    return s.substr(7, l-8);
                if(s.substr(0,7) == "%\"class")
                    return s.substr(8, l-10);
                if(s.substr(0,7) == "%struct")
                    return ss.str().substr(8, l-9);
                if(s.substr(0,8) == "%\"struct")
                    return s.substr(9, l-11);
                hasTrueClass = false;
            }
        }

        if(auto getelm = dyn_cast<GetElementPtrInst>(possible->getOperand(0))) {
            if(auto cast = dyn_cast<BitCastInst>(getelm->getOperand(0))) {
                if(auto load = dyn_cast<LoadInst>(cast->getOperand(0))) {
                    if(this_ptr == dyn_cast<Instruction>(load->getOperand(0))) {
                        if(!dyn_cast<ConstantInt>(getelm->getOperand(1)))
                            return "";
                        unsigned off = dyn_cast<ConstantInt>(getelm->getOperand(1))->getZExtValue()/8;
                        std::string data_str;
                        llvm::raw_string_ostream ss(data_str);
                        possible->getDestTy()->getScalarType()->getScalarType()->print(ss);
                        std::string s = ss.str();
                        //fprintf(stderr, "name = %s\n", s.c_str());
                        int l = s.length();
                        //if(off) {
                            if(s.substr(0,6) == "%class")
                                return s.substr(7, l-8)+"+"+to_s(off);
                            if(s.substr(0,7) == "%\"class")
                                return s.substr(8, l-10)+"+"+to_s(off);
                            if(s.substr(0,7) == "%struct")
                                return ss.str().substr(8, l-9)+"+"+to_s(off);
                            if(s.substr(0,8) == "%\"struct")
                                return s.substr(9, l-11)+"+"+to_s(off);
                        //} else {
                        //    if(s.substr(0,6) == "%class")
                        //        return s.substr(7, l-8);
                        //    if(s.substr(0,7) == "%\"class")
                        //        return s.substr(8, l-10);
                        //    if(s.substr(0,7) == "%struct")
                        //        return ss.str().substr(8, l-9);
                        //    if(s.substr(0,8) == "%\"struct")
                        //        return s.substr(9, l-11);
                        //}

                    }
                }
            }
        }

        return "";
    }

    string className(const char *name)
    {
        char *res = strdup(name);
        char *end_of_name = (char*)rindex(res, ':');
        if(end_of_name && end_of_name > res+1) {
            end_of_name[-1] = 0;
            return res;
        }
        return "";
    }

    void findSuperClasses(Function &Fn, string name, string fullname, string alias="")
    {
        //The first IR Element is an alloca for the this pointer
        //later there is a load of this %this.addr
        //if this load is cast into anything other than i8***, then that's a
        //super class
        //fprintf(stderr, "%s : %s : %s\n", name.c_str(), fullname.c_str(), alias.c_str());
        //Fn.dump();

        std::vector<string> class_list;
        Instruction *this_ptr = NULL;
        bool hasTrueClass = false;
        for(auto &bb:Fn) {
            for(auto &I:bb) {
                if(!this_ptr && dyn_cast<AllocaInst>(&I))
                    this_ptr = &I;

                if(auto bitcast = dyn_cast<BitCastInst>(&I))
                {
                    auto super = extractSuperClass(this_ptr, bitcast, hasTrueClass);
                    if(!super.empty())
                    {
                        bool not_already_here = true;
                        for(auto x:class_list)
                            if(x == super)
                                not_already_here = false;
                        if(not_already_here)
                            class_list.push_back(super);
                    }
                }
            }
        }

        //printf("name = '%s'\n", name.c_str());
        //printf("fullname = '%s'\n", fullname.c_str());
        //printf("classname = '%s'\n",
        //        className(fullname.c_str()).c_str());
        std::unique(class_list.begin(), class_list.end());
        if(!class_list.empty() || !alias.empty()) {
            fprintf(stderr, "%.1024s:\n", className(fullname.c_str()).c_str());
            if(!alias.empty())
                fprintf(stderr, "    - alias.%s\n", removeClassStruct(alias).c_str());
            for(auto x:class_list)
                fprintf(stderr, "    - %s\n", removeUniqueTail(x).c_str());
        }


    }
    
    std::string removeUniqueTail(std::string name)
    {
        char *dpos = rindex((char*)name.c_str(), '.');
        int off = INT_MAX;
        if(dpos && isdigit(dpos[1]))
            off = dpos-name.c_str();


        std::string replacement;
        int dots = 0;
        for(int i=0; i<name.length(); ++i) {
            char cur = name[i];
            if(i == off)
                replacement.append(".subtype");
            else
                replacement.push_back(cur);
        }
        return replacement;
    }

    bool runOnFunction(Function &Fn) override {
        assert(isTemplateConstructorp("mididings::Patch::ModuleImpl<mididings::Patch::Chain>::ModuleImpl()"));
        int status = 0;
        char *realname = abi::__cxa_demangle(Fn.getName().str().c_str(), 0, 0, &status);
        //fprintf(stderr, "realname='%s'\n", realname);
        if(realname) {
            string fullname = getFullName(realname);
            //fprintf(stderr, "truename='%s'\n", Fn.getName().str().c_str());
            //fprintf(stderr, "fullname='%s'\n", fullname.c_str());
            if(isConstructorp(fullname.c_str())) {
                //fprintf(stderr, "It's a constructor\n");
                findSuperClasses(Fn, getMethodName(realname), fullname);
            } else if(isTemplateConstructorp(fullname.c_str())) {
                //fprintf(stderr, "It's a template constructor\n");
                std::string alias_type = Fn.getArgumentList().front().getType()->getPointerElementType()->getScalarType()->getStructName().str();
                findSuperClasses(Fn, getMethodName(realname), fullname, removeUniqueTail(alias_type));
            }
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
        //Field 0/1 repeat with multiple inheritance
        if(!name || !v)
            return;
        unsigned ops = v->getNumOperands();
        if(ops >= 2)
            fprintf(stderr, "%.1024s:\n", name);
        int variant = 0;
        int ii = 0;
        for(unsigned i=2; i<ops; ++i)
        {
            auto op       = v->getOperand(i);
            char *fname = NULL;
            if(!op)
                continue;
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
                
            if(dyn_cast<ConstantPointerNull>(op)) {
                ii += 1;
                continue;
            }

            if(!fname) {
                int variant_id = 0;
                if(op->getNumOperands()) {
                    if(auto n = dyn_cast<ConstantInt>(op->getOperand(0))) {
                        if(n->getSExtValue() <= 1) {
                            continue;
                        }
                        variant_id = -n->getZExtValue()/8;
                        if(variant_id == -1) {
                            ii++;
                            continue;
                        }
                    } else {
                        ii++;
                        continue;
                    }
                }
                fprintf(stderr, "%s.variant%d:\n", name, variant_id);//++variant);
                i++;
                ii+=2;
                continue;
            }
            char *tmp = fname;
            while(tmp && *tmp && isprint(*tmp))
                tmp++;
            if(tmp)
                *tmp = 0;
            fprintf(stderr, "    %d: %.1024s\n", i-2-ii, fname);
            free(fname);
        }
    }

    bool runOnModule(Module &m) override {
        auto &gl = m.getGlobalList();
        for(auto &g:gl) {
            if(g.getName().str().substr(0,4) == "_ZTV") {
                std::string name(g.getName().str());
                const char *vtable_name = name.c_str();
                int status = 0;
                char *realname = abi::__cxa_demangle(vtable_name, 0, 0, &status);
                if(!realname || strstr(realname, "__cxxabi"))
                    continue;
                char *tmp = realname;
                while(*tmp && isprint(*tmp))
                    tmp++;
                *tmp = 0;

                if(g.getNumOperands() && strlen(realname) > 11)
                    handleVtable(realname+11, dyn_cast<ConstantArray>(g.getOperand(0)));
            }
        }
        return false;
    }
};

struct ExtractRtosc : public FunctionPass {
    static char ID;
    ExtractRtosc() : FunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
    }

    bool isCallback(GetElementPtrInst *getelm)
    {
        if(getelm->getNumOperands() != 3)
            return false;
        if(dyn_cast<ConstantInt>(getelm->getOperand(2))->getZExtValue() != 3)
            return false;
        //fprintf(stderr, "=================================\n");
        //getelm->dump();
        auto inst = dyn_cast<Instruction>(getelm->getOperand(0));
        std::string data_str;
        llvm::raw_string_ostream ss(data_str);
        auto xyz = inst->getType()->getPointerElementType()->getScalarType()->getStructName();
        //fprintf(stderr, "Type of '%s'\n", xyz.str().c_str());
        if(xyz == "struct.rtosc::Port")
            return true;
        return false;
    }

    bool isName(GetElementPtrInst *getelm)
    {
        if(getelm->getNumOperands() != 3)
            return false;
        if(dyn_cast<ConstantInt>(getelm->getOperand(2))->getZExtValue() != 0)
            return false;
        auto inst = dyn_cast<Instruction>(getelm->getOperand(0));
        std::string data_str;
        llvm::raw_string_ostream ss(data_str);
        auto type = inst->getType()->getArrayElementType();
        if(type->isArrayTy())
            return false;
        string name;
        if(type->isArrayTy())
            name = type->getPointerElementType()->getStructName().str();
        else
            name =  type->getStructName().str();

        if(name != "struct.rtosc::Port")
            return false;
        return true;
    }
    bool isMeta(GetElementPtrInst *getelm)
    {
        if(getelm->getNumOperands() != 3)
            return false;
        if(dyn_cast<ConstantInt>(getelm->getOperand(2))->getZExtValue() != 1)
            return false;
        auto inst = dyn_cast<Instruction>(getelm->getOperand(0));
        std::string data_str;
        llvm::raw_string_ostream ss(data_str);
        auto type = inst->getType()->getArrayElementType();
        if(type->isArrayTy())
            return false;
        string name;
        if(type->isArrayTy())
            name = type->getPointerElementType()->getStructName().str();
        else
            name =  type->getStructName().str();

        if(name != "struct.rtosc::Port")
            return false;
        return true;
    }

    GetElementPtrInst *runOnCallInst(GetElementPtrInst *getelm)
    {
        //Load the this pointer
        if(dyn_cast<LoadInst>(getelm->getOperand(0)))
            return getelm;
        return NULL;
    }

    string runOnLambdaConstructor(Function &Fn)
    {
        //This function starts with the alloca for the anonymous class
        //However, we don't really care about this too much
        //We want to find the M_invoke call which is places the lambda function
        //down
        Value *value = NULL;
        for(auto &bb:Fn) {
            for(auto &i:bb) {
                if(i.getOpcode() == Instruction::GetElementPtr && !value)
                    value = runOnCallInst(dyn_cast<GetElementPtrInst>(&i));
                if(i.getOpcode() == Instruction::Store) {
                    if(value && i.getOperand(1) == value) {
                        return dyn_cast<Function>(i.getOperand(0))->getName().str();
                    }
                }
            }
        }
        return "";
    }


#define NONE     (0)
#define CALLBACK (1)
#define NAME     (2)
#define META     (3)
    bool runOnFunction(Function &Fn) override {
        //TODO check for the existance of an __cxx_global_var_init$N for an
        //arbitrary N
        if(!(Fn.getName() == "__cxx_global_var_init"
                    || Fn.getName() == "__cxx_global_var_init1"
                    || Fn.getName() == "__cxx_global_var_init2"
                    || Fn.getName() == "__cxx_global_var_init3"
                    || Fn.getName() == "__cxx_global_var_init4"
                    || Fn.getName() == "__cxx_global_var_init5"
                    || Fn.getName() == "__cxx_global_var_init6"
                    || Fn.getName() == "__cxx_global_var_init7"
                    || Fn.getName() == "__cxx_global_var_init8"
                    || Fn.getName() == "__cxx_global_var_init9"))
            return false;

        int state = NONE;
        struct RtInfo {
            string name, meta, func;
        };
        std::vector<RtInfo> v;
        RtInfo current;

        //collect name then meta then func
        for(auto &bb:Fn) {
            for(auto &i:bb) {
                auto opcode = i.getOpcode();
                //i.dump();
                if(state == NONE && opcode == Instruction::GetElementPtr) {
                    auto inst = dyn_cast<GetElementPtrInst>(&i);
                    if(isCallback(inst)) state = CALLBACK;
                    if(isName(inst))     state = NAME;
                    if(isMeta(inst))     state = META;
                    continue;
                }

                if(state == CALLBACK) {
                    if(i.getOpcode() != Instruction::Invoke) {
                        state = NONE;
                        continue;
                    }

                    //There should be 4 arguments
                    //0: std::function memory pointer
                    //1: std::function<> constructor
                    //2: continue instruction pointer
                    //3: failure instruction pointer
                    auto fn = runOnLambdaConstructor(*dyn_cast<Function>(i.getOperand(1)));
                    current.func = fn;
                    v.push_back(current);
                    current = {"INVALID","INVALID","INVALID"};
                    state = NONE;
                }
                if(state == NAME) {
                    assert(i.getOpcode() == Instruction::Store);
                    auto data = dyn_cast<ConstantExpr>(i.getOperand(0))->getOperand(0);
                    auto arr  = dyn_cast<ConstantDataArray>(data->getOperand(0));
                    if(arr)
                        current.name = arr->getRawDataValues().str();
                    state = NONE;
                }

                if(state == META) {
                    assert(i.getOpcode() == Instruction::Store);
                    if(!dyn_cast<ConstantExpr>(i.getOperand(0))) {
                        current.meta = "";
                        state = NONE;
                        continue;
                    }

                    auto data = dyn_cast<ConstantExpr>(i.getOperand(0))->getOperand(0);
                    auto arr  = dyn_cast<ConstantDataArray>(data->getOperand(0));
                    if(arr)
                        current.meta = arr->getRawDataValues().str();
                    state = NONE;
                }
            }
        }
        for(auto x:v) {
            fprintf(stderr, "-\n");
            llvm::errs() << "  name: ";
            escapeOutput(x.name.c_str());
            llvm::errs() << "\n";
            llvm::errs() << "  meta: ";
            escapeOutput(x.meta);
            llvm::errs() << "\n";
            llvm::errs() << "  func: " << x.func << "\n";
        }
        return false;
    }
};
}

char ExtractCallGraph::ID = 0;
char ExtractAnnotations::ID = 0;
char ExtractClassHierarchy::ID = 0;
char ExtractVtables::ID = 0;
char ExtractRtosc::ID = 0;
static RegisterPass<ExtractCallGraph> P1("extract-callgraph", "Print YAML Representation of Callgraph including vTable Calls");
static RegisterPass<ExtractAnnotations> P2("extract-annotations", "Print YAML Representation of Function Annotations");
static RegisterPass<ExtractClassHierarchy> P3("extract-class-hierarchy", "Print YAML Representation of Class Hierarchy Derived From Constructors");
static RegisterPass<ExtractVtables> P4("extract-vtables", "Print YAML Representation of VTable's Function Pointer Contents");
static RegisterPass<ExtractRtosc> P5("extract-rtosc", "Print YAML Representation of rtosc Dispatch Calls");
