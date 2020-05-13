
#include "jitintern.h"

void JitCompiler::EmitSB()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(Trunc8(LoadD(B)), ToInt8Ptr(LoadA(A), ConstD(C)));
}

void JitCompiler::EmitSB_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(Trunc8(LoadD(B)), ToInt8Ptr(LoadA(A), LoadD(C)));
}

void JitCompiler::EmitSH()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(Trunc16(LoadD(B)), ToInt16Ptr(LoadA(A), ConstD(C)));
}

void JitCompiler::EmitSH_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(Trunc16(LoadD(B)), ToInt16Ptr(LoadA(A), LoadD(C)));
}

void JitCompiler::EmitSW()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(LoadD(B), ToInt32Ptr(LoadA(A), ConstD(C)));
}

void JitCompiler::EmitSW_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(LoadD(B), ToInt32Ptr(LoadA(A), LoadD(C)));
}

void JitCompiler::EmitSSP()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(FPTrunc(LoadF(B)), ToFloatPtr(LoadA(A), ConstD(C)));
}

void JitCompiler::EmitSSP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(FPTrunc(LoadF(B)), ToFloatPtr(LoadA(A), LoadD(C)));
}

void JitCompiler::EmitSDP()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(LoadF(B), ToDoublePtr(LoadA(A), ConstD(C)));
}

void JitCompiler::EmitSDP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	Store(LoadF(B), ToDoublePtr(LoadA(A), LoadD(C)));
}

void JitCompiler::EmitSS()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.CreateCall(GetNativeFunc<void, FString*, FString*>("__CallAssignString", &JitCompiler::CallAssignString), { OffsetPtr(LoadA(A), ConstD(C)), LoadS(B) });
}

void JitCompiler::EmitSS_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.CreateCall(GetNativeFunc<void, FString*, FString*>("__CallAssignString", &JitCompiler::CallAssignString), { OffsetPtr(LoadA(A), LoadD(C)), LoadS(B) });
}

void JitCompiler::EmitSO()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	typedef void(*FuncPtr)(DObject*);
	cc.CreateCall(GetNativeFunc<void, DObject*>("__WriteBarrier", static_cast<FuncPtr>(GC::WriteBarrier)), { OffsetPtr(LoadA(A), ConstD(C)), LoadA(B) });
}

void JitCompiler::EmitSO_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	typedef void(*FuncPtr)(DObject*);
	cc.CreateCall(GetNativeFunc<void, DObject*>("__WriteBarrier", static_cast<FuncPtr>(GC::WriteBarrier)), { OffsetPtr(LoadA(A), LoadD(C)), LoadA(B) });
}

void JitCompiler::EmitSP()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.CreateStore(LoadA(B), OffsetPtr(LoadA(A), ConstD(C)));
}

void JitCompiler::EmitSP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.CreateStore(LoadA(B), OffsetPtr(LoadA(A), LoadD(C)));
}

void JitCompiler::EmitSV2()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	IRValue* base = ToDoublePtr(LoadA(A), ConstD(C));
	Store(LoadF(B), base);
	Store(LoadF(B + 1), OffsetPtr(base, 1));
}

void JitCompiler::EmitSV2_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	IRValue* base = ToDoublePtr(LoadA(A), LoadD(C));
	Store(LoadF(B), base);
	Store(LoadF(B + 1), OffsetPtr(base, 1));
}

void JitCompiler::EmitSV3()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	IRValue* base = ToDoublePtr(LoadA(A), ConstD(C));
	Store(LoadF(B), base);
	Store(LoadF(B + 1), OffsetPtr(base, 1));
	Store(LoadF(B + 2), OffsetPtr(base, 2));
}

void JitCompiler::EmitSV3_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	IRValue* base = ToDoublePtr(LoadA(A), LoadD(C));
	Store(LoadF(B), base);
	Store(LoadF(B + 1), OffsetPtr(base, 1));
	Store(LoadF(B + 2), OffsetPtr(base, 2));
}

void JitCompiler::EmitSBIT()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);

	IRValue* ptr = LoadA(A);
	IRValue* value = cc.CreateLoad(ptr);
	
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	cc.CreateCondBr(cc.CreateICmpNE(LoadD(B), ConstValueD(0)), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(cc.CreateOr(value, ircontext->getConstantInt((int)C)), ptr);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(cc.CreateAnd(value, ircontext->getConstantInt(~(int)C)), ptr);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}
