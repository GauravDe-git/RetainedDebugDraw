#include "RDDShapes.h"

namespace RDDShapes
{
	void AppendBox(TArray<FRDDLine>& Out, const FVector& Center,
	               const FVector& Extent, const FColor& Color)
	{
		const FVector& B = Extent;
		auto Add = [&](const FVector& From, const FVector& To)
		{
			Out.Emplace(Center + From, Center + To, Color);
		};

		// Same 12 lines, in the same order, as ULineBatchComponent::DrawBox.
		// Top face
		Add(FVector( B.X,  B.Y,  B.Z), FVector( B.X, -B.Y,  B.Z));
		Add(FVector( B.X, -B.Y,  B.Z), FVector(-B.X, -B.Y,  B.Z));
		Add(FVector(-B.X, -B.Y,  B.Z), FVector(-B.X,  B.Y,  B.Z));
		Add(FVector(-B.X,  B.Y,  B.Z), FVector( B.X,  B.Y,  B.Z));
		// Bottom face
		Add(FVector( B.X,  B.Y, -B.Z), FVector( B.X, -B.Y, -B.Z));
		Add(FVector( B.X, -B.Y, -B.Z), FVector(-B.X, -B.Y, -B.Z));
		Add(FVector(-B.X, -B.Y, -B.Z), FVector(-B.X,  B.Y, -B.Z));
		Add(FVector(-B.X,  B.Y, -B.Z), FVector( B.X,  B.Y, -B.Z));
		// Verticals
		Add(FVector( B.X,  B.Y,  B.Z), FVector( B.X,  B.Y, -B.Z));
		Add(FVector( B.X, -B.Y,  B.Z), FVector( B.X, -B.Y, -B.Z));
		Add(FVector(-B.X, -B.Y,  B.Z), FVector(-B.X, -B.Y, -B.Z));
		Add(FVector(-B.X,  B.Y,  B.Z), FVector(-B.X,  B.Y, -B.Z));
	}

	void AppendSphere(TArray<FRDDLine>& Out, const FVector& Center,
	                  float Radius, int32 Segments, const FColor& Color)
	{
		// Mirrors ULineBatchComponent::DrawSphere: same maths, same float precision,
		// same line order. Each step emits one line along the latitude ring and one
		// down to the next ring.
		Segments = FMath::Max(Segments, 4);
		Out.Reserve(Out.Num() + 2 * Segments * Segments);

		const float AngleInc = 2.f * UE_PI / Segments;
		int32 NumSegmentsY = Segments;
		float Latitude = AngleInc;
		float SinY1 = 0.0f, CosY1 = 1.0f;

		while (NumSegmentsY--)
		{
			const float SinY2 = FMath::Sin(Latitude);
			const float CosY2 = FMath::Cos(Latitude);

			FVector Vertex1 = FVector(SinY1, 0.0f, CosY1) * Radius + Center;
			FVector Vertex3 = FVector(SinY2, 0.0f, CosY2) * Radius + Center;
			float Longitude = AngleInc;

			int32 NumSegmentsX = Segments;
			while (NumSegmentsX--)
			{
				const float SinX = FMath::Sin(Longitude);
				const float CosX = FMath::Cos(Longitude);

				const FVector Vertex2 = FVector(CosX * SinY1, SinX * SinY1, CosY1) * Radius + Center;
				const FVector Vertex4 = FVector(CosX * SinY2, SinX * SinY2, CosY2) * Radius + Center;

				Out.Emplace(Vertex1, Vertex2, Color);
				Out.Emplace(Vertex1, Vertex3, Color);

				Vertex1 = Vertex2;
				Vertex3 = Vertex4;
				Longitude += AngleInc;
			}
			SinY1 = SinY2;
			CosY1 = CosY2;
			Latitude += AngleInc;
		}
	}
}