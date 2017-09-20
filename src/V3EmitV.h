// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Emit Verilog code for module tree
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
//
//*************************************************************************

#ifndef _V3EMITV_H_
#define _V3EMITV_H_ 1
#include "config_build.h"
#include "verilatedos.h"
#include "V3Error.h"
#include "V3Ast.h"
#include "V3EmitCBase.h"

//============================================================================

class EmitVBaseVisitor : public EmitCBaseVisitor {
protected:
    // MEMBERS
    bool	m_suppressSemi;
    AstSenTree*	m_sensesp;

    // METHODS
    static int debug();
    virtual void puts(const string& str) = 0;
    virtual void putbs(const string& str) = 0;
    virtual void putfs(AstNode* nodep, const string& str) = 0;  // Fileline and node %% mark
    virtual void putqs(AstNode* nodep, const string& str) = 0;  // Fileline quiet w/o %% mark
    virtual void putsNoTracking(const string& str) = 0;
    virtual void putsQuoted(const string& str);

    // VISITORS
    virtual void visit(AstNetlist* nodep);
    virtual void visit(AstNodeModule* nodep);
    virtual void visit(AstNodeFTask* nodep);

    virtual void visit(AstBegin* nodep);
    virtual void visit(AstGenerate* nodep);
    virtual void visit(AstFinal* nodep);
    virtual void visit(AstInitial* nodep);
    virtual void visit(AstAlways* nodep);
    virtual void visit(AstAlwaysPublic* nodep);
    virtual void visit(AstNodeAssign* nodep);
    virtual void visit(AstAssignDly* nodep);
    virtual void visit(AstAssignAlias* nodep);
    virtual void visit(AstAssignW* nodep);
    virtual void visit(AstBreak* nodep);
    virtual void visit(AstSenTree* nodep);
    virtual void visit(AstSenGate* nodep);
    virtual void visit(AstSenItem* nodep);
    virtual void visit(AstNodeCase* nodep);
    virtual void visit(AstCaseItem* nodep);
    virtual void visit(AstComment* nodep);
    virtual void visit(AstContinue* nodep) ;
    virtual void visit(AstCoverDecl*);
    virtual void visit(AstCoverInc*);
    virtual void visit(AstCoverToggle*);

    void visitNodeDisplay(AstNode* nodep, AstNode* fileOrStrgp, const string& text, AstNode* exprsp);
    virtual void visit(AstDisable* nodep);
    virtual void visit(AstDisplay* nodep);
    virtual void visit(AstFScanF* nodep);
    virtual void visit(AstSScanF* nodep);
    virtual void visit(AstSFormat* nodep);
    virtual void visit(AstSFormatF* nodep);
    virtual void visit(AstFOpen* nodep);
    virtual void visit(AstFClose* nodep);
    virtual void visit(AstFFlush* nodep);
    virtual void visit(AstJumpGo* nodep);
    virtual void visit(AstJumpLabel* nodep);
    virtual void visit(AstReadMem* nodep);
    virtual void visit(AstSysIgnore* nodep);
    virtual void visit(AstNodeFor* nodep);
    virtual void visit(AstRepeat* nodep);
    virtual void visit(AstWhile* nodep);
    virtual void visit(AstNodeIf* nodep);
    virtual void visit(AstReturn* nodep);
    virtual void visit(AstStop* nodep);
    virtual void visit(AstFinish* nodep);
    virtual void visit(AstText* nodep);
    virtual void visit(AstScopeName* nodep);
    virtual void visit(AstCStmt* nodep);
    virtual void visit(AstCMath* nodep);
    virtual void visit(AstUCStmt* nodep);
    virtual void visit(AstUCFunc* nodep);

    virtual void emitVerilogFormat(AstNode* nodep, const string& format,
				   AstNode* lhsp=NULL, AstNode* rhsp=NULL, AstNode* thsp=NULL);

    virtual void visit(AstNodeTermop* nodep);
    virtual void visit(AstNodeUniop* nodep);
    virtual void visit(AstNodeBiop* nodep);
    virtual void visit(AstNodeTriop* nodep);
    virtual void visit(AstAttrOf* nodep);
    virtual void visit(AstInitArray* nodep);
    virtual void visit(AstNodeCond* nodep);
    virtual void visit(AstRange* nodep);
    virtual void visit(AstSel* nodep);
    virtual void visit(AstTypedef* nodep);
    virtual void visit(AstBasicDType* nodep);
    virtual void visit(AstConstDType* nodep);
    virtual void visit(AstNodeArrayDType* nodep);
    virtual void visit(AstNodeClassDType* nodep);
    virtual void visit(AstMemberDType* nodep);
    virtual void visit(AstNodeFTaskRef* nodep);
    virtual void visit(AstArg* nodep);
    virtual void visit(AstVarRef* nodep);
    virtual void visit(AstVarXRef* nodep);
    virtual void visit(AstConst* nodep);
    virtual void visit(AstTopScope* nodep);
    virtual void visit(AstScope* nodep);
    virtual void visit(AstVar* nodep);
    virtual void visit(AstActive* nodep);
    virtual void visit(AstVarScope*);
    virtual void visit(AstNodeText*);
    virtual void visit(AstTraceDecl*);
    virtual void visit(AstTraceInc*);
    virtual void visit(AstPragma*);
    virtual void visit(AstCell*);
    virtual void visit(AstNode* nodep);

public:
    explicit EmitVBaseVisitor(AstSenTree* domainp=NULL);
    virtual ~EmitVBaseVisitor();
};


class V3EmitV {
public:
    static void emitv();
    static void verilogForTree(AstNode* nodep, ostream& os=cout);
    static void verilogPrefixedTree(AstNode* nodep, ostream& os, const string& prefix, int flWidth,
				    AstSenTree* domainp, bool user3percent);
};

#endif // Guard
