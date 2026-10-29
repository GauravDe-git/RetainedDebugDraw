#include "RDDComponent.h"

#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"

namespace
{
	struct FRDDTestReport
	{
		int32 Passed = 0;
		int32 Failed = 0;

		void Expect(bool bCondition, const TCHAR* Name)
		{
			if (bCondition)
			{
				++Passed;
				UE_LOG(LogTemp, Display, TEXT("RDD SelfTest  PASS  %s"), Name);
			}
			else
			{
				++Failed;
				UE_LOG(LogTemp, Error, TEXT("RDD SelfTest  FAIL  %s"), Name);
			}
		}
	};

	bool SameLine(const FRDDLine& A, const FRDDLine& B)
	{
		return A.Start == B.Start && A.End == B.End && A.Color == B.Color;
	}

	void RunHandleSelfTest()
	{
		// A standalone component that's never registered, so nothing renders.
		// This tests the handle bookkeeping on its own.
		URDDComponent* Comp = NewObject<URDDComponent>(GetTransientPackage());
		FRDDTestReport R;
		FRDDLine Out;

		const FRDDLine LA(FVector(0, 0, 0),    FVector(100, 0, 0),  FColor::Red);
		const FRDDLine LB(FVector(0, 0, 0),    FVector(0, 100, 0),  FColor::Green);
		const FRDDLine LC(FVector(0, 0, 0),    FVector(0, 0, 100),  FColor::Blue);
		const FRDDLine LD(FVector(50, 50, 50), FVector(60, 60, 60), FColor::Yellow);

		// Submitting returns a handle
		const FRDDHandle A = Comp->SubmitLine(LA);
		const FRDDHandle B = Comp->SubmitLine(LB);
		const FRDDHandle C = Comp->SubmitLine(LC);
		R.Expect(Comp->IsValidHandle(A) && Comp->GetLine(A, Out) && SameLine(Out, LA),
		         TEXT("submit returns a valid handle to the same line"));

		// Releasing removes exactly that line
		R.Expect(Comp->ReleaseLine(B),       TEXT("releasing a live handle succeeds"));
		R.Expect(!Comp->IsValidHandle(B),    TEXT("a released handle is no longer valid"));
		R.Expect(Comp->GetLine(A, Out) && SameLine(Out, LA) &&
		         Comp->GetLine(C, Out) && SameLine(Out, LC),
		         TEXT("other lines are untouched by a release"));
		R.Expect(Comp->GetNumLines() == 2,   TEXT("live line count drops by one"));

		// A stale handle can't touch its slot's new occupant
		const FRDDHandle D = Comp->SubmitLine(LD);
		R.Expect(D.Index == B.Index,         TEXT("a new line reuses the released slot"));
		R.Expect(!Comp->IsValidHandle(B),    TEXT("the old handle stays invalid after its slot is reused"));
		R.Expect(!Comp->ReleaseLine(B),      TEXT("releasing through the old handle is rejected"));
		R.Expect(Comp->GetLine(D, Out) && SameLine(Out, LD),
		         TEXT("the new occupant survives the stale release"));

		// Double release and unset handles
		R.Expect(!Comp->ReleaseLine(B),                 TEXT("a double release is rejected"));
		R.Expect(!Comp->IsValidHandle(FRDDHandle()),    TEXT("an unset handle is never valid"));

		// Clear invalidates everything, even once slots are reused
		Comp->ClearLines();
		R.Expect(!Comp->IsValidHandle(A) && !Comp->IsValidHandle(C) && !Comp->IsValidHandle(D),
		         TEXT("clear invalidates every handle"));

		const FRDDHandle E = Comp->SubmitLine(LA);
		R.Expect(E.Index == A.Index && !Comp->IsValidHandle(A) && Comp->IsValidHandle(E),
		         TEXT("handles from before a clear stay dead when their slots are reused"));

		UE_LOG(LogTemp, Display, TEXT("RDD SelfTest  %d passed, %d failed"), R.Passed, R.Failed);
	}
}

static FAutoConsoleCommand GRDDSelfTestHandles(
	TEXT("RDD.SelfTest.Handles"),
	TEXT("Run the handle bookkeeping self-test and log PASS or FAIL for each case."),
	FConsoleCommandDelegate::CreateStatic(&RunHandleSelfTest));