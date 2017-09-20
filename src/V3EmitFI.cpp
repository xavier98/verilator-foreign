// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Block code ordering
//
// Code available from: http://www.veripool.org/verilator
//
//*************************************************************************
//
// Copyright 2003-2017 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "config_build.h"
#include "verilatedos.h"
#include "V3EmitFI.h"
#include "V3EmitV.h"

//######################################################################
// V3EmitFIVisitor

class V3EmitFIVisitor : public EmitVBaseVisitor {
    // MEMBERS
    V3OutFile*	m_ofp;
    bool m_first_port;
    AstNodeModule* m_modp;
  
    // METHODS
    V3OutFile*	ofp() const { return m_ofp; }
    virtual void puts(const string& str) { ofp()->puts(str); }
    virtual void putbs(const string& str) { ofp()->putbs(str); }
    virtual void putfs(AstNode*, const string& str) { putbs(str); }
    virtual void putqs(AstNode*, const string& str) { putbs(str); }
    virtual void putsNoTracking(const string& str) { ofp()->putsNoTracking(str); }

    void visit(AstVar* nodep) {
	if (!m_first_port)
	    puts(",\n");
	putfs(nodep,nodep->verilogKwd());
	puts(" ");
	nodep->dtypep()->iterate(*this); puts(" ");
	puts(nodep->prettyName());
    }

public:
    V3EmitFIVisitor(V3OutFile* ofp)
	: m_ofp(ofp), m_first_port(true) {
    }
    virtual ~V3EmitFIVisitor() {}

    void emitModuleOpen(AstNodeModule* nodep) {
	// Tracing is disabled on the foreign interface, so the outer module
	// doesn't generate traces for the foreign interface ports, which will
	// be duplicated by the inner module.
	puts("// verilator tracing_off\n");
	puts("// verilator lint_off UNOPTFLAT\n"); // * fix me; imported settles cause these to appear
	putfs(nodep, nodep->verilogKwd()+" foreign_"+nodep->foreignName()+" (\n");
	m_modp = nodep;
    }
    void emitModulePort(AstVar* port_var) {
	visit(port_var);
	m_first_port = false;
    }
    void emitModuleOpenDone() {
	puts(");\n");
	puts("// verilator inline_module\n");
	puts("// verilator foreign_interface ");
	puts(m_modp->foreignName());
	puts("\n");
    }
    void emitModuleClose(AstNodeModule* nodep) {
	putqs(nodep, "end"+nodep->verilogKwd()+"\n");
    }

    void emitSentreeInitial() {
	puts("initial begin\n");
    }
    void emitSentreeFinal() {
	puts("final begin\n");
    }
    void emitSentreeAlways(AstSenTree* sensesp) {
	puts("always ");
	if (sensesp->hasSettle())
	    puts("@(foreign_settle)");
	else
	    EmitVBaseVisitor::visit(sensesp);
	puts(" begin\n");
    }
    void emitSentreeClose() {
	puts("end\n");
    }

    void emitForeignEval(AstCFunc* func) {
	puts("// verilator foreign_eval ");
	if (v3Global.opt.trace())
	    puts("_foreign");
	puts(func->name());
	puts("\n");
    }
    void emitForeignDepend(AstCFunc* func) {
	puts("// verilator foreign_depend ");
	if (v3Global.opt.trace())
	    puts("_foreign");
	puts(func->name());
	puts("\n");
    }
    void emitForeignRead(AstVar* var) {
	puts("// verilator foreign_read ");
	puts(var->name());
	puts("\n");
    }
    void emitForeignReadPost(AstVar* var) {
	puts("// verilator foreign_read_post ");
	puts(var->name());
	puts("\n");
    }
    void emitForeignWrite(AstVar* var) {
	puts("// verilator foreign_write ");
	puts(var->name());
	puts("\n");
    }

};

// * collect should be based on AstVarScope* or AstVar*?

class V3CollectForeignRefs : public AstNVisitor {
    // STATE
    AstNodeModule* m_modp;
    AstScope* m_scopetopp;

    // AstVar::user2() indicates whether result of AstAssignPost
    AstUser2InUse m_inuser2;

    enum BlockType {
	ALWAYS,
	INITIAL,
	FINAL
    };
    struct WriteInfo {
	AstVar* m_varp;
	bool m_post;
	WriteInfo()
	    : m_varp(NULL), m_post(false) {
	}
	WriteInfo(AstVar* varp, bool is_post)
	    : m_varp(varp), m_post(is_post) {
	}
	bool operator< (const WriteInfo& rhs) const {
	    if (m_varp != rhs.m_varp)
		return m_varp < rhs.m_varp;
	    return m_post < rhs.m_post;
	}
    };
    typedef AstVar* EvalRead;
    struct EvalInfo {
	BlockType m_block_type;
	AstSenTree* m_sensesp;
	AstCFunc* m_funcp;
	set<EvalInfo*> m_depends;
	set<AstVar*> m_port_reads;
     	set<WriteInfo> m_port_writes;	
	set<AstVar*> m_all_reads;
	set<WriteInfo> m_all_writes;
	set<pair<AstForeignInstance*,string> > m_inner_evals;
	set<pair<AstForeignInstance*,string> > m_inner_depends;
    };
    vector<EvalInfo> m_evals;
    EvalInfo* m_evalp;
    AstForeignEval* m_fe;
    AstSenTree* m_sensesp;
    bool m_inPost;

    vector<AstVar*> m_ports;

    enum AssignCapture {
	ASSIGN_CAPTURE_WRITE,
	ASSIGN_CAPTURE_READ
    } m_assign_capture;

    // VISITORS
    virtual void visit(AstTopScope* nodep) {
	if (m_scopetopp) nodep->v3fatalSrc("Only one topscope should ever be created");
	AstNode::user2ClearTree();
	m_scopetopp = nodep->scopep();
	nodep->iterateChildren(*this);
    }

    virtual void visit(AstNode* nodep) {
	nodep->iterateChildren(*this);
    }

    virtual void visit(AstVar* nodep) {
	if (nodep->isPrimaryIO() && (nodep->isInput() || nodep->isOutput()))
	    m_ports.push_back(nodep);
    }

    virtual void visit(AstForeignDepend* nodep) {
	m_evalp->m_inner_depends.insert(make_pair(m_fe->foreignInstance(),nodep->name()));
    }

    virtual void visit(AstForeignEval* nodep) {
	if (nodep->unconditional())
	    return;
	if (!m_evalp)
	    nodep->v3fatalSrc("conditional foreign eval not under eval");

	m_evalp->m_inner_evals.insert(make_pair(nodep->foreignInstance(),nodep->name()));

	if (nodep->reads()) {
	    m_assign_capture = ASSIGN_CAPTURE_WRITE;
	    nodep->reads()->iterateAndNext(*this);
	}

	if (nodep->writes()) {
	    m_assign_capture = ASSIGN_CAPTURE_READ;
	    nodep->writes()->iterateAndNext(*this);
	}

	if (nodep->depends()) {
	    m_fe = nodep;
	    nodep->depends()->iterateAndNext(*this);
	    m_fe = NULL;
	}
	
    }

    virtual void visit(AstVarRef* nodep) {
	if (!m_evalp)
	    return;

	if (m_assign_capture == ASSIGN_CAPTURE_WRITE && m_inPost)
	    nodep->varp()->user2(true);
	if (nodep->varp()->user2())
	    m_inPost = true;

	if (m_assign_capture == ASSIGN_CAPTURE_WRITE) {
	    if (nodep->varp()->isPrimaryIO() && nodep->varp()->isOutput())
		m_evalp->m_port_writes.insert(WriteInfo(nodep->varp(),m_inPost));
	    m_evalp->m_all_writes.insert(WriteInfo(nodep->varp(),m_inPost));
	} else if (m_assign_capture == ASSIGN_CAPTURE_READ) {
	    if (nodep->varp()->isPrimaryIO() && nodep->varp()->isInput())
		m_evalp->m_port_reads.insert(nodep->varp());
	    m_evalp->m_all_reads.insert(nodep->varp());
	}
    }

    virtual void visit(AstNodeAssign* nodep) {
	m_assign_capture = ASSIGN_CAPTURE_READ;
	nodep->rhsp()->iterate(*this);
	m_assign_capture = ASSIGN_CAPTURE_WRITE;
	nodep->lhsp()->iterate(*this);
	m_inPost = false;
    }

    virtual void visit(AstAssignPost* nodep) {
	m_assign_capture = ASSIGN_CAPTURE_READ;
	nodep->rhsp()->iterate(*this);
	m_assign_capture = ASSIGN_CAPTURE_WRITE;
	m_inPost = true;
	nodep->lhsp()->iterate(*this);
	m_inPost = false;
    }

    virtual void visit(AstCCall* nodep) {
	if (nodep->funcp()->stmtsp()) {

	    m_evalp = &*m_evals.insert(m_evals.end(),EvalInfo());
	    m_evalp->m_block_type = m_sensesp &&
		m_sensesp->hasInitial() ? INITIAL : ALWAYS;
	    m_evalp->m_sensesp = m_sensesp;
	    m_evalp->m_funcp = nodep->funcp();

	    nodep->funcp()->stmtsp()->iterateAndNext(*this);
	}
    }

    virtual void visit(AstCFunc* nodep) {
    }

    virtual void visit(AstActive* nodep) {
	m_sensesp = nodep->sensesp();
	nodep->iterateChildren(*this);
	m_sensesp = NULL;
    }

    void emitDefaultWrites(V3EmitFIVisitor& fi_v) {
	set<AstVar*> all_port_reads;
	for (size_t i=0;i<m_evals.size();++i) {
	    EvalInfo& fe = m_evals[i];
	    all_port_reads.insert(fe.m_port_reads.begin(),fe.m_port_reads.end());
	}

	for (size_t i=0;i<m_ports.size();++i) {
	    AstVar* port = m_ports[i];
	    if (!port->isInput())
		continue;
	    if (all_port_reads.find(port) == all_port_reads.end())
		fi_v.emitForeignWrite(port);
	}
    }
  
public:
    V3CollectForeignRefs(AstNodeModule* modp)
	: m_modp(modp),
	  m_scopetopp(NULL),
	  m_assign_capture(ASSIGN_CAPTURE_READ),
	  m_evalp(NULL),
	  m_fe(NULL),
	  m_sensesp(NULL),
	  m_inPost(false) {
	UINFO(2,"    process...\n");
	m_modp->accept(*this);

	// generate EvalInfo::m_depends
	UINFO(2,"    collect depends...\n");
	map<AstVar*, std::vector<EvalInfo*> > generators;
	map<pair<AstForeignInstance*,string>, std::vector<EvalInfo*> > inner_evals;
	for (size_t i=0;i<m_evals.size();++i) {
	    EvalInfo& fe = m_evals[i];
	    for (set<WriteInfo>::iterator it=fe.m_all_writes.begin();
		 it!=fe.m_all_writes.end();++it) {
		generators[it->m_varp].push_back(&fe);
	    }
	    for (set<pair<AstForeignInstance*,string> >::iterator
		     it=fe.m_inner_evals.begin();it!=fe.m_inner_evals.end();++it) {
		inner_evals[*it].push_back(&fe);
	    }
	}
	for (size_t i=0;i<m_evals.size();++i) {
	    EvalInfo& fe = m_evals[i];
	    for (set<AstVar*>::iterator it=fe.m_all_reads.begin();
		 it!=fe.m_all_reads.end();++it) {
		std::vector<EvalInfo*>& depends_fep = generators[*it];
		for (size_t j=0;j<depends_fep.size();++j) {
		    EvalInfo* src_fe = depends_fep[j];
		    if (&fe == src_fe)
			continue;
		    if (fe.m_sensesp && fe.m_sensesp->hasSettle() &&
			(!src_fe->m_sensesp || !src_fe->m_sensesp->hasSettle()))
			continue;
		    fe.m_depends.insert(src_fe);
		}
	    }
	    for (set<pair<AstForeignInstance*,string> >::iterator
		     it=fe.m_inner_depends.begin();it!=fe.m_inner_depends.end();++it) {
		std::vector<EvalInfo*>& depends_fep = inner_evals[*it];
		for (size_t j=0;j<depends_fep.size();++j) {
		    EvalInfo* src_fe = depends_fep[j];
		    if (&fe == src_fe)
			continue;
		    if (fe.m_sensesp && fe.m_sensesp->hasSettle() &&
			(!src_fe->m_sensesp || !src_fe->m_sensesp->hasSettle()))
			continue;
		    fe.m_depends.insert(src_fe);
		}		
	    }
	}
    }

    void emit(V3OutFile* ofp) {
	V3EmitFIVisitor fi_v(ofp);

	fi_v.emitModuleOpen(m_modp);

	for (size_t i=0;i<m_ports.size();++i)
	    fi_v.emitModulePort(m_ports[i]);
    
	fi_v.emitModuleOpenDone();

	// Emit list of variables not otherwise written to the inner module (e.g.,
	// sentree items such as clocks that are only used in sensitivity lists
	// but not logic). This is to support tracing and inspection; at exit of
	// outer module eval(), all inner modules should have these symbols set
	// to correct values.
	emitDefaultWrites(fi_v);

	// Emit a sensitivity list per inner module eval function
	for (size_t i=0;i<m_evals.size();++i) {
	    EvalInfo& fe = m_evals[i];

	    // Emit the sensitivity list. Note that sensitivity list items
	    // currently must be in terms of port signals (i.e., that
	    // will exist in the outer module per the cell instantiation).
	    if (fe.m_block_type == INITIAL)
		fi_v.emitSentreeInitial();
	    else if (fe.m_block_type == FINAL)
		fi_v.emitSentreeFinal();
	    else if (fe.m_block_type == ALWAYS)
		fi_v.emitSentreeAlways(fe.m_sensesp);

	    // Emit the inner eval function name
	    fi_v.emitForeignEval(fe.m_funcp);

	    // Emit the list of inner module evals this one depends on;
	    // this is used for inter-inner-eval dependendies needed in
	    // V3Gate and V3Order, that would otherwise be lost/hidden in
	    // the inner module implementation. Note that the inner-module
	    // evals are scheduled by the outer module. 
	    for (set<EvalInfo*>::iterator it=fe.m_depends.begin();
		 it!=fe.m_depends.end();++it)
		fi_v.emitForeignDepend((*it)->m_funcp);

	    // Emit the list of ports the outer module must populate
	    // before calling the eval function.
	    for (set<AstVar*>::iterator it=fe.m_port_reads.begin();
		 it!=fe.m_port_reads.end();++it)
		fi_v.emitForeignWrite(*it);

	    // Emit the list of ports whose value will have changed after
	    // running the eval function.
	    for (set<WriteInfo>::iterator it=fe.m_port_writes.begin();
		 it!=fe.m_port_writes.end();++it) {
		if (it->m_post)
		    fi_v.emitForeignReadPost(it->m_varp);
		else
		    fi_v.emitForeignRead(it->m_varp);
	    }

	    fi_v.emitSentreeClose();
	}

	fi_v.emitModuleClose(m_modp);
    }

    void addEntryPoints() {

	// If tracing is enabled, add _foreign_xyz stub functions that will
	// simply dispatch to the actual eval routines but also get annotated
	// with trace activity updates by V3Trace.

	if (!v3Global.opt.trace())
	    return;

	set<AstCFunc*> evals;
	for (size_t i=0;i<m_evals.size();++i)
	    evals.insert(m_evals[i].m_funcp);

	for (set<AstCFunc*>::iterator it=evals.begin();
	     it!=evals.end();++it) {
	    AstCFunc* f = *it;

	    AstCFunc* ff = new AstCFunc(f->fileline(), "_foreign"+f->name(), m_scopetopp);
	    ff->argTypes(EmitCBaseVisitor::symClassVar());
	    ff->symProlog(true);
	    AstCCall* callp = new AstCCall(f->fileline(), f);
	    callp->argTypes("vlSymsp");
	    ff->addStmtsp(callp);

	    ff->addStmtsp(new AstText(f->fileline(), "vlSymsp->__Vm_activity = true;\n", true));
	    
	    m_scopetopp->addActivep(ff);
	}
	
    }
};

//######################################################################
// V3EmitFIUnpack

class V3EmitFIUnpack : public AstNVisitor {
    AstForeignEval* m_fe;
    AstScope* m_scopetopp;	// Scope under TOPSCOPE
    AstNode* m_unpack_seq;

    map<pair<AstForeignInstance*, string>, AstVarRef*> m_foreign_ports;
    map<pair<AstForeignInstance*, string>, AstCFunc*> m_foreign_funcs;

    std::vector<AstForeignEval*> m_uncond_evals;
    std::vector<AstForeignInstance*> m_all_fi;

    AstVarRef* foreignPortVar(const std::string& port_name, AstNodeDType* dtp, bool lvalue) {
	AstForeignInstance* fi = m_fe->foreignInstance();
	map<pair<AstForeignInstance*, string>, AstVarRef*>::iterator
	    it = m_foreign_ports.find(make_pair(fi, port_name));
	if (it != m_foreign_ports.end()) {
	    if (it->second->lvalue() != lvalue)
		dtp->v3fatalSrc("inconsistent port direction");
	    return it->second;
	}
	
	string varname = "__F"+fi->name()+"->"+port_name;
	AstVar* varp = new AstVar (m_fe->fileline(), AstVarType::MODULETEMP, varname, dtp);
	varp->sigPublic(true);
	AstVarScope* scopep = new AstVarScope(m_fe->fileline(), m_scopetopp, varp);
	m_scopetopp->addActivep(varp);
	m_scopetopp->addActivep(scopep);
	AstVarRef* varrefp = new AstVarRef(m_fe->fileline(), scopep, lvalue);

	m_foreign_ports.insert(make_pair(make_pair(fi, port_name), varrefp));

	return varrefp;
    }

    AstCFunc* foreignEvalFunc() {
	AstForeignInstance* fi = m_fe->foreignInstance();
	map<pair<AstForeignInstance*, string>, AstCFunc*>::iterator
	    it = m_foreign_funcs.find(make_pair(fi, m_fe->name()));
	if (it != m_foreign_funcs.end())
	    return it->second;

	AstCFunc* funcp = new AstCFunc(m_fe->fileline(), "V"+fi->modName()+"::"+m_fe->name(), m_scopetopp);
	funcp->skipDecl(true);
	funcp->funcType(AstCFuncType::FT_FOREIGN);
	funcp->dontCombine(true);
	funcp->isStatic(true);
	m_scopetopp->addActivep(funcp);

	m_foreign_funcs.insert(make_pair(make_pair(fi, m_fe->name()), funcp));

	return funcp;
    }

    void emitUncondEvals() {
	m_unpack_seq = NULL;

	// Generate _foreign_uncond that will execute all
	// unconditional foreign_writes to our sub-modules
	AstCFunc* funcp = new AstCFunc(m_scopetopp->fileline(), "_foreign_uncond", m_scopetopp);
	funcp->argTypes(EmitCBaseVisitor::symClassVar());
	funcp->symProlog(true);
	funcp->dontCombine(true); // allow empty _foreign_eval, since unknown outer module may call it
	funcp->isStatic(true);
	m_scopetopp->addActivep(funcp);

	funcp->addStmtsp(new AstText(funcp->fileline(), "vlSymsp->__Vm_activity = true;\n", true));

	// For each sub-module that has unconditional writes,
	// unpack the writes
	for (size_t i=0;i<m_uncond_evals.size();++i) {
	    AstForeignEval* nodep = m_uncond_evals[i];
	    m_fe = nodep;

	    AstNode* last_unpack_seq = m_unpack_seq;
	    if (nodep->writes())
		nodep->writes()->iterateAndNext(*this);
	    
	    nodep->unlinkFrBack();
	    pushDeletep(nodep);

	    m_fe = NULL;
	}
	if (m_unpack_seq)
	    funcp->addStmtsp(m_unpack_seq);

	// Call _foreign_uncond on sub-modules
	map<string, AstCFunc*> created_funcs;
	for (size_t i=0;i<m_all_fi.size();++i) {
	    AstForeignInstance* fi = m_all_fi[i];

	    map<string, AstCFunc*>::iterator
		it = created_funcs.find(fi->modName());
	    if (it == created_funcs.end()) {
		AstCFunc* sub_funcp = new AstCFunc(m_scopetopp->fileline(), "V"+fi->modName()+"::_foreign_uncond", m_scopetopp);
		sub_funcp->skipDecl(true);
		sub_funcp->funcType(AstCFuncType::FT_FOREIGN);
		sub_funcp->dontCombine(true);
		sub_funcp->isStatic(true);
		m_scopetopp->addActivep(sub_funcp);
		it = created_funcs.insert(make_pair(fi->modName(), sub_funcp)).first;
	    }
	    AstCFunc* sub_funcp = it->second;
	    
	    AstCCall* callp = new AstCCall(m_scopetopp->fileline(), sub_funcp);
	    funcp->addStmtsp(new AstText(funcp->fileline(), "VL_DEBUG_PUSH_FOREIGN_SCOPE(\""+fi->name()+"\");\n", true));
	    callp->argTypes("vlTOPp->__F"+fi->name()+"->__VlSymsp");
	    funcp->addStmtsp(callp);
	    funcp->addStmtsp(new AstText(funcp->fileline(), "VL_DEBUG_POP_FOREIGN_SCOPE();\n", true));
	}

	m_unpack_seq = NULL;
    }

    virtual void visit(AstNode* nodep) {
	nodep->iterateChildren(*this);
    }

    virtual void visit(AstTopScope* nodep) {
	AstScope* scopep = nodep->scopep();
	m_scopetopp = scopep;
	nodep->iterateChildren(*this);

	emitUncondEvals();
    }

    virtual void visit(AstForeignInstance* nodep) {
	m_all_fi.push_back(nodep);
    }
    
    virtual void visit(AstForeignRead* nodep) {
	AstVarRef* port_var = foreignPortVar(nodep->name(), nodep->op1p()->dtypep(), false)->cloneTree(true);
	AstNode* dst = nodep->dst()->cloneTree(true);
	AstAssign* assign = new AstAssign(nodep->fileline(), dst, port_var);
	if (m_unpack_seq)
	    m_unpack_seq->addNextNull(assign);
	else
	    m_unpack_seq = assign;
    }
    virtual void visit(AstForeignWrite* nodep) {
    	AstVarRef* port_var = foreignPortVar(nodep->name(), nodep->op1p()->dtypep(), true)->cloneTree(true);
	AstNode* src = nodep->src()->cloneTree(true);
    	AstAssign* assign = new AstAssign(nodep->fileline(), port_var, src);
	if (m_unpack_seq)
	    m_unpack_seq->addNextNull(assign);
	else
	    m_unpack_seq = assign;
    }

    virtual void visit(AstForeignEval* nodep) {
	// Unconditional evals (that is, evals that contain the set of
	// unconditional AstForeignWrite) get handled separatedly since
	// they are not bound to a foreign eval func that is scheduled
	// by the outer module, but instead are called via bottom of
	// outermost-module's _eval.
	if (nodep->unconditional()) {
	    m_uncond_evals.push_back(nodep);
	    return;
	}

	m_fe = nodep;
	m_unpack_seq = NULL;

	// unpack ForeignWrites into AstAssign ahead of the foreign eval call
	if (nodep->writes())
	    nodep->writes()->iterateAndNext(*this);

	// unpack the foreign eval call itself
	AstForeignInstance* fi = m_fe->foreignInstance();
	AstCFunc* funcp = foreignEvalFunc();
	AstText* scope_enter = new AstText(funcp->fileline(), "VL_DEBUG_PUSH_FOREIGN_SCOPE(\""+fi->name()+"\");\n", true);
	AstText* scope_exit = new AstText(funcp->fileline(), "VL_DEBUG_POP_FOREIGN_SCOPE();\n", true);
	AstCCall* callp = new AstCCall(m_fe->fileline(), funcp);
	callp->argTypes("vlTOPp->__F"+fi->name()+"->__VlSymsp");
	if (m_unpack_seq)
	    m_unpack_seq->addNextNull(scope_enter);
	else
	    m_unpack_seq = scope_enter;
	m_unpack_seq->addNextNull(callp);
	m_unpack_seq->addNextNull(scope_exit);

	// unpack ForeignReads into AstAssign after the foreign eval call
	if (nodep->reads())
	    nodep->reads()->iterateAndNext(*this);

	// The unpack sequence replaces the original AstForeignEval
	nodep->replaceWith(m_unpack_seq);
	pushDeletep(nodep);

    	m_fe = NULL;
	m_unpack_seq = NULL;
    }

public:
    V3EmitFIUnpack(AstNodeModule* modp)
	: m_fe(NULL),
	  m_unpack_seq(NULL),
	  m_scopetopp(NULL) {
	modp->accept(*this);
    }
};

//######################################################################
// V3EmitFIImpl

class V3EmitFIImpl {
    V3OutVFile* newOutVFile(AstNodeModule* modp) {
	string filename = v3Global.opt.makeDir()+"/foreign_"+modp->foreignName()+".v";

	return new V3OutVFile(filename);
    }

public:
   
    void emitFI(AstNetlist* nodep) {

	for (AstNodeModule* nodep = v3Global.rootp()->modulesp(); nodep; nodep=nodep->nextp()->castNodeModule()) {

	    if (v3Global.opt.genForeignInterface() || nodep->foreignModule()) {

		V3OutVFile* v_file = newOutVFile(nodep);
		
		// generate the exported foreign interface spec
		UINFO(2,"  Collect foreign refs...\n");
		V3CollectForeignRefs collect_frefs(nodep);
		UINFO(2,"  Emit foreign interface...\n");
		collect_frefs.emit(v_file);
		UINFO(2,"  Add traceable entry points...\n");
		collect_frefs.addEntryPoints();
		
		delete v_file;

	    }

	    // translate AstForeignEval to AstAssign and AstCCall
	    UINFO(2,"  Unpack foreign evals...\n");
	    V3EmitFIUnpack unpack_fe(nodep);

	    UINFO(2,"  done...\n");
	}
  
    }

};

void V3EmitFI::emitFI(AstNetlist* nodep) {

    V3EmitFIImpl impl;
    impl.emitFI(nodep);
    V3Global::dumpCheckGlobalTree("emitfi.tree", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);

}
