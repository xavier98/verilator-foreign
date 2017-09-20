// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Emit Verilog from tree
//
// Code available from: http://www.veripool.org/verilator
//
//*************************************************************************
//
// Copyright 2004-2017 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <cmath>
#include <map>
#include <vector>
#include <algorithm>

#include "V3Global.h"
#include "V3EmitV.h"

//######################################################################
// EmitVBaseVisitor class functions

int EmitVBaseVisitor::debug() {
    static int level = -1;
    if (VL_UNLIKELY(level < 0)) level = v3Global.opt.debugSrcLevel(__FILE__);
    return level;
}

void EmitVBaseVisitor::putsQuoted(const string& str) {
    // Quote \ and " for use inside C programs
    // Don't use to quote a filename for #include - #include doesn't \ escape.
    // Duplicate in V3File - here so we can print to string
    putsNoTracking("\"");
    putsNoTracking(V3Number::quoteNameControls(str));
    putsNoTracking("\"");
}

void EmitVBaseVisitor::visit(AstNetlist* nodep) {
    nodep->iterateChildren(*this);
}

void EmitVBaseVisitor::visit(AstNodeModule* nodep) {
    putfs(nodep, nodep->verilogKwd()+" "+modClassName(nodep)+";\n");
    nodep->iterateChildren(*this);
    putqs(nodep, "end"+nodep->verilogKwd()+"\n");
}

void EmitVBaseVisitor::visit(AstNodeFTask* nodep) {
    putfs(nodep, nodep->isFunction() ? "function":"task");
    puts(" ");
    puts(nodep->prettyName());
    puts(";\n");
    putqs(nodep, "begin\n");  // Only putfs the first time for each visitor; later for same node is putqs
    nodep->stmtsp()->iterateAndNext(*this);
    putqs(nodep, "end\n");
}

void EmitVBaseVisitor::visit(AstBegin* nodep) {
    if (nodep->unnamed()) {
	putbs("begin\n");
    } else {
	putbs("begin : "+nodep->name()+"\n");
    }
    nodep->iterateChildren(*this);
    puts("end\n");
}

void EmitVBaseVisitor::visit(AstGenerate* nodep) {
    putfs(nodep, "generate\n");
    nodep->iterateChildren(*this);
    putqs(nodep, "end\n");
}

void EmitVBaseVisitor::visit(AstFinal* nodep) {
    putfs(nodep, "final begin\n");
    nodep->iterateChildren(*this);
    putqs(nodep, "end\n");
}

void EmitVBaseVisitor::visit(AstInitial* nodep) {
    putfs(nodep,"initial begin\n");
    nodep->iterateChildren(*this);
    putqs(nodep, "end\n");
}

void EmitVBaseVisitor::visit(AstAlways* nodep) {
    putfs(nodep,"always ");
    if (m_sensesp) m_sensesp->iterateAndNext(*this);  // In active
    else nodep->sensesp()->iterateAndNext(*this);
    putbs(" begin\n");
    nodep->bodysp()->iterateAndNext(*this);
    putqs(nodep,"end\n");
}

void EmitVBaseVisitor::visit(AstAlwaysPublic* nodep) {
    putfs(nodep,"/*verilator public_flat_rw ");
    if (m_sensesp) m_sensesp->iterateAndNext(*this);  // In active
    else nodep->sensesp()->iterateAndNext(*this);
    putqs(nodep," ");
    nodep->bodysp()->iterateAndNext(*this);
    putqs(nodep,"*/\n");
}

void EmitVBaseVisitor::visit(AstNodeAssign* nodep) {
    nodep->lhsp()->iterateAndNext(*this);
    putfs(nodep," "+nodep->verilogKwd()+" ");
    nodep->rhsp()->iterateAndNext(*this);
    if (!m_suppressSemi) puts(";\n");
}

void EmitVBaseVisitor::visit(AstAssignDly* nodep) {
    nodep->lhsp()->iterateAndNext(*this);
    putfs(nodep," <= ");
    nodep->rhsp()->iterateAndNext(*this);
    puts(";\n");
}

void EmitVBaseVisitor::visit(AstAssignAlias* nodep) {
    putbs("alias ");
    nodep->lhsp()->iterateAndNext(*this);
    putfs(nodep," = ");
    nodep->rhsp()->iterateAndNext(*this);
    if (!m_suppressSemi) puts(";\n");
}

void EmitVBaseVisitor::visit(AstAssignW* nodep) {
    putfs(nodep,"assign ");
    nodep->lhsp()->iterateAndNext(*this);
    putbs(" = ");
    nodep->rhsp()->iterateAndNext(*this);
    if (!m_suppressSemi) puts(";\n");
}

void EmitVBaseVisitor::visit(AstBreak* nodep) {
    putbs("break");
    if (!m_suppressSemi) puts(";\n");
}

void EmitVBaseVisitor::visit(AstSenTree* nodep) {
    // AstSenItem is called for dumping in isolation by V3Order
    putfs(nodep,"@(");
    for (AstNode* expp=nodep->sensesp(); expp; expp = expp->nextp()) {
	expp->accept(*this);
	if (expp->nextp()) putqs(expp->nextp()," or ");
    }
    puts(")");
}

void EmitVBaseVisitor::visit(AstSenGate* nodep) {
    emitVerilogFormat(nodep, nodep->emitVerilog(), nodep->sensesp(), nodep->rhsp());
}

void EmitVBaseVisitor::visit(AstSenItem* nodep) {
    putfs(nodep,"");
    puts(nodep->edgeType().verilogKwd());
    if (nodep->sensp()) puts(" ");
    nodep->iterateChildren(*this);
}

void EmitVBaseVisitor::visit(AstNodeCase* nodep) {
    putfs(nodep,"");
    if (AstCase* casep = nodep->castCase()) {
	if (casep->priorityPragma()) puts("priority ");
	if (casep->uniquePragma()) puts("unique ");
	if (casep->unique0Pragma()) puts("unique0 ");
    }
    puts(nodep->verilogKwd());
    puts(" (");
    nodep->exprp()->iterateAndNext(*this);
    puts(")\n");
    if (AstCase* casep = nodep->castCase()) {
	if (casep->fullPragma() || casep->parallelPragma()) {
	    puts(" // synopsys");
	    if (casep->fullPragma()) puts(" full_case");
	    if (casep->parallelPragma()) puts(" parallel_case");
	}
    }
    nodep->itemsp()->iterateAndNext(*this);
    putqs(nodep,"endcase\n");
}

void EmitVBaseVisitor::visit(AstCaseItem* nodep) {
    if (nodep->condsp()) {
	nodep->condsp()->iterateAndNext(*this);
    } else putbs("default");
    putfs(nodep,": begin ");
    nodep->bodysp()->iterateAndNext(*this);
    putqs(nodep,"end\n");
}

void EmitVBaseVisitor::visit(AstComment* nodep) {
    puts((string)"// "+nodep->name()+"\n");
    nodep->iterateChildren(*this);
}

void EmitVBaseVisitor::visit(AstContinue* nodep) {
    putbs("continue");
    if (!m_suppressSemi) puts(";\n");
}

void EmitVBaseVisitor::visit(AstCoverDecl*) {  // N/A
}

void EmitVBaseVisitor::visit(AstCoverInc*) {  // N/A
}

void EmitVBaseVisitor::visit(AstCoverToggle*) {  // N/A
}

void EmitVBaseVisitor::visitNodeDisplay(AstNode* nodep, AstNode* fileOrStrgp, const string& text, AstNode* exprsp) {
    putfs(nodep,nodep->verilogKwd());
    putbs(" (");
    if (fileOrStrgp) { fileOrStrgp->iterateAndNext(*this); putbs(","); }
    putsQuoted(text);
    for (AstNode* expp=exprsp; expp; expp = expp->nextp()) {
	puts(",");
	expp->iterateAndNext(*this);
    }
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstDisable* nodep) {
    putbs("disable "+nodep->name()+";\n");
}

void EmitVBaseVisitor::visit(AstDisplay* nodep) {
    visitNodeDisplay(nodep, nodep->filep(), nodep->fmtp()->text(), nodep->fmtp()->exprsp());
}

void EmitVBaseVisitor::visit(AstFScanF* nodep) {
    visitNodeDisplay(nodep, nodep->filep(), nodep->text(), nodep->exprsp());
}

void EmitVBaseVisitor::visit(AstSScanF* nodep) {
    visitNodeDisplay(nodep, nodep->fromp(), nodep->text(), nodep->exprsp());
}

void EmitVBaseVisitor::visit(AstSFormat* nodep) {
    visitNodeDisplay(nodep, nodep->lhsp(), nodep->fmtp()->text(), nodep->fmtp()->exprsp());
}

void EmitVBaseVisitor::visit(AstSFormatF* nodep) {
    visitNodeDisplay(nodep, NULL, nodep->text(), nodep->exprsp());
}

void EmitVBaseVisitor::visit(AstFOpen* nodep) {
    putfs(nodep,nodep->verilogKwd());
    putbs(" (");
    if (nodep->filep()) nodep->filep()->iterateAndNext(*this);
    putbs(",");
    if (nodep->filenamep()) nodep->filenamep()->iterateAndNext(*this);
    putbs(",");
    if (nodep->modep()) nodep->modep()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstFClose* nodep) {
    putfs(nodep,nodep->verilogKwd());
    putbs(" (");
    if (nodep->filep()) nodep->filep()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstFFlush* nodep) {
    putfs(nodep,nodep->verilogKwd());
    putbs(" (");
    if (nodep->filep()) nodep->filep()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstJumpGo* nodep) {
    putbs("disable "+cvtToStr((void*)(nodep->labelp()))+";\n");
}

void EmitVBaseVisitor::visit(AstJumpLabel* nodep) {
    putbs("begin : "+cvtToStr((void*)(nodep))+"\n");
    if (nodep->stmtsp()) nodep->stmtsp()->iterateAndNext(*this);
    puts("end\n");
}

void EmitVBaseVisitor::visit(AstReadMem* nodep) {
    putfs(nodep,nodep->verilogKwd());
    putbs(" (");
    if (nodep->filenamep()) nodep->filenamep()->iterateAndNext(*this);
    putbs(",");
    if (nodep->memp()) nodep->memp()->iterateAndNext(*this);
    if (nodep->lsbp()) { putbs(","); nodep->lsbp()->iterateAndNext(*this); }
    if (nodep->msbp()) { putbs(","); nodep->msbp()->iterateAndNext(*this); }
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstSysIgnore* nodep) {
    putfs(nodep,nodep->verilogKwd());
    putbs(" (");
    nodep->exprsp()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstNodeFor* nodep) {
    putfs(nodep,"for (");
    m_suppressSemi = true;
    nodep->initsp()->iterateAndNext(*this);
    puts(";");
    nodep->condp()->iterateAndNext(*this);
    puts(";");
    nodep->incsp()->iterateAndNext(*this);
    m_suppressSemi = false;
    puts(") begin\n");
    nodep->bodysp()->iterateAndNext(*this);
    putqs(nodep,"end\n");
}

void EmitVBaseVisitor::visit(AstRepeat* nodep) {
    putfs(nodep,"repeat (");
    nodep->countp()->iterateAndNext(*this);
    puts(") begin\n");
    nodep->bodysp()->iterateAndNext(*this);
    putfs(nodep,"end\n");
}

void EmitVBaseVisitor::visit(AstWhile* nodep) {
    nodep->precondsp()->iterateAndNext(*this);
    putfs(nodep,"while (");
    nodep->condp()->iterateAndNext(*this);
    puts(") begin\n");
    nodep->bodysp()->iterateAndNext(*this);
    nodep->incsp()->iterateAndNext(*this);
    nodep->precondsp()->iterateAndNext(*this);  // Need to recompute before next loop
    putfs(nodep,"end\n");
}

void EmitVBaseVisitor::visit(AstNodeIf* nodep) {
    putfs(nodep,"");
    if (AstIf* ifp = nodep->castIf()) {
	if (ifp->priorityPragma()) puts("priority ");
	if (ifp->uniquePragma()) puts("unique ");
	if (ifp->unique0Pragma()) puts("unique0 ");
    }
    puts("if (");
    nodep->condp()->iterateAndNext(*this);
    puts(") begin\n");
    nodep->ifsp()->iterateAndNext(*this);
    if (nodep->elsesp()) {
	putqs(nodep,"end\n");
	putqs(nodep,"else begin\n");
	nodep->elsesp()->iterateAndNext(*this);
    }
    putqs(nodep,"end\n");
}

void EmitVBaseVisitor::visit(AstReturn* nodep) {
    putfs(nodep,"return ");
    nodep->lhsp()->iterateAndNext(*this);
    puts(";\n");
}

void EmitVBaseVisitor::visit(AstStop* nodep) {
    putfs(nodep,"$stop;\n");
}

void EmitVBaseVisitor::visit(AstFinish* nodep) {
    putfs(nodep,"$finish;\n");
}

void EmitVBaseVisitor::visit(AstText* nodep) {
    putsNoTracking(nodep->text());
}

void EmitVBaseVisitor::visit(AstScopeName* nodep) {
}

void EmitVBaseVisitor::visit(AstCStmt* nodep) {
    putfs(nodep,"$_CSTMT(");
    nodep->bodysp()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstCMath* nodep) {
    putfs(nodep,"$_CMATH(");
    nodep->bodysp()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstUCStmt* nodep) {
    putfs(nodep,"$c(");
    nodep->bodysp()->iterateAndNext(*this);
    puts(");\n");
}

void EmitVBaseVisitor::visit(AstUCFunc* nodep) {
    putfs(nodep,"$c(");
    nodep->bodysp()->iterateAndNext(*this);
    puts(")");
}

// Operators
void EmitVBaseVisitor::emitVerilogFormat(AstNode* nodep, const string& format,
					 AstNode* lhsp, AstNode* rhsp, AstNode* thsp) {
    // Look at emitVerilog() format for term/uni/dual/triops,
    // and write out appropriate text.
    //	%f	Potential fileline-if-change and line break
    //	%l	lhsp - if appropriate
    //	%r	rhsp - if appropriate
    //	%t	thsp - if appropriate
    //	%d	dtypep - if appropriate
    //	%k	Potential line break
    bool inPct = false;
    putbs("");
    for (string::const_iterator pos = format.begin(); pos != format.end(); ++pos) {
	if (pos[0]=='%') {
	    inPct = true;
	} else if (!inPct) {   // Normal text
	    string s; s+=pos[0]; puts(s);
	} else { // Format character
	    inPct = false;
	    switch (*pos) {
	    case '%': puts("%");  break;
	    case 'f': putfs(nodep,"");  break;
	    case 'k': putbs("");  break;
	    case 'l': {
		if (!lhsp) { nodep->v3fatalSrc("emitVerilog() references undef node"); }
		else lhsp->iterateAndNext(*this);
		break;
	    }
	    case 'r': {
		if (!rhsp) { nodep->v3fatalSrc("emitVerilog() references undef node"); }
		else rhsp->iterateAndNext(*this);
		break;
	    }
	    case 't': {
		if (!thsp) { nodep->v3fatalSrc("emitVerilog() references undef node"); }
		else thsp->iterateAndNext(*this);
		break;
	    }
	    case 'd': {
		if (!nodep->dtypep()) { nodep->v3fatalSrc("emitVerilog() references undef node"); }
		else nodep->dtypep()->iterateAndNext(*this);
		break;
	    }
	    default:
		nodep->v3fatalSrc("Unknown emitVerilog format code: %"<<pos[0]);
		break;
	    }
	}
    }
}

void EmitVBaseVisitor::visit(AstNodeTermop* nodep) {
    emitVerilogFormat(nodep, nodep->emitVerilog());
}

void EmitVBaseVisitor::visit(AstNodeUniop* nodep) {
    emitVerilogFormat(nodep, nodep->emitVerilog(), nodep->lhsp());
}

void EmitVBaseVisitor::visit(AstNodeBiop* nodep) {
    emitVerilogFormat(nodep, nodep->emitVerilog(), nodep->lhsp(), nodep->rhsp());
}

void EmitVBaseVisitor::visit(AstNodeTriop* nodep) {
    emitVerilogFormat(nodep, nodep->emitVerilog(), nodep->lhsp(), nodep->rhsp(), nodep->thsp());
}

void EmitVBaseVisitor::visit(AstAttrOf* nodep) {
    putfs(nodep,"$_ATTROF(");
    nodep->fromp()->iterateAndNext(*this);
    if (nodep->dimp()) {
	putbs(",");
	nodep->dimp()->iterateAndNext(*this);
    }
    puts(")");
}

void EmitVBaseVisitor::visit(AstInitArray* nodep) {
    putfs(nodep,"`{");
    int pos = 0;
    for (AstNode* itemp = nodep->initsp(); itemp; ++pos, itemp=itemp->nextp()) {
	int index = nodep->posIndex(pos);
	puts(cvtToStr(index));
	puts(":");
	itemp->accept(*this);
	if (itemp->nextp()) putbs(",");
    }
    puts("}");
}

void EmitVBaseVisitor::visit(AstNodeCond* nodep) {
    putbs("(");
    nodep->condp()->iterateAndNext(*this); putfs(nodep," ? ");
    nodep->expr1p()->iterateAndNext(*this); putbs(" : ");
    nodep->expr2p()->iterateAndNext(*this); puts(")");
}

void EmitVBaseVisitor::visit(AstRange* nodep) {
    puts("[");
    if (nodep->msbp()->castConst() && nodep->lsbp()->castConst()) {
	// Looks nicer if we print [1:0] rather than [32'sh1:32sh0]
	puts(cvtToStr(nodep->leftp()->castConst()->toSInt())); puts(":");
	puts(cvtToStr(nodep->rightp()->castConst()->toSInt())); puts("]");
    } else {
	nodep->leftp()->iterateAndNext(*this); puts(":");
	nodep->rightp()->iterateAndNext(*this); puts("]");
    }
}

void EmitVBaseVisitor::visit(AstSel* nodep) {
    nodep->fromp()->iterateAndNext(*this); puts("[");
    if (nodep->lsbp()->castConst()) {
	if (nodep->widthp()->isOne()) {
	    if (nodep->lsbp()->castConst()) {
		puts(cvtToStr(nodep->lsbp()->castConst()->toSInt()));
	    } else {
		nodep->lsbp()->iterateAndNext(*this);
	    }
	} else {
	    puts(cvtToStr(nodep->lsbp()->castConst()->toSInt()
			  +nodep->widthp()->castConst()->toSInt()
			  -1));
	    puts(":");
	    puts(cvtToStr(nodep->lsbp()->castConst()->toSInt()));
	}
    } else {
	nodep->lsbp()->iterateAndNext(*this); putfs(nodep,"+:");
	nodep->widthp()->iterateAndNext(*this); puts("]");
    }
    puts("]");
}

void EmitVBaseVisitor::visit(AstTypedef* nodep) {
    putfs(nodep,"typedef ");
    nodep->dtypep()->iterateAndNext(*this); puts(" ");
    puts(nodep->prettyName());
    puts(";\n");
}

void EmitVBaseVisitor::visit(AstBasicDType* nodep) {
    if (nodep->isSigned()) putfs(nodep,"signed ");
    putfs(nodep,nodep->prettyName());
    if (nodep->rangep()) { puts(" "); nodep->rangep()->iterateAndNext(*this); puts(" "); }
    else if (nodep->isRanged()) { puts(" ["); puts(cvtToStr(nodep->msb())); puts(":0] "); }
}

void EmitVBaseVisitor::visit(AstConstDType* nodep) {
    putfs(nodep,"const ");
    nodep->subDTypep()->accept(*this);
}

void EmitVBaseVisitor::visit(AstNodeArrayDType* nodep) {
    nodep->subDTypep()->accept(*this);
    nodep->rangep()->iterateAndNext(*this);
}

void EmitVBaseVisitor::visit(AstNodeClassDType* nodep) {
    puts(nodep->verilogKwd()+" ");
    if (nodep->packed()) puts("packed ");
    puts("\n");
    nodep->membersp()->iterateAndNext(*this);
    puts("}");
}

void EmitVBaseVisitor::visit(AstMemberDType* nodep) {
    nodep->subDTypep()->accept(*this);
    puts(" ");
    puts(nodep->name());
    puts("}");
}

void EmitVBaseVisitor::visit(AstNodeFTaskRef* nodep) {
    if (nodep->dotted()!="") { putfs(nodep,nodep->dotted()); puts("."); puts(nodep->prettyName()); }
    else { putfs(nodep,nodep->prettyName()); }
    puts("(");
    nodep->pinsp()->iterateAndNext(*this);
    puts(")");
}

void EmitVBaseVisitor::visit(AstArg* nodep) {
    nodep->exprp()->iterateAndNext(*this);
}

// Terminals
void EmitVBaseVisitor::visit(AstVarRef* nodep) {
    if (nodep->varScopep())
	putfs(nodep,nodep->varScopep()->prettyName());
    else {
	putfs(nodep,nodep->hiername());
	puts(nodep->varp()->prettyName());
    }
}

void EmitVBaseVisitor::visit(AstVarXRef* nodep) {
    putfs(nodep,nodep->dotted());
    puts(".");
    puts(nodep->varp()->prettyName());
}

void EmitVBaseVisitor::visit(AstConst* nodep) {
    putfs(nodep,nodep->num().ascii(true,true));
}

// Just iterate
void EmitVBaseVisitor::visit(AstTopScope* nodep) {
    nodep->iterateChildren(*this);
}

void EmitVBaseVisitor::visit(AstScope* nodep) {
    nodep->iterateChildren(*this);
}

void EmitVBaseVisitor::visit(AstVar* nodep) {
    putfs(nodep,nodep->verilogKwd());
    puts(" ");
    nodep->dtypep()->iterate(*this); puts(" ");
    puts(nodep->prettyName());
    puts(";\n");
}

void EmitVBaseVisitor::visit(AstActive* nodep) {
    m_sensesp = nodep->sensesp();
    nodep->stmtsp()->iterateAndNext(*this);
    m_sensesp = NULL;
}

void EmitVBaseVisitor::visit(AstVarScope*) {
}

void EmitVBaseVisitor::visit(AstNodeText*) {
}

void EmitVBaseVisitor::visit(AstTraceDecl*) {
}

void EmitVBaseVisitor::visit(AstTraceInc*) {
}

// NOPs
void EmitVBaseVisitor::visit(AstPragma*) {
}

void EmitVBaseVisitor::visit(AstCell*) {		// Handled outside the Visit class
}

// Default
void EmitVBaseVisitor::visit(AstNode* nodep) {
    puts((string)"\n???? // "+nodep->prettyTypeName()+"\n");
    nodep->iterateChildren(*this);
    // Not v3fatalSrc so we keep processing
    nodep->v3error("Internal: Unknown node type reached emitter: "<<nodep->prettyTypeName());
}

EmitVBaseVisitor::EmitVBaseVisitor(AstSenTree* domainp) {   // Domain for printing one a ALWAYS under a ACTIVE
    m_suppressSemi = false;
    m_sensesp = domainp;
}

EmitVBaseVisitor::~EmitVBaseVisitor() {
}

//######################################################################
// Emit to an output file

class EmitVFileVisitor : public EmitVBaseVisitor {
    // MEMBERS
    V3OutFile*	m_ofp;
    // METHODS
    V3OutFile*	ofp() const { return m_ofp; }
    virtual void puts(const string& str) { ofp()->puts(str); }
    virtual void putbs(const string& str) { ofp()->putbs(str); }
    virtual void putfs(AstNode*, const string& str) { putbs(str); }
    virtual void putqs(AstNode*, const string& str) { putbs(str); }
    virtual void putsNoTracking(const string& str) { ofp()->putsNoTracking(str); }
public:
    EmitVFileVisitor(AstNode* nodep, V3OutFile* ofp) {
	m_ofp = ofp;
	nodep->accept(*this);
    }
    virtual ~EmitVFileVisitor() {}
};

//######################################################################
// Emit to a stream (perhaps stringstream)

class EmitVStreamVisitor : public EmitVBaseVisitor {
    // MEMBERS
    ostream&	m_os;
    // METHODS
    virtual void putsNoTracking(const string& str) { m_os<<str; }
    virtual void puts(const string& str) { putsNoTracking(str); }
    virtual void putbs(const string& str) { puts(str); }
    virtual void putfs(AstNode*, const string& str) { putbs(str); }
    virtual void putqs(AstNode*, const string& str) { putbs(str); }
public:
    EmitVStreamVisitor(AstNode* nodep, ostream& os)
	: m_os(os) {
	nodep->accept(*this);
    }
    virtual ~EmitVStreamVisitor() {}
};

//######################################################################
// Emit to a stream (perhaps stringstream)

class EmitVPrefixedFormatter : public V3OutFormatter {
    ostream&	m_os;
    string	m_prefix;	// What to print at beginning of each line
    int		m_flWidth;	// Padding of fileline
    int		m_column;	// Rough location; need just zero or non-zero
    FileLine*	m_prefixFl;
    // METHODS
    virtual void putcOutput(char chr) {
	if (chr == '\n') {
	    m_column = 0;
	    m_os<<chr;
	} else {
	    if (m_column == 0) {
		m_column = 10;
		m_os<<m_prefixFl->ascii()+":";
		m_os<<V3OutFile::indentSpaces(m_flWidth-(m_prefixFl->ascii().length()+1));
		m_os<<" ";
		m_os<<m_prefix;
	    }
	    m_column++;
	    m_os<<chr;
	}
    }
public:
    void prefixFl(FileLine* fl) { m_prefixFl = fl; }
    FileLine* prefixFl() const { return m_prefixFl; }
    int column() const { return m_column; }
    EmitVPrefixedFormatter(ostream& os, const string& prefix, int flWidth)
	: V3OutFormatter("__STREAM", V3OutFormatter::LA_VERILOG)
	, m_os(os), m_prefix(prefix), m_flWidth(flWidth) {
	m_column = 0;
	m_prefixFl = v3Global.rootp()->fileline();  // NETLIST's fileline instead of NULL to avoid NULL checks
    }
    virtual ~EmitVPrefixedFormatter() {
	if (m_column) puts("\n");
    }
};

class EmitVPrefixedVisitor : public EmitVBaseVisitor {
    // MEMBERS
    EmitVPrefixedFormatter m_formatter; // Special verilog formatter (Way down the inheritance is another unused V3OutFormatter)
    // METHODS
    virtual void putsNoTracking(const string& str) { m_formatter.putsNoTracking(str); }
    virtual void puts(const string& str) { m_formatter.puts(str); }
    // We don't use m_formatter's putbs because the tokens will change filelines
    // and insert returns at the proper locations
    virtual void putbs(const string& str) { m_formatter.puts(str); }
    virtual void putfs(AstNode* nodep, const string& str) { putfsqs(nodep,str,false); }
    virtual void putqs(AstNode* nodep, const string& str) { putfsqs(nodep,str,true); }
    void putfsqs(AstNode* nodep, const string& str, bool quiet) {
	if (m_formatter.prefixFl() != nodep->fileline()) {
	    m_formatter.prefixFl(nodep->fileline());
	    if (m_formatter.column()) puts("\n");  // This in turn will print the m_prefixFl
	}
	if (!quiet && nodep->user3()) puts("%%");
	putbs(str);
    }

public:
    EmitVPrefixedVisitor(AstNode* nodep, ostream& os, const string& prefix, int flWidth,
			 AstSenTree* domainp, bool user3mark)
	: EmitVBaseVisitor(domainp), m_formatter(os, prefix, flWidth) {
	if (user3mark) { AstUser3InUse::check(); }
	nodep->accept(*this);
    }
    virtual ~EmitVPrefixedVisitor() {}
};

//######################################################################
// EmitV class functions

void V3EmitV::emitv() {
    UINFO(2,__FUNCTION__<<": "<<endl);
    if (1) {
	// All-in-one file
	V3OutVFile of (v3Global.opt.makeDir()+"/"+v3Global.opt.prefix()+"__Vout.v");
	of.putsHeader();
	of.puts("# DESCR" "IPTION: Verilator output: Verilog representation of internal tree for debug\n");
	EmitVFileVisitor visitor (v3Global.rootp(), &of);
    } else {
	// Process each module in turn
	for (AstNodeModule* modp = v3Global.rootp()->modulesp(); modp; modp=modp->nextp()->castNodeModule()) {
	    V3OutVFile of (v3Global.opt.makeDir()
			   +"/"+EmitCBaseVisitor::modClassName(modp)+"__Vout.v");
	    of.putsHeader();
	    EmitVFileVisitor visitor (modp, &of);
	}
    }
}

void V3EmitV::verilogForTree(AstNode* nodep, ostream& os) {
    EmitVStreamVisitor(nodep, os);
}

void V3EmitV::verilogPrefixedTree(AstNode* nodep, ostream& os, const string& prefix, int flWidth,
				  AstSenTree* domainp, bool user3mark) {
    EmitVPrefixedVisitor(nodep, os, prefix, flWidth, domainp, user3mark);
}
