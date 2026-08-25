// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/RouteClientTravelTester.h"
#include "Framework/RouteGameInstance.h"

// Sets default values
ARouteClientTravelTester::ARouteClientTravelTester()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ARouteClientTravelTester::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Error, TEXT("RouteClientTravelTester BeginPlay"));

	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Error, TEXT("Dedicated Server. Skip RouteClientTravelTester."));
		return;
	}

	FTimerHandle TimerHandle;

	GetWorldTimerManager().SetTimer(
		TimerHandle,
		this,
		&ARouteClientTravelTester::RequestTravelToTestServer,
		2.0f,
		false
	);

}

// Called every frame
void ARouteClientTravelTester::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARouteClientTravelTester::RequestTravelToTestServer()
{
	URouteGameInstance* RouteGameInstance = Cast<URouteGameInstance>(GetGameInstance());

	if (!RouteGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("RouteGameInstance is null. ClientTravel canceled"));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("RouteClientTravelTester calls TravelToTestServer"));

	RouteGameInstance->TravelToTestServer();

}

