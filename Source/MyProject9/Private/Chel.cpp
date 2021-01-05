// Fill out your copyright notice in the Description page of Project Settings.


#include "Chel.h"

#include "Components/InputComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine.h"
#include "Engine/EngineTypes.h"
#include "Blueprint/UserWidget.h"
#include "Components/SkinnedMeshComponent.h"
#include "AreaCollision.h"
#include "BP_PlayerController.h"
#include "Cache_Key.h"
#include "CanalizationDamageCollision.h"
#include "Code_Note.h"

enum AreaType
{
	Canalizacia,
	Shelter,
	Ventilacia,
	ShelterOpener
};

enum PickUpType {
	Boltorez,
	KeyShelter,
	Otvertka,
	OpenArea,
	Cache,
	CacheKey,
	GeneratorArea,
	CanalizationButton,
	WebCamLocker,
	InvisiblePotion,
	CodeNote,
	ClickButton,
};

enum CacheType {
	KeyBronze,
	KeySilver,
	KeyGold
};

// Sets default values
AChel::AChel()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PoseableMeshComp = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("PoseableMeshComp"));
	PoseableMeshComp->SetupAttachment(RootComponent);

	Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
	Scene->SetupAttachment(PoseableMeshComp, TEXT("Bone_002_end"));

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(Scene);

	DamageCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("DamageCollision"));
	DamageCollision->SetupAttachment(RootComponent);

	Stone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stone"));
	Stone->SetupAttachment(CameraComp);

	TimeLine_Stone_First = CreateDefaultSubobject<UTimelineComponent>(TEXT("ThrowStoneFirst"));
	TimeLine_Stone_Second = CreateDefaultSubobject<UTimelineComponent>(TEXT("ThrowStoneSecond"));
	TimeLine_FOV_WebCam = CreateDefaultSubobject<UTimelineComponent>(TEXT("FOV_WebCam"));

	//������� ���������, ����� ������� ��������� ��� Update � ��� OnFinished
	InterpFunction_Stone.BindUFunction(this, FName("TimelineVectorReturn_Stone"));
	InterpFunction_FOV_WebCam.BindUFunction(this, FName("TimelineFloatReturn_FOV_WebCam"));
	TimelineFinished_Stone_First.BindUFunction(this, FName("OnTimelineFinished_Stone_First"));
	TimelineFinished_Stone_Second.BindUFunction(this, FName("OnTimelineFinished_Stone_Second"));

	DamageCollision->OnComponentBeginOverlap.AddDynamic(this, &AChel::OnOverlapBegin);

	Health = 0;
	bIsAlreadyThrowing = false;
	IsInGame = false;
	Ammo = 15;
	bLineTrace_is_need_refresh = false;
	DoesHave_Owner = false;
	ItemCodePickUp = -1;
	IsNotInWebCam = true;
	CanThrowStone = true;
	WebCamIterator = -1;

	CanalizationDamage = 1.0f;

	KillerIndex = -1;
	IsSuccessOpening = false;
	AreaCode = -1;
	bCanPossessWebCam = false;
	bCanWalkingAndWatching = true;
	IsAwake = true;

	bInEscMenu = false;
//	OpenAreaObj = nullptr;

	KeysCount.Init(0, 3);
	TargetItemsStatic.Init(nullptr, 0);
	TargetArrowsStatic.Init(nullptr, 0);
	TargetArrowsDynamic.Init(nullptr, 0);
	TargetItemsDynamic.Init(nullptr, 0);
}

//SetupReplicationVariables----
void AChel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AChel, Index);
	DOREPLIFETIME(AChel, Health);
	DOREPLIFETIME(AChel, NickName);
	DOREPLIFETIME(AChel, Ammo);
	DOREPLIFETIME(AChel, Death);
	DOREPLIFETIME(AChel, Kills);
	DOREPLIFETIME(AChel, DoesHave);
	DOREPLIFETIME(AChel, KeysCount);
}
//-----------------------------

// Called when the game starts or when spawned
void AChel::BeginPlay()
{
	Super::BeginPlay();

	if (GetLocalRole() != ROLE_Authority)
		MyBeginPlay();
}

void AChel::MyBeginPlay()
{
	UE_LOG(LogTemp, Warning, TEXT("StartPlaying"));
	GS = Cast<AGS>(GetWorld()->GetGameState());

	IsServerAuth = GetLocalRole() == ROLE_Authority;
	World = GetWorld();
	IsPlayerOwner = UGameplayStatics::GetPlayerController(GetWorld(), 0) == GetController();
	StonePosition = Stone->GetRelativeLocation();

	GI = World->GetGameInstance<UGI>();

	if (vCurveStone) {
		//���������� ��� ������� ��� TimeLine
		TimeLine_Stone_First->AddInterpVector(vCurveStone, InterpFunction_Stone);
		TimeLine_Stone_First->SetTimelineFinishedFunc(TimelineFinished_Stone_First);

		TimeLine_Stone_First->SetLooping(false);
		TimeLine_Stone_First->SetIgnoreTimeDilation(true);

		TimeLine_Stone_Second->AddInterpVector(vCurveStone, InterpFunction_Stone);
		TimeLine_Stone_Second->SetTimelineFinishedFunc(TimelineFinished_Stone_Second);

		TimeLine_Stone_Second->SetLooping(false);
		TimeLine_Stone_Second->SetIgnoreTimeDilation(true);
	}

	if (vCurveFOV_WebCam)
	{
		TimeLine_FOV_WebCam->AddInterpFloat(vCurveFOV_WebCam, InterpFunction_FOV_WebCam);

		TimeLine_FOV_WebCam->SetLooping(false);
		TimeLine_FOV_WebCam->SetIgnoreTimeDilation(true);
	}

	if (IsPlayerOwner) {
		UserView = Cast<UUserView>(CreateWidget(World, UserViewClass));
		GeneratorView = Cast<UGeneratorWidget>(CreateWidget(World, GeneratorView_class));
		KillFeed = Cast<UKillFeed>(CreateWidget(World, KillFeed_class));
		Widget_Note = Cast<UNoteWidget>(CreateWidget(World, NoteWidget_class));
		WebCamUI = Cast<UWebCamWidget>(CreateWidget(World, WebCamWidget_class));
		WebCamUI->AddToViewport();
		UserView->AddToViewport();
		KillFeed->AddToViewport();
		GeneratorView->AddToViewport();
		GeneratorView->SetVisibility(ESlateVisibility::Hidden);
		Widget_Note->AddToViewport();
		Widget_Note->SetVisibility(ESlateVisibility::Hidden);
		KillFeed->AddToViewport();

		UserView->Player = this;
		UserView->AmmoLabel->SetText(FText::AsNumber((int32)Ammo));

		if (GI != nullptr) {
			Sensetivity = GI->Sensetivity;
			NickName = GI->NickName;
			DeliverNicknameToServer(NickName);
		}

		TArray<AActor*>MainExis;
		UGameplayStatics::GetAllActorsOfClass(World, AItemPromtArrow_MainExis::StaticClass(), MainExis);
		for (auto& it : MainExis)
		{
			AItemPromtArrow_MainExis* Exis = Cast<AItemPromtArrow_MainExis>(it);
			switch (Exis->Type)
			{
			case 0:
			{
				MainExis_Canalizacia = Cast<AItemPromtArrow_MainExis>(it);
				UE_LOG(LogTemp, Warning, TEXT("AddCanalizacia"));
				break;
			}
			case 1:
			{
				MainExis_Shelter = Cast<AItemPromtArrow_MainExis>(it);
				UE_LOG(LogTemp, Warning, TEXT("AddArrowShelter"));
				break;
			}
			case 2:
			{
				MainExis_Ventilacia = Cast<AItemPromtArrow_MainExis>(it);
				UE_LOG(LogTemp, Warning, TEXT("AddArrowVentilacia"));
				break;
			}
			}
		}
	}

	if (IsServerAuth) {
		Index = GS->AmountOfPlayers++;
		Cast<ABP_PlayerController>(GetController())->Index = Index;
		UE_LOG(LogTemp, Warning, TEXT("AmountOfPlayers increase Chel"))
		for (int i = 0; i < 2; ++i)
		{
			ASpectator* spec = World->SpawnActorDeferred<ASpectator>(SpectatorClass, CameraComp->GetComponentTransform());
			if (spec != nullptr) {
				spec->Player = this;
				spec->Index = Index;
				spec->Layer = i;
				UGameplayStatics::FinishSpawningActor(spec, CameraComp->GetComponentTransform());
			}
			if (GS != nullptr)
				GS->Spectators.Add(spec);
		}

		DoesHave.Init(false, 3);
	}

	if (IsServerAuth && IsPlayerOwner)
		Cast<ABP_PlayerController>(GetController())->IsHost = true;
}

void AChel::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	MyBeginPlay();
}

// Called every frame
void AChel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (IsInGame == true) {
		if (IsServerAuth) {
			DeltaTime *= 2 * 0.01f * RadCoeff * CanalizationDamage;
			Health += DeltaTime;
			if (Health > 1.0f) {
				if (DoesHave[Boltorez])
					GS->CurrentBoltorez--;
				if (DoesHave[KeyShelter])
					GS->CurrentKeyShelter--;
				if (DoesHave[Otvertka])
					GS->CurrentOtvertka--;

				TArray<AActor*>Players;
				UGameplayStatics::GetAllActorsOfClass(World, StaticClass(), Players);
				for (auto& Player : Players) {
					Cast<AChel>(Player)->RefreshWidgets(DoesHave, KillerIndex, Index);
				}
				DoesHave.Init(false, 3);
				KillerIndex = -1;
				bCanWalkingAndWatching = true;
				KillPlayer();
			}
		}
	}

	if (IsPlayerOwner) {
		UserView->RadiationPoints->SetPercent(Health);
		UserView->DarkScreen->SetRenderOpacity(Health);

		FHitResult OutHit;

		FVector StartLocation = CameraComp->GetComponentLocation();
		FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

		FCollisionQueryParams CollisionParams;

		//DrawDebugLine(World, StartLocation, EndLocation, FColor::Red, false, 1, 0, 1);

		isTracedBad = false; //�����������, ��� �� �� ����� ���� ������� ������ ��� ��������
		if (World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams))
		{
			if (OutHit.bBlockingHit)
			{
				AActor* HittableActor = OutHit.GetActor();
				if (HittableActor) { //���� �� ���������� � �����-�� �����, � �� � �������
					APickableItem* TracedItem = Cast<APickableItem>(HittableActor);
					if (TracedItem) { //���� �� ���������� � ������ ��� �������
						ItemCodePickUp = TracedItem->Type;
						if (TracedItem->Type != ClickButton)
							UserView->E_Mark->SetVisibility(ESlateVisibility::Visible);
						else if (GS->IsShelterAvaliable)
							UserView->E_Mark->SetVisibility(ESlateVisibility::Visible);
						isTracedBad = false;
						bLineTrace_is_need_refresh = true;  //�������, ��� � ������� ����� �� ��������� � ������ �������
						if (LastItem)
							LastItem->Item->SetCustomDepthStencilValue(0);
						LastItem = TracedItem;
						if (TracedItem->Type == ClickButton)
						{
							if (GS->IsCodeTerminalAvaliable)
								LastItem->Item->SetCustomDepthStencilValue(2);
						}
						else if (TracedItem->Type == OpenArea) {
							AOpenArea* MyOpenArea = Cast<AOpenArea>(TracedItem);
							if (!MyOpenArea->bIsAvaliable) {
								UserView->E_Mark->SetVisibility(ESlateVisibility::Hidden);
							}
							else {
								LastItem->Item->SetCustomDepthStencilValue(2);
							}
						}
						else if (TracedItem->Type >= 0 && TracedItem->Type <= 2)
						{
							if (DoesHave[LastItem->Type])
								LastItem->Item->SetCustomDepthStencilValue(1);
							else
								LastItem->Item->SetCustomDepthStencilValue(2);
						}
						else
							LastItem->Item->SetCustomDepthStencilValue(2);
					}
					else
					{
						ACache* TracedCache = Cast<ACache>(HittableActor);
						if (LastItem)
							LastItem->Item->SetCustomDepthStencilValue(0);
						if (TracedCache) {
							if (TracedCache->IsEnabled && KeysCount[TracedCache->CacheType] > 0) {
								isTracedBad = false;
								bLineTrace_is_need_refresh = true;
								ItemCodePickUp = Cache;
								UserView->E_Mark->SetVisibility(ESlateVisibility::Visible);

								LastCache = TracedCache;
							}
						}
						else
						{
							AButtonCanalization* TracedButton = Cast<AButtonCanalization>(HittableActor);
							if (TracedButton) {
								bLineTrace_is_need_refresh = true;
								ItemCodePickUp = CanalizationButton;
								UserView->E_Mark->SetVisibility(ESlateVisibility::Visible);

								LastButton = TracedButton;
							}
							else
							{
								AWebCamLocker* TracedWebCamLocker = Cast<AWebCamLocker>(HittableActor);
								if (TracedCache)
									TracedCache->Mesh->SetCustomDepthStencilValue(0);
								if (TracedWebCamLocker)
								{
									bLineTrace_is_need_refresh = true;
									ItemCodePickUp = WebCamLocker;
									UserView->E_Mark->SetVisibility(ESlateVisibility::Visible);
									TracedWebCamLocker->Mesh->SetCustomDepthStencilValue(2);
									LastWebCamLocker = TracedWebCamLocker;
								}
								else
									isTracedBad = true; //�� �� ������ � ������ ��� �������
								
							}
						}
					}
				}
				else
					isTracedBad = true;  //�� �� ������ � ������ ��� �������
			}
			else
				isTracedBad = true;  //�� �� ������ � ������ ��� �������
		}
		else
			isTracedBad = true;   //�� �� ������ � ������ ��� �������

		if (isTracedBad && bLineTrace_is_need_refresh) { 
			//���� �� �� ������ � ������ ��� �������, �� �� ������� ����� �� ���������� ������,
			// �� ��� ����������� �� �������� ��� ������� �� ������� �������� ������������ � ������ ���������
			bLineTrace_is_need_refresh = false;

			ItemCodePickUp = -1;
			if (LastItem)
			{
				LastItem->Item->SetCustomDepthStencilValue(0);
			}

			if (LastWebCamLocker)
			{
				LastWebCamLocker->Mesh->SetCustomDepthStencilValue(0);
			}

			if (LastItem && Cast<AOpenArea>(LastItem)) 
			{
				PickUp_Released();
			}
			UserView->StopAllAnimations();
			UserView->E_Mark->SetVisibility(ESlateVisibility::Hidden);

		}
		for(int i = 0; i < TargetArrowsStatic.Num(); ++i)
		{
			UpdateTargetArrowPosition(TargetItemsStatic[i],TargetArrowsStatic[i]);
		}
		//UE_LOG(LogTemp, Warning, TEXT("TargetItemsDynamic.Num - %d"), TargetItemsDynamic.Num());
		//UE_LOG(LogTemp, Warning, TEXT("TargetArrowsDynamic.Num - %d"), TargetArrowsDynamic.Num());
		for (int i = 0; i < TargetArrowsDynamic.Num(); ++i)
		{
			UpdateTargetArrowPosition(TargetItemsDynamic[i], TargetArrowsDynamic[i]);
		}
	}
}

//NicknameSetup----------------
void AChel::DeliverNicknameToServer_Implementation(const FText& newNickName) {
	GS->NickNames[Index] = newNickName;
}

bool AChel::DeliverNicknameToServer_Validate(const FText& newNickName) {
	return true;
}
// Called to bind functionality to input
void AChel::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("GoForward", this, &AChel::GoForward);
	PlayerInputComponent->BindAxis("GoRight", this, &AChel::GoRight);
	PlayerInputComponent->BindAxis("LookUp", this, &AChel::LookUp);
	PlayerInputComponent->BindAxis("LookRight", this, &AChel::LookRight);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AChel::MyJump);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AChel::StartSprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AChel::StopSprint);
	PlayerInputComponent->BindAction("KillFeed", IE_Pressed, this, &AChel::ShowKillFeed);
	PlayerInputComponent->BindAction("KillFeed", IE_Released, this, &AChel::UnShowKillFeed);
	PlayerInputComponent->BindAction("ThrowStone", IE_Pressed, this, &AChel::ThrowStone);
	PlayerInputComponent->BindAction("PickUp", IE_Pressed, this, &AChel::PickUp);
	PlayerInputComponent->BindAction("PickUp", IE_Released, this, &AChel::PickUp_Released);
	PlayerInputComponent->BindAction("Opening", IE_Pressed, this, &AChel::OpenAreaPressed);
	PlayerInputComponent->BindAction("Opening", IE_Released, this, &AChel::OpenAreaReleased);
	PlayerInputComponent->BindAction("UpdateSpectating_Left", IE_Released, this, &AChel::UpdateSpectating_Left);
	PlayerInputComponent->BindAction("UpdateSpectating_Right", IE_Released, this, &AChel::UpdateSpectating_Right);
}

void AChel::UpdateSpectating_Right()
{
	if (!IsNotInWebCam)
	{
		UpdateSpectating_Right_Server();
		CameraSwitch();
	}
}

void AChel::SetCameraRotationWebCam_Implementation(float RollRot, float PitchRot, float YawRot)
{
	BaseRotation = FRotator(PitchRot, YawRot, RollRot);
	UE_LOG(LogTemp, Warning, TEXT("TmpBaseRotationYaw - %f"  ),   YawRot);
	UE_LOG(LogTemp, Warning, TEXT("TmpBaseRotationPitch - %f"), PitchRot);
	CameraComp->SetWorldRotation(BaseRotation);
}

void AChel::UpdateSpectating_Right_Server_Implementation()
{
	if (WebCamIterator != -1)
		GS->WebCams[WebCamIterator]->is_Enabled = true;
	WebCamIterator += 1;
	if (WebCamIterator >= GS->WebCams.Num())
	{
		WebCamIterator -= GS->WebCams.Num();
	}
	UE_LOG(LogTemp, Warning, TEXT("WebCamIterator - %d"), WebCamIterator);
	UE_LOG(LogTemp, Warning, TEXT("Count - %d"), GS->WebCams.Num());
	
	GoToWebCamServer(WebCamIterator);
	SetCameraRotationWebCam(GS->WebCams[WebCamIterator]->GetActorRotation().Roll,
		GS->WebCams[WebCamIterator]->GetActorRotation().Pitch,
		GS->WebCams[WebCamIterator]->GetActorRotation().Yaw);
}

bool AChel::UpdateSpectating_Right_Server_Validate()
{
	return true;
}

void AChel::UpdateSpectating_Left()
{
	if (!IsNotInWebCam)
	{
		UpdateSpectating_Left_Server();
		CameraSwitch();
	}
}

void AChel::UpdateSpectating_Left_Server_Implementation()
{
	if (WebCamIterator != -1)
		GS->WebCams[WebCamIterator]->is_Enabled = true;
	WebCamIterator -= 1;
	if (WebCamIterator < 0)
	{
		WebCamIterator += GS->WebCams.Num();
	}
	UE_LOG(LogTemp, Warning, TEXT("WebCamIterator - %d"), WebCamIterator);
	UE_LOG(LogTemp, Warning, TEXT("Count - %d"), GS->WebCams.Num());

	GoToWebCamServer(WebCamIterator);
	SetCameraRotationWebCam(GS->WebCams[WebCamIterator]->GetActorRotation().Roll,
		GS->WebCams[WebCamIterator]->GetActorRotation().Pitch,
		GS->WebCams[WebCamIterator]->GetActorRotation().Yaw);
}

bool AChel::UpdateSpectating_Left_Server_Validate()
{
	return true;
}

void AChel::OpenAreaPressed() 
{
	if (!bInEscMenu && bCanWalkingAndWatching) {
		if (AreaCode != -1)
		{
			IsSuccessOpening = true;
			if (AreaCode <= 2 && AreaCode >= 0) {
				if (!GS->AreaAvaliables[AreaCode] && DoesHave[AreaCode]) {
					UserView->PB_Opening->SetVisibility(ESlateVisibility::Visible);
					UserView->TimeLeft->SetVisibility(ESlateVisibility::Visible);
				}
			}
			switch (AreaCode)
			{
			case Boltorez:
			{
				if (GS->AreaAvaliables[AreaCode])
				{
					PlayerEscape(AreaCode);
					UserView->RemoveFromParent();
				}
				else if (DoesHave[AreaCode])
				{
					UserView->PlayAnimation(UserView->CanalizaciaAnim);
				}
				break;
			}
			case KeyShelter:
			{
				if (DoesHave[AreaCode] && !GS->AreaAvaliables[AreaCode])
				{
					UserView->PlayAnimation(UserView->ShelterAnim);
				}
				break;
			}
			case Otvertka:
			{
				if (GS->AreaAvaliables[AreaCode])
				{
					PlayerEscape(AreaCode);
					UserView->RemoveFromParent();
				}
				else if (DoesHave[AreaCode])
				{
					UserView->PlayAnimation(UserView->VentilaciaAnim);
				}
				break;
			}
			case GeneratorArea:
			{
				if (GenAreaObj) {
					if (GenAreaObj->IsAvalible) {
						if (GeneratorView->IsVisible()) {
							if (GeneratorView->Corretca->Value <= GeneratorView->PB_Repair->Percent) {
								ChangeGeneratorStas();
							}
							else {
								GeneratorView->ChangeCorretcaPosition(GenAreaObj->Stadiya);
								OutlineBad_Server();//����� ����� ���� ���� ������ � �������� ����������� ������ ��� ���� ���������, �� ��� 12 ����� ����, ����, ��
							}
						}
						else {
							UserView->HoldText->SetVisibility(ESlateVisibility::Hidden);
							GeneratorView->ChangeCorretcaPosition(GenAreaObj->Stadiya);
							GeneratorView->SetVisibility(ESlateVisibility::Visible);
						}
					}
				}
				break;
			}
			case ShelterOpener:
			{
				if (GS->AreaAvaliables[1]) {
					PlayerEscape(1);
					UserView->RemoveFromParent();
				}
			}
			}
		}
	}
}

void AChel::OpenAreaReleased()
{
	if (AreaCode != -1) {
		if (IsSuccessOpening) {
			IsSuccessOpening = false;
			UserView->StopAllAnimations();
		}
	}
}

void AChel::AddChromaticAbberationClient_Implementation()
{
	AddChromaticAbberation();
}

//TimelineAnimation
void AChel::OnTimelineFinished_Stone_First() {
	TimeLine_Stone_Second->ReverseFromEnd();
	if (IsPlayerOwner)
	{
		PlayStoneThrowSound();
	}

	if (IsServerAuth) {
		--Ammo;
		StoneCountUpdate(Ammo);

		FTransform trans;
		trans.SetLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Stone->GetComponentLocation().Z));
		trans.SetRotation(FQuat(Stone->GetComponentRotation()));

		AStone* NewStone = World->SpawnActorDeferred<AStone>(StoneClass, trans, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		NewStone->Index = Index;
		UGameplayStatics::FinishSpawningActor(NewStone, trans);
		if (Ammo == 0) {
			HideStoneMulticast();
		}
	}
}

void AChel::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (GetLocalRole() == ROLE_Authority) {
		AStone* OverStone = Cast<AStone>(OtherActor);
		if (OverStone) {
			if (OverStone->Index != Index)
			{
				StoneAttack(OverStone->Index);
				TArray<AActor*>Players;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChel::StaticClass(), Players);
				for (int i = 0; i < Players.Num(); i++) {
					AChel* Chel = Cast<AChel>(Players[i]);
					if (Chel->Index == OverStone->Index)
					{
						Chel->AddHitMarker();
						break;
					}
				}
				PlayStoneHit();
				OverStone->Destroy();
			}
		}
	}
}

void AChel::StoneCountUpdate_Implementation(int32 Count)
{
	UserView->AmmoLabel->SetText(FText::AsNumber(Count));
}

void AChel::TimelineVectorReturn_Stone(FVector value) {
	Stone->SetRelativeLocation(StonePosition + value);
}

void AChel::TimelineFloatReturn_FOV_WebCam(float value)
{
	CameraComp->SetFieldOfView(value);
}

void AChel::OnTimelineFinished_Stone_Second() {
	bIsAlreadyThrowing = false;
}
//-----------------------------

//AttackInput------------------
void AChel::ThrowStone() {
	if (Ammo > 0 && IsNotInWebCam && bCanWalkingAndWatching && !bInEscMenu && CanThrowStone) {
		if (!bIsAlreadyThrowing) {
			bIsAlreadyThrowing = true;
			TimeLine_Stone_First->PlayFromStart();
			ThrowStoneServer();
		}
	}
}

void AChel::ThrowStoneMulticast_Implementation()
{
	if (!IsPlayerOwner)
		TimeLine_Stone_First->PlayFromStart();
}

void AChel::ThrowStoneServer_Implementation()
{
	ThrowStoneMulticast();
}

bool AChel::ThrowStoneServer_Validate()
{
	return true;
}

void AChel::HideStoneMulticast_Implementation() {
	Stone->SetHiddenInGame(true);
}

void AChel::ShowStoneMulticast_Implementation() {
	Stone->SetHiddenInGame(false);
}
//----------------------------

//KeyBoardInput----------------
void AChel::GoForward(float input) {
	if (bCanWalkingAndWatching && !bInEscMenu) {
		if (input != 0.0f && IsNotInWebCam) {
			AddMovementInput(GetActorForwardVector(), input * MoveCoeff);
		}
	}
}

void AChel::GoRight(float input) {
	if (bCanWalkingAndWatching && !bInEscMenu) {
		if (input != 0.0f && IsNotInWebCam) {
			AddMovementInput(GetActorRightVector(), input * MoveCoeff);
		}
	}
}
//-----------------------------

//ReplicationMouseInput--------
void AChel::MeshCompRepMulticast_Implementation(float RotationRoll)
{
	if (!IsPlayerOwner)
		PoseableMeshComp->SetBoneRotationByName(TEXT("Bone_002"), { 0, 0, RotationRoll }, EBoneSpaces::ComponentSpace);
}


void AChel::MeshCompRepServer_Implementation(float RotationRoll)
{
	MeshCompRepMulticast(RotationRoll);
}

bool AChel::MeshCompRepServer_Validate(float RotationRoll)
{
	return true;
}
//-----------------------------

//MouseInput-------------------
void AChel::LookUp(float input)
{
	if (bCanWalkingAndWatching && !bInEscMenu) {
		if (IsNotInWebCam) {
			if (input != 0.0f)
			{
				input *= Sensetivity;
				float RotationRoll = PoseableMeshComp->GetBoneRotationByName(TEXT("Bone_002"), EBoneSpaces::ComponentSpace).Roll;
				if (RotationRoll + input >= 0.f && RotationRoll + input <= 179.f) {
					RotationRoll += input;
				}
				else if (RotationRoll + input > 179.f) {
					RotationRoll = 179.f;
				}
				else
					RotationRoll = 0;
				PoseableMeshComp->SetBoneRotationByName(TEXT("Bone_002"), { 0, 0, RotationRoll }, EBoneSpaces::ComponentSpace);
				MeshCompRepServer(RotationRoll);
			}
		}
		else
		{
			if (input != 0.0f) {
				input *= Sensetivity * WebCamSensetivity;
				float NewPitchRot = CameraComp->GetComponentRotation().Pitch - BaseRotation.Pitch;

				if (NewPitchRot + BaseRotation.Pitch >= 180.f)
					NewPitchRot -= 360;
				else if (NewPitchRot + BaseRotation.Pitch <= -180.f)
					NewPitchRot += 360;

				NewPitchRot = FMath::Clamp<float>(NewPitchRot + input, -MaxPitchAngle, MaxPitchAngle);
				UE_LOG(LogTemp, Warning, TEXT("Rotation - %f"), NewPitchRot + BaseRotation.Pitch);
				CameraComp->SetWorldRotation(FRotator(NewPitchRot + BaseRotation.Pitch,
					CameraComp->GetComponentRotation().Yaw, CameraComp->GetComponentRotation().Roll));
			}
		}
	}
}


void AChel::LookRight(float input)
{
	if (bCanWalkingAndWatching && !bInEscMenu) {
		if (IsNotInWebCam) {
			if (input != 0.0f) {
				input *= Sensetivity;
				AddControllerYawInput(input);
			}
		}
		else
		{
			if (input != 0.0f) {
				input *= Sensetivity * WebCamSensetivity;
				float NewYawRot = CameraComp->GetComponentRotation().Yaw - BaseRotation.Yaw;

				NewYawRot = FMath::Clamp<float>(NewYawRot + input, -MaxYawAngle, MaxYawAngle);

				CameraComp->SetWorldRotation(FRotator(CameraComp->GetComponentRotation().Pitch, NewYawRot + BaseRotation.Yaw,
					CameraComp->GetComponentRotation().Roll));
			}
		}
	}
}
//-----------------------------

//Jump-------------------------
void AChel::MyJump()
{
	if (bCanWalkingAndWatching) {
		if (IsNotInWebCam)
			Jump();
	}
}
//-----------------------------


//Sprint-----------------------
void AChel::StartSprint_Server_Implementation()
{
	GetCharacterMovement()->MaxWalkSpeed = 1200.f;
}

bool AChel::StartSprint_Server_Validate()
{
	return true;
}

void AChel::StartSprint() {
	GetCharacterMovement()->MaxWalkSpeed = 1200.f;
	StartSprint_Server();
}

void AChel::StopSprint_Server_Implementation()
{
	GetCharacterMovement()->MaxWalkSpeed = 800.f;
}

bool AChel::StopSprint_Server_Validate()
{
	return true;
}

void AChel::StopSprint() {
	GetCharacterMovement()->MaxWalkSpeed = 800.f;
	StopSprint_Server();
}
//-----------------------------


//KillFeed --------------------
void AChel::ShowKillFeed()
{
	KillFeed->SetVisibility(ESlateVisibility::Visible);
}

void AChel::UnShowKillFeed()
{
	KillFeed->SetVisibility(ESlateVisibility::Hidden);
}

//PlayStartingAnimation---------------------
void AChel::PlaySpawnAnimationSleep_Implementation() {
	for (int i = 0; i < TargetArrowsStatic.Num(); ++i)
	{
		TargetArrowsStatic.RemoveAt(i);
		TargetItemsStatic.RemoveAt(i);
	}
	for (int i = 0; i < TargetArrowsDynamic.Num(); ++i)
	{
		TargetArrowsDynamic.RemoveAt(i);
		TargetItemsDynamic.RemoveAt(i);
	}
	ResetCacheKeys();
	CanThrowStone = false;
	SpawnDeadSound();
	UserView->PlayAnimation(UserView->Shading);
	FTimerHandle FuzeTimerHandle;
	World->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::SleepAnimation_End, 2.0f, false);
}

void AChel::SleepAnimation_End()
{
	if (bCanPossessWebCam) {
		UserView->SetVisibility(ESlateVisibility::Hidden);
		IsNotInWebCam = false;
		UpdateSpectating_Right();
		WebCamUI->SetVisibility(ESlateVisibility::Visible);
		IsAwake = false;
	}
	else
		IsAwake = false;
}

void AChel::AwakeAnimation_End()
{
	CameraComp->SetFieldOfView(90.0f);
	if (bCanPossessWebCam) {
		IsAwake = true;
	}
	else
	{
		bCanPossessWebCam = true;
		IsAwake = true;
	}
}

//PlayStartingAnimation---------------------
void AChel::PlaySpawnAnimationAwake_Implementation() {
	TimeLine_FOV_WebCam->Stop();
	IsNotInWebCam = true;
	FTimerHandle FuzeTimerHandle;
	World->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::AwakeAnimation_End, 2, false);
	CanThrowStone = true;
	CameraComp->SetRelativeLocation({ 0,0,0 });
	CameraComp->SetRelativeRotation({ 0,0,0 });
	if (bCanPossessWebCam)
		CameraTurnOff();
	CameraComp->SetFieldOfView(90.0f);
	StoneCountUpdate(15);

	WebCamUI->SetVisibility(ESlateVisibility::Hidden);

	UserView->SetVisibility(ESlateVisibility::Visible);
	SpawnWakeUpSound();
	UserView->PlayAnimation(UserView->Shading, 0, 1, EUMGSequencePlayMode::Type::Reverse);
}

//-----------------------------
void AChel::PickUp() {
	if (!bInEscMenu) {
		if (IsNotInWebCam) {
			if (ItemCodePickUp != -1) {
				UE_LOG(LogTemp, Warning, TEXT("ItemCode - %d"), ItemCodePickUp);
				IsSuccessOpening = true;
				switch (ItemCodePickUp) {
				case Boltorez:
				{
					if (!DoesHave[Boltorez]) {

						UserView->WS_Boltorez->SetActiveWidgetIndex(1);
						DoesHave_Owner = true;
						NewHaveItemServer(Boltorez);
						ArrowCanalizacia = Cast<UTargetArrow>(CreateWidget(World, TargetArrowClass));
						ArrowCanalizacia->AddToViewport();
						MainExis_Canalizacia->Mesh->SetCustomDepthStencilValue(3);
						TargetItemsDynamic.Add(MainExis_Canalizacia);
						TargetArrowsDynamic.Add(ArrowCanalizacia);
						ExisType = Boltorez;
						FTimerHandle FuzeTimerHandle;
						GetWorld()->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::DeleteArrowDelay, ShowMainExis_TIME, false);
						PickUpSound();
					}
					break;
				}
				case KeyShelter:
				{
					if (!DoesHave[KeyShelter]) {
						UserView->WS_KeyShelter->SetActiveWidgetIndex(1);
						DoesHave_Owner = true;
						NewHaveItemServer(KeyShelter);
						ArrowShelter = Cast<UTargetArrow>(CreateWidget(World, TargetArrowClass));
						ArrowShelter->AddToViewport();
						MainExis_Shelter->Mesh->SetCustomDepthStencilValue(3);
						TargetItemsDynamic.Add(MainExis_Shelter);
						TargetArrowsDynamic.Add(ArrowShelter);
						ExisType = KeyShelter;
						FTimerHandle FuzeTimerHandle;
						GetWorld()->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::DeleteArrowDelay, ShowMainExis_TIME, false);
						PickUpSound();
					}
					break;
				}
				case Otvertka:
				{
					if (!DoesHave[Otvertka]) {
						UserView->WS_Otvertka->SetActiveWidgetIndex(1);
						DoesHave_Owner = true;
						NewHaveItemServer(Otvertka);
						ArrowVentilacia = Cast<UTargetArrow>(CreateWidget(World, TargetArrowClass));
						ArrowVentilacia->AddToViewport();
						MainExis_Ventilacia->Mesh->SetCustomDepthStencilValue(3);
						TargetItemsDynamic.Add(MainExis_Ventilacia);
						TargetArrowsDynamic.Add(ArrowVentilacia);
						ExisType = Otvertka;
						FTimerHandle FuzeTimerHandle;
						GetWorld()->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::DeleteArrowDelay, ShowMainExis_TIME, false);
						PickUpSound();
					}
					break;
				}
				case CacheKey:
				{
					int KeyType = Cast<ACache_Key>(LastItem)->KeyType;
					switch (KeyType)
					{
					case KeyBronze:
					{
						UserView->KeyLeft_Bronze->SetText(FText::AsNumber(KeysCount[KeyBronze] + 1));
						break;
					}
					case KeySilver:
					{
						UserView->KeyLeft_Silver->SetText(FText::AsNumber(KeysCount[KeySilver] + 1));
						break;
					}
					case KeyGold:
					{
						UserView->KeyLeft_Gold->SetText(FText::AsNumber(KeysCount[KeyGold] + 1));
						break;
					}
					}
					KeysCount[KeyType]++;
					DoesHave_Owner = true;
					NewHaveItemServer(ItemCodePickUp);
					PickUpSound();
					break;
				}
				case Cache:
				{
					if (LastCache != nullptr) {
						if (KeysCount[LastCache->CacheType] > 0)
						{
							switch (LastCache->CacheType)
							{
							case KeyBronze:
							{
								UserView->KeyLeft_Bronze->SetText(FText::AsNumber(KeysCount[KeyBronze] - 1));
								break;
							}
							case KeySilver:
							{
								UserView->KeyLeft_Silver->SetText(FText::AsNumber(KeysCount[KeySilver] - 1));
								break;
							}
							case KeyGold:
							{
								UserView->KeyLeft_Gold->SetText(FText::AsNumber(KeysCount[KeyGold] - 1));
								break;
							}
							}
							KeysCount[LastCache->CacheType]--;
							UserView->E_Mark->SetVisibility(ESlateVisibility::Hidden);
							ChangeIsAvaliableCache();
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("LastCache is nullptr"))
					}
					break;

				}
				case CanalizationButton:
				{
					UserView->E_Mark->SetVisibility(ESlateVisibility::Hidden);
					ChangeButtonCount_Server();
					break;
				}
				case WebCamLocker:
				{
					LockWebCam_Server();
					break;
				}
				case InvisiblePotion:
				{
					AddInvisibleServer();
					break;
				}
				case CodeNote:
				{
					if (Widget_Note->IsVisible())
					{
						Widget_Note->SetVisibility(ESlateVisibility::Hidden);
						PickUpSound();
						bCanWalkingAndWatching = true;
					}
					else
					{
						Widget_Note->ChangeText(GS->CodeGenerator);
						Widget_Note->SetVisibility(ESlateVisibility::Visible);
						PickUpSound();
						bCanWalkingAndWatching = false;
					}
					break;
				}
				case ClickButton:
				{
					AClickButton* CurButton = Cast<AClickButton>(LastItem);
					if (GS->IsCodeTerminalAvaliable && !GS->ButtonPlayAnim && CurButton)
					{
						if (CurButton->ButtonType <= 9) {
							if (GS->NumbersOnPanel.Num() <= 4) {
								AddNumToTerminalServer(CurButton->ButtonType);
								ButtonPressAnimationServer();
							}
						}
						else {
							if (CurButton->ButtonType == 10) {
								if (GS->NumbersOnPanel.Num() > 0) {
									DeleteLAstNumServer();
									ButtonPressAnimationServer();
								}
							}
							else
							{
								CheckCodeServer();
								ButtonPressAnimationServer();
							}
						}
					}
					break;
				}
				case OpenArea:
				{
					AOpenArea* CurOpArea = Cast<AOpenArea>(LastItem);
					if (!CurOpArea->bIsUsed)
					{
						if (CurOpArea != nullptr && CurOpArea->bIsAvaliable) {
							UserView->PB_Opening->SetVisibility(ESlateVisibility::Visible);
							UserView->TimeLeft->SetVisibility(ESlateVisibility::Visible);
						}
						UserView->PlayAnimation(UserView->OpenAreaAnim, CurOpArea->CurTimeVentil);
						GoToServerOpenArea(true);
						break;
					}
				}
				}
			}
		}
		else
		{
			TimeLine_FOV_WebCam->Play();
			CameraZoomIn();
		}
	}
}

void AChel::PickUp_Released()
{
	if (bCanWalkingAndWatching) {
		if (IsNotInWebCam) {
			IsSuccessOpening = false;
			if (Cast<AOpenArea>(LastItem))
			{
				GoToServerOpenArea(false);
			}
			UserView->StopAllAnimations();
		}
		else
		{
			TimeLine_FOV_WebCam->Reverse();
			CameraZoomOut();
		}
	}
}

void AChel::ChangeIsAvaliableCache_Implementation()
{
	FHitResult OutHit;

	FVector StartLocation = CameraComp->GetComponentLocation();
	FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

	FCollisionQueryParams CollisionParams;

	World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
	if (OutHit.GetActor()) {
		ACache* TempItem = Cast<ACache>(OutHit.GetActor());
		if (TempItem)
		{
			TempItem->IsEnabled = false;
			TempItem->Opening();
			return;
		}
	}
}

bool AChel::ChangeIsAvaliableCache_Validate()
{
	return true;
}

void AChel::NewHaveItemServer_Implementation(int32 ItemType)
{
	if (ItemType >= 0 && ItemType <= 2) {
		DoesHave[ItemType] = true;
		TArray<AActor*>Players;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChel::StaticClass(), Players);
		for (auto& Player : Players)
			Cast<AChel>(Player)->NewHaveItemClient(ItemType);
	}

	FHitResult OutHit;

	FVector StartLocation = CameraComp->GetComponentLocation();
	FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

	FCollisionQueryParams CollisionParams;

	World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
	if (OutHit.GetActor()) {
		APickableItem* TempItem = Cast<APickableItem>(OutHit.GetActor());
		if (TempItem)
		{
			ACache_Key* CacheKey = Cast<ACache_Key>(TempItem);
			if (CacheKey)
			{
				CacheKey->RemoveAndRefreshTimer();
			}
			else
			{
				switch (TempItem->Type)
				{
				case 0:
				{
					if (GS->CurrentBoltorez == 1)
					{
						int ArrayIndex = FMath::Rand() % GS->CacheItems_Stuff_IsAvaliable.Num();
						while (!GS->CacheItems_Stuff_IsAvaliable[ArrayIndex])
						{
							ArrayIndex = FMath::Rand() % GS->CacheItems_Stuff_IsAvaliable.Num();
						}
						FAttachmentTransformRules AttachmentRules = FAttachmentTransformRules(EAttachmentRule::KeepRelative, true);
						TempItem->AttachToActor(GS->Caches[ArrayIndex], AttachmentRules);
						TempItem->SetActorScale3D(GS->BoltorezTransform[GS->Caches[ArrayIndex]->CacheIndex].GetScale3D());
						FVector NewLocation;
						NewLocation.X = GS->BoltorezTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().X / abs(GS->Caches[ArrayIndex]->GetActorScale3D().X);
						NewLocation.Y = GS->BoltorezTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().Y / abs(GS->Caches[ArrayIndex]->GetActorScale3D().Y);
						NewLocation.Z = GS->BoltorezTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().Z / abs(GS->Caches[ArrayIndex]->GetActorScale3D().Z);
						TempItem->SetActorRelativeLocation({ 0.0f, 0.0f, 0.0f });
						TempItem->SetActorRelativeRotation({ 0.0f, 0.0f, 0.0f });
						TempItem->AddActorLocalOffset(NewLocation);
						TempItem->AddActorLocalRotation(GS->BoltorezTransform[GS->Caches[ArrayIndex]->CacheIndex].GetRotation());
						GS->CacheItems_Stuff_IsAvaliable[TempItem->EnabledArrayIndex] = true;
						Cast<APickableItem>(TempItem)->EnabledArrayIndex = ArrayIndex;
						GS->CacheItems_Stuff_IsAvaliable[ArrayIndex] = false;
					}
					else
					{
						GS->CurrentBoltorez--;
						TempItem->Destroy();
					}
					break;
				}
				case 1:
				{
					if (GS->CurrentKeyShelter == 1)
					{
						int ArrayIndex = FMath::Rand() % GS->CacheItems_Stuff_IsAvaliable.Num();
						while (!GS->CacheItems_Stuff_IsAvaliable[ArrayIndex])
						{
							ArrayIndex = FMath::Rand() % GS->CacheItems_Stuff_IsAvaliable.Num();
						}
						FAttachmentTransformRules AttachmentRules = FAttachmentTransformRules(EAttachmentRule::KeepRelative, true);
						TempItem->AttachToActor(GS->Caches[ArrayIndex], AttachmentRules);
						TempItem->SetActorScale3D(GS->KeyShelterTransform[GS->Caches[ArrayIndex]->CacheIndex].GetScale3D());
						FVector NewLocation;
						NewLocation.X = GS->KeyShelterTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().X / abs(GS->Caches[ArrayIndex]->GetActorScale3D().X);
						NewLocation.Y = GS->KeyShelterTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().Y / abs(GS->Caches[ArrayIndex]->GetActorScale3D().Y);
						NewLocation.Z = GS->KeyShelterTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().Z / abs(GS->Caches[ArrayIndex]->GetActorScale3D().Z);
						TempItem->SetActorRelativeLocation({ 0.0f, 0.0f, 0.0f });
						TempItem->SetActorRelativeRotation({ 0.0f, 0.0f, 0.0f });
						TempItem->AddActorLocalOffset(NewLocation);
						TempItem->AddActorLocalRotation(GS->KeyShelterTransform[GS->Caches[ArrayIndex]->CacheIndex].GetRotation());
						GS->CacheItems_Stuff_IsAvaliable[TempItem->EnabledArrayIndex] = true;
						Cast<APickableItem>(TempItem)->EnabledArrayIndex = ArrayIndex;
						GS->CacheItems_Stuff_IsAvaliable[ArrayIndex] = false;
					}
					else
					{
						GS->CurrentKeyShelter--;
						TempItem->Destroy();
					}
					break;
				}
				case 2:
				{
					if (GS->CurrentOtvertka == 1)
					{
						int ArrayIndex = FMath::Rand() % GS->CacheItems_Stuff_IsAvaliable.Num();
						while (!GS->CacheItems_Stuff_IsAvaliable[ArrayIndex])
						{
							ArrayIndex = FMath::Rand() % GS->CacheItems_Stuff_IsAvaliable.Num();
						}
						FAttachmentTransformRules AttachmentRules = FAttachmentTransformRules(EAttachmentRule::KeepRelative, true);
						TempItem->AttachToActor(GS->Caches[ArrayIndex], AttachmentRules);
						TempItem->SetActorScale3D(GS->OtvertkaTransform[GS->Caches[ArrayIndex]->CacheIndex].GetScale3D());
						FVector NewLocation;
						NewLocation.X = GS->OtvertkaTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().X / abs(GS->Caches[ArrayIndex]->GetActorScale3D().X);
						NewLocation.Y = GS->OtvertkaTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().Y / abs(GS->Caches[ArrayIndex]->GetActorScale3D().Y);
						NewLocation.Z = GS->OtvertkaTransform[GS->Caches[ArrayIndex]->CacheIndex].GetLocation().Z / abs(GS->Caches[ArrayIndex]->GetActorScale3D().Z);
						TempItem->SetActorRelativeLocation({ 0.0f, 0.0f, 0.0f });
						TempItem->SetActorRelativeRotation({ 0.0f, 0.0f, 0.0f });
						TempItem->AddActorLocalOffset(NewLocation);
						TempItem->AddActorLocalRotation(GS->OtvertkaTransform[GS->Caches[ArrayIndex]->CacheIndex].GetRotation());
						GS->CacheItems_Stuff_IsAvaliable[TempItem->EnabledArrayIndex] = true;
						Cast<APickableItem>(TempItem)->EnabledArrayIndex = ArrayIndex;
						GS->CacheItems_Stuff_IsAvaliable[ArrayIndex] = false;
					}
					else
					{
						GS->CurrentOtvertka--;
						TempItem->Destroy();
					}
					break;
				}
				}
			}
		}
	}
	
}

bool AChel::NewHaveItemServer_Validate(int32 ItemType)
{
	return true;
}

void AChel::NewHaveItemClient_Implementation(int32 ItemType)
{
	if (!DoesHave_Owner)
		switch (ItemType) {
		case Boltorez:
		{
			UserView->WS_Boltorez->SetRenderOpacity(0.5f);
			break;
		}
		case KeyShelter:
		{
			UserView->WS_KeyShelter->SetRenderOpacity(0.5f);
			break;
		}
		case Otvertka:
		{
			UserView->WS_Otvertka->SetRenderOpacity(0.5f);
			break;
		}
		default:
			break;
		}
	DoesHave_Owner = false;
}

void AChel::AddHitMarker_Implementation()
{
	UserView->Marker->SetVisibility(ESlateVisibility::Visible);
	FTimerHandle FuzeTimerHandle;
	World->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::RemoveHitMarker, 0.05, false);
}

void AChel::RemoveHitMarker()
{
	UserView->Marker->SetVisibility(ESlateVisibility::Hidden);
}

void AChel::StoneAttack(int StoneIndex)
{
	if (IsInGame) 
	{
		if (STONE_DAMAGE + Health + DeltaRadiation >= 1.0f)
		{
			KillerIndex = StoneIndex;
		}

		Health += STONE_DAMAGE;

		if (Health >= 1.0f)
		{

			if (DoesHave[Boltorez])
				GS->CurrentBoltorez--;
			if (DoesHave[KeyShelter])
				GS->CurrentKeyShelter--;
			if (DoesHave[Otvertka])
				GS->CurrentOtvertka--;

			TArray<AActor*>Players;
			UGameplayStatics::GetAllActorsOfClass(World, StaticClass(), Players);
			for (auto& Player : Players)
					Cast<AChel>(Player)->RefreshWidgets(DoesHave, KillerIndex, Index);
			DoesHave.Init(false, 3);
			bCanWalkingAndWatching = true;
			KillPlayer();
		}
	}
}

void AChel::RefreshWidgets_Implementation(const TArray<bool>& whatToUpdate, int KillerNickIndex, int VictimNickIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("WidgetUpdates"))
	if (whatToUpdate[Boltorez])
	{
		UserView->WS_Boltorez->SetActiveWidgetIndex(0);
		UserView->WS_Boltorez->SetRenderOpacity(1.f);
	}
	if (whatToUpdate[Otvertka])
	{
		UserView->WS_Otvertka->SetActiveWidgetIndex(0);
		UserView->WS_Otvertka->SetRenderOpacity(1.f);
	}
	if (whatToUpdate[KeyShelter])
	{
		UserView->WS_KeyShelter->SetActiveWidgetIndex(0);
		UserView->WS_KeyShelter->SetRenderOpacity(1.f);
	}

	if (KillerNickIndex == -1) {
		UPlayerSuicide* PS_Widget = Cast<UPlayerSuicide>(CreateWidget(World, PlayerSuicide_class));
		PS_Widget->Player->SetText(GS->NickNames[VictimNickIndex]);
		KillFeed->VB_KillFeed->AddChild(PS_Widget);
	}
	else
	{
		UPlayerKillPlayer* PKP_Widget = Cast<UPlayerKillPlayer>(CreateWidget(World, PlayerKillPlayer_class));
		PKP_Widget->Killer->SetText(GS->NickNames[KillerNickIndex]);
		PKP_Widget->Victim->SetText(GS->NickNames[VictimNickIndex]);
		KillFeed->VB_KillFeed->AddChild(PKP_Widget);
	}

	if (whatToUpdate[Boltorez] || whatToUpdate[Otvertka] || whatToUpdate[KeyShelter])
	{
		UPlayerLostItem* PLI_Widget = Cast<UPlayerLostItem>(CreateWidget(World, PlayerLostItem_class));
		PLI_Widget->Player->SetText(GS->NickNames[VictimNickIndex]);
		if (whatToUpdate[Boltorez])
			PLI_Widget->IMG_Boltorez->SetVisibility(ESlateVisibility::Visible);
		if (whatToUpdate[Otvertka])
			PLI_Widget->IMG_Otvertka->SetVisibility(ESlateVisibility::Visible);
		if (whatToUpdate[KeyShelter])
			PLI_Widget->IMG_KeyShelter->SetVisibility(ESlateVisibility::Visible);

		KillFeed->VB_KillFeed->AddChild(PLI_Widget);
	}

}

void AChel::KillPlayer()
{
	TArray<AActor*>Players;
	UGameplayStatics::GetAllActorsOfClass(World, AChel::StaticClass(), Players);

	KeysCount[0] = 0;
	KeysCount[1] = 0;
	KeysCount[2] = 0;

	for (auto& Player : Players)
	{
		AChel* Chel = Cast<AChel>(Player);
		if (Chel)
		{
			Chel->DeleteStrelkaBadOutline_Client(Index);
		}
	}
	GetCharacterMovement()->StopMovementImmediately();
	DisableCollisionEverywhere();
	HideCustomItems(true);
	SetActorHiddenInGame(true);
	if (Widget_Note)
		Widget_Note->SetVisibility(ESlateVisibility::Hidden);
	IsInGame = false;
	PlaySpawnAnimationSleep();

	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, this, &AChel::SpawnPlayer, SPAWN_TIME, false);
}

void AChel::HideCustomItems_Implementation(bool NewHide)
{
	for (auto& CustomItem : CustomizationChilds)
	{
		if (CustomItem)
			CustomItem->SetActorHiddenInGame(NewHide);
	}
}


void AChel::GoToWebCamServer(int32 Iterator)
{
	SetActorLocation(GS->WebCams[Iterator]->GetActorLocation());
	GS->WebCams[Iterator]->CurChelix = this;
	if (GS->WebCams[Iterator]->is_Enabled == true)
	{
		ShowNoiseWebCamUI(false);
	}
	else
	{
		ShowNoiseWebCamUI(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("Staying on webcam"));
}

void AChel::SpawnPlayer()
{
	UE_LOG(LogTemp, Warning, TEXT("Setting camera from webcam to player"));

	HideCustomItems(false);
	EnableCollisionEverywhere();
	ShowStoneMulticast();
	SetActorHiddenInGame(false);
	IsNotInWebCam = true;
	IsInGame = true;
	Health = 0;
	Ammo = 15;

	TArray<AActor*> TargetPoints;
	UGameplayStatics::GetAllActorsOfClassWithTag(World, ATargetPoint::StaticClass(), TEXT("spawn"), TargetPoints);
	SetActorLocation(TargetPoints[FMath::Rand() % 4]->GetActorLocation());

	PlaySpawnAnimationAwake();
}

void AChel::DisableCollisionEverywhere_Implementation()
{
	SetActorEnableCollision(false);
	GetCharacterMovement()->GravityScale = 0;
	GetCharacterMovement()->StopMovementImmediately();
}

void AChel::EnableCollisionEverywhere_Implementation()
{
	SetActorEnableCollision(true);
	GetCharacterMovement()->GravityScale = 1.2f;
	GetCharacterMovement()->StopMovementImmediately();
	CameraComp->SetFieldOfView(90.0f);
	CameraComp->SetRelativeRotation({ 0,0,0 });
}

void AChel::PlayerOpenAreaUpdate_Implementation(int32 EscapeWay)
{
	TArray<AActor*>Players;
	UGameplayStatics::GetAllActorsOfClass(World, AChel::StaticClass(), Players);
	for (auto& Player : Players) {
		Cast<AChel>(Player)->RefreshWidgets_Winner(EscapeWay);
	}

	TArray<AActor*>Chels;
	if (EscapeWay != 1) {
		GS->Areas[EscapeWay]->GetOverlappingActors(Chels);
		for (auto& Player : Chels)
		{
			Cast<AChel>(Player)->ExitAvaliableUpdate(EscapeWay);
		}
	}
	else {
		GS->Areas[3]->GetOverlappingActors(Chels);
		for (auto& Player : Chels)
		{
			Cast<AChel>(Player)->ExitAvaliableUpdate(ShelterOpener);
		}
	}
}

bool AChel::PlayerOpenAreaUpdate_Validate(int32 EscapeWay)
{
	return true;
}

void AChel::RefreshWidgets_Winner_Implementation(int32 EscapeWay)
{
	switch (EscapeWay)
	{
	case Boltorez:
	{
		UserView->WS_Boltorez->SetVisibility(ESlateVisibility::Hidden);
		break;
	}
	case Otvertka:
	{
		UserView->WS_Otvertka->SetVisibility(ESlateVisibility::Hidden);
		break;
	}
	case KeyShelter:
	{
		UserView->WS_KeyShelter->SetVisibility(ESlateVisibility::Hidden);
		break;
	}
	}
}

void AChel::StuffAvaliableUpdate_Implementation(int32 EscapeWay)
{
	GS->AreaAvaliables[EscapeWay] = true;
}

bool AChel::StuffAvaliableUpdate_Validate(int32 EscapeWay)
{
	return true;
}

void AChel::ExitAvaliableUpdate_Implementation(int32 EscapeWay)
{
	AreaCode = EscapeWay;
	UserView->EscapeText->SetVisibility(ESlateVisibility::Visible);
}

void AChel::PlayerEscape_Implementation(int32 EscapeWay)
{
	GS->AreaClosed[EscapeWay] = true;
	GS->WinnersNickNames.Add(GS->NickNames[Index]);
	GS->WinnersIndex.Add(Index);
	GS->EscapeTime.Add(GS->CurrentTime);
	
	TArray<AActor*>DeletingItems;
	switch (EscapeWay)
	{
	case Boltorez:
	{
		UGameplayStatics::GetAllActorsOfClass(World, GS->Boltorez, DeletingItems);
		break;
	}
	case KeyShelter:
	{
		UGameplayStatics::GetAllActorsOfClass(World, GS->KeyShelter, DeletingItems);
		break;
	}
	case Otvertka:
	{
		UGameplayStatics::GetAllActorsOfClass(World, GS->Otvertka, DeletingItems);
		break;
	}
	}

	for (auto& it : DeletingItems)
	{
		it->Destroy();
	}

	for (int i = 0; i < CustomizationChilds.Num(); ++i)
	{
		if (CustomizationChilds[i])
			CustomizationChilds[i]->Destroy();
	}

	if (GS->GeneralLayer == 2) {
		GS->SpawnCustomizationChels();

		TArray<AActor*>Players;
		UGameplayStatics::GetAllActorsOfClass(World, AChel::StaticClass(), Players);
		for (auto& Player : Players) {
			Cast<AChel>(Player)->DeleteGameHUD();
		}

		TArray<AActor*>PlayerControllers;
		TArray<AActor*>FinalMenuPlayers;

		UGameplayStatics::GetAllActorsOfClass(World, ABP_PlayerController::StaticClass(), PlayerControllers);
		UGameplayStatics::GetAllActorsOfClass(World, AFinalMenuPawn::StaticClass(), FinalMenuPlayers);

		for (int i = 0; i < 4; ++i)
		{
			ABP_PlayerController* PC = Cast<ABP_PlayerController>(PlayerControllers[i]);
			PC->Possess(Cast<APawn>(FinalMenuPlayers[i]));
			PC->AddFinalMenu();
		}

		TArray<AActor*>Spectators;
		UGameplayStatics::GetAllActorsOfClass(World, ASpectator::StaticClass(), Spectators);
		for (auto& Spec : Spectators) {
			Spec->Destroy();
		}
		for (auto& Player : Players) {
			Destroy();
		}
	}
	else
	{
		TArray<AActor*>Chels;
		GS->Areas[EscapeWay]->GetOverlappingActors(Chels);
		for (auto& Player : Chels)
		{
			Cast<AChel>(Player)->AreaClosedUpdate(EscapeWay);
		}

		PossessToSpectator();
	}
}

bool AChel::PlayerEscape_Validate(int32 EscapeWay)
{
	return true;
}

void AChel::AreaClosedUpdate_Implementation(int32 EscapeWay)
{
	AreaCode = -1;
	UserView->EscapeText->SetVisibility(ESlateVisibility::Hidden);
}


void AChel::PossessToSpectator()
{
	int iteration = 0;
	for (iteration = 0; iteration < 8; iteration += 2)
	{
		if (GS->Spectators[iteration]->Index == Index)
		{
			if (GS->Spectators[iteration]->IsPawnControlled())
				GS->Spectators[iteration]->UpdateSpectating_Right();
			if (GS->Spectators[iteration + 1]->IsPawnControlled())
				GS->Spectators[iteration + 1]->UpdateSpectating_Right();

			GS->Spectators[iteration]->IsKilled = true;
			GS->Spectators[iteration + 1]->IsKilled = true;
			break;
		}
	}

	iteration += 2;
	iteration += GS->GeneralLayer;
	GS->GeneralLayer++;
	while (true)
	{
		if (iteration >= 8)
		{
			iteration -= 8;
		}
		if (!GS->Spectators[iteration]->IsKilled)
		{
			GetController()->Possess(GS->Spectators[iteration]);
			Destroy();
			break;
		}
		iteration += 2;
	}
}

void AChel::ChangeGeneratorStas_Implementation()
{
	GenAreaObj->Stadiya++;
	GenAreaObj->ChangeSoundAmbientToWorking();
	GoodAttempGeneratorSound();
	if (GenAreaObj->Stadiya >= 3) {
		GenAreaObj->Stadiya = 0;
		for (auto& it : GenAreaObj->PromtCollisionTerminal)
		{
			it->bISAvaliable = true;
		}

		for (auto& it : GenAreaObj->PromtCollisionGenerator)
		{
			it->bISAvaliable = false;
		}

		TArray<AActor*>Chelix;
		GenAreaObj->ShelterCollision->Collision->GetOverlappingActors(Chelix, AChel::StaticClass());
		for (auto& it : Chelix)
		{
			Cast<AChel>(it)->ShowUIAfterTerminalAndGenerator(1, true);
		}

		GenAreaObj->DoSomethinkGen();
		GS->IsShelterAvaliable = true;
		TArray<AActor*> temp;
		UGameplayStatics::GetAllActorsOfClass(World, APromptCollisionArea::StaticClass(), temp);
		for (auto& it : temp) {
			APromptCollisionArea* CurAreaPromt = Cast<APromptCollisionArea>(it);
			if (CurAreaPromt)
			{
				if (CurAreaPromt->TypePromptCollision == 1)
				{
					CurAreaPromt->bISAvaliable = false;
					CurAreaPromt->SettingAvaliableFalse();
				}
			}
		}


		ShowRandomItem();

		TArray<AActor*> Players;
		GenAreaObj->GetOverlappingActors(Players, AChel::StaticClass());
		for (auto& it : Players) {
			Cast<AChel>(it)->HideWidgetStas();
			Cast<AChel>(it)->ChangeCorretca_Client(0);
		}
		GS->EventSpawnNote();
	}
	else {
		TArray<AActor*> Players;
		GenAreaObj->GetOverlappingActors(Players, AChel::StaticClass());
		for (auto& it: Players) {
			Cast<AChel>(it)->ChangeCorretca_Client(GenAreaObj->Stadiya);
		}
	}
}

bool AChel::ChangeGeneratorStas_Validate()
{
	return true;
}

void AChel::CallDoThomethinkArea_Implementation()
{
	Cast<AOpenArea>(LastItem)->DoSomethink();
}
bool AChel::CallDoThomethinkArea_Validate()
{
	return true;
}
void AChel::DeleteGameHUD_Implementation()
{
	UserView->RemoveFromParent();
	UserView->Destruct();

	KillFeed->RemoveFromParent();
	KillFeed->Destruct();
	
	Destroy();
}

void AChel::AddDoubleRadiationWidget_Implementation()
{
	UserView->AddDoubleRadiationEffect();
}

void AChel::DisableDoubleRadiationWidget_Implementation()
{
	UserView->DisableDoubleRadiationEffect();
}

void AChel::ChangeCorretca_Client_Implementation(int32 ValueV)
{
	GeneratorView->ChangeCorretcaPosition(ValueV);
}

void AChel::HideWidgetStas_Implementation()
{
	UserView->AreaUsedText->SetVisibility(ESlateVisibility::Visible);
	GeneratorView->SetVisibility(ESlateVisibility::Hidden);
}

void AChel::ChangeButtonCount_Server_Implementation()
{
	FHitResult OutHit;

	FVector StartLocation = CameraComp->GetComponentLocation();
	FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

	FCollisionQueryParams CollisionParams;

	World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
	if (OutHit.GetActor()) {
		AButtonCanalization* TempItem = Cast<AButtonCanalization>(OutHit.GetActor());
		if (TempItem)
		{
			if (!TempItem->DoesRefresh)
			{
				TempItem->Open();
				GS->CurrentButtonCount++;
				if (GS->CurrentButtonCount == 3)
				{
					TArray<AActor*>CanalAreas;
					UGameplayStatics::GetAllActorsOfClass(World, ACanalizationDamageCollision::StaticClass(), CanalAreas);
					if (CanalAreas.Num() != 0)
					{
						Cast<ACanalizationDamageCollision>(CanalAreas[0])->StopRadiation();
					}
				}
			}
		}
	}
}

bool AChel::ChangeButtonCount_Server_Validate()
{
	return true;
}

void AChel::RefreshOutline()
{
	Cast<AChel>(UGameplayStatics::GetPlayerCharacter(World, 0))->RemoveTargetArrowDynamic();
	PoseableMeshComp->SetCustomDepthStencilValue(0);
}

void AChel::DeleteArrowDelay()
{
	switch (ExisType)
	{
	case Boltorez:
	{
		RemoveTargetArrowDynamic(ArrowCanalizacia);
		MainExis_Canalizacia->Mesh->SetCustomDepthStencilValue(0);
		break;
	}
	case KeyShelter:
	{
		RemoveTargetArrowDynamic(ArrowShelter);
		MainExis_Shelter->Mesh->SetCustomDepthStencilValue(0);
		break;
	}
	case Otvertka:
	{
		RemoveTargetArrowDynamic(ArrowVentilacia);
		MainExis_Ventilacia->Mesh->SetCustomDepthStencilValue(0);
		break;
	}
	}
}

void AChel::OutlineBad_Multicast_Implementation()
{
	PoseableMeshComp->SetCustomDepthStencilValue(1);
	
	if (!IsPlayerOwner)
		Cast<AChel>(UGameplayStatics::GetPlayerCharacter(World, 0))->AddTargetArrowDynamic(this);
	FTimerHandle FuzeTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::RefreshOutline, 7, false);
}


void AChel::OutlineBad_Server_Implementation()
{
	if (PoseableMeshComp->CustomDepthStencilValue != 1)
		OutlineBad_Multicast();
}

bool AChel::OutlineBad_Server_Validate()
{
	return true;
}

void AChel::ShowRandomItem_Implementation() {
	TArray<AActor*> Items;
	UGameplayStatics::GetAllActorsOfClass(World, APickableItem::StaticClass(), Items);
	TArray<APickableItem*> ImportantItems;
	ImportantItems.Init(nullptr, 0);
	for (auto& it : Items) 
	{
		if (Cast<APickableItem>(it)->Type <= 2) 
		{
			ImportantItems.Add(Cast<APickableItem>(it));
		}
	}
	if (ImportantItems.Num() != 0)
	{
		LastOutlineItem = ImportantItems[FMath::Rand() % ImportantItems.Num()];
		LastOutlineItem->Item->SetCustomDepthStencilValue(2);
		AddTargetArrowDynamic(LastOutlineItem);

		FTimerHandle FuzeTimerHandle;
		World->GetTimerManager().SetTimer(FuzeTimerHandle, this, &AChel::HideRandomItem, ShowRandomItem_TIME, false);

	}
}

void AChel::HideRandomItem() {
	if (LastOutlineItem) 
	{
		LastOutlineItem->Item->SetCustomDepthStencilValue(0);
		LastOutlineItem = nullptr;
		RemoveTargetArrowDynamic();
	}
}

void AChel::ShowNoiseWebCamUI_Implementation(bool DoesNoise)
{
	//TODO ������� ���
}

void AChel::LockWebCam_Server_Implementation()
{
	FHitResult OutHit;

	FVector StartLocation = CameraComp->GetComponentLocation();
	FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

	FCollisionQueryParams CollisionParams;

	World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
	if (OutHit.GetActor()) {
		AWebCamLocker* TempItem = Cast<AWebCamLocker>(OutHit.GetActor());
		if (TempItem)
		{
			if (TempItem->DoesLock && !TempItem->DoesPlayAnim)
			{
				TempItem->Close();
				TempItem->DoesLock = false;
				if (GS->WebCams[TempItem->Index]->is_Enabled == true)
				{
					GS->WebCams[TempItem->Index]->CurChelix->ShowNoiseWebCamUI(false);
				}
				GS->WebCams[TempItem->Index]->is_Enabled = true;
			}
			else if (!TempItem->DoesLock && !TempItem->DoesPlayAnim)
			{
				TempItem->Open();
				TempItem->DoesLock = true;
				if (GS->WebCams[TempItem->Index]->is_Enabled == false)
				{
					GS->WebCams[TempItem->Index]->CurChelix->ShowNoiseWebCamUI(true);
				}
				GS->WebCams[TempItem->Index]->is_Enabled = false;
			} 
		}
	}
}

bool AChel::LockWebCam_Server_Validate()
{
	return true;
}

void AChel::AddInvisibleServer_Implementation()
{
	InvisibleEverywhere();
}

bool AChel::AddInvisibleServer_Validate()
{
	return true;
}

void AChel::HideNoteWidget_Implementation()
{
	Widget_Note->SetVisibility(ESlateVisibility::Hidden);
	bCanWalkingAndWatching = true;
}

void AChel::RefreshGeneratorArea_Implementation()
{
	UserView->AreaUsedText->SetVisibility(ESlateVisibility::Hidden);
	UserView->HoldText->SetVisibility(ESlateVisibility::Visible);
}

void AChel::AddNumToTerminalServer_Implementation(int32 ButtonType)
{
	UE_LOG(LogTemp, Warning, TEXT("%d"), ButtonType);
	GS->AddNumToTerminal(ButtonType);
}

bool AChel::AddNumToTerminalServer_Validate(int32 ButtonType)
{
	return true;
}

void AChel::DeleteLAstNumServer_Implementation()
{
	GS->DeleteLastNumber();
}

bool AChel::DeleteLAstNumServer_Validate()
{
	return true;
}

void AChel::CheckCodeServer_Implementation()
{
	GS->CheckCode(Index);
}

bool AChel::CheckCodeServer_Validate()
{
	return true;
}

void AChel::ButtonPressAnimationServer_Implementation() {
	FHitResult OutHit;

	FVector StartLocation = CameraComp->GetComponentLocation();
	FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

	FCollisionQueryParams CollisionParams;

	if (World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams))
	{		
		Cast<AClickButton>(OutHit.GetActor())->ButtonPressAnimation();
	}
	
}

bool AChel::ButtonPressAnimationServer_Validate()
{
	return true;
}

void AChel::DoTraceOpenArea() 
{
	FHitResult OutHit;

	FVector StartLocation = CameraComp->GetComponentLocation();
	FVector EndLocation = StartLocation + CameraComp->GetForwardVector() * 300;

	FCollisionQueryParams CollisionParams;

	if (World->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams))
	{
		LastItem = Cast<AOpenArea>(OutHit.GetActor());
	}

}

void AChel::GoToServerOpenArea_Implementation(bool IsStart) 
{
	if (IsStart) 
	{
		DoTraceOpenArea();
		AOpenArea* CurArea = Cast<AOpenArea>(LastItem);
		CurArea->bIsUsed = true;
		CurArea->RotateVentilServer();
	}
	else
	{
		AOpenArea* CurArea = Cast<AOpenArea>(LastItem);
		if (CurArea) {
			CurArea->bIsUsed = false;
			if (Cast<AOpenArea>(LastItem)->CurTimeVentil != 10.f)
			{
				Cast<AOpenArea>(LastItem)->RotateVentilServerReverse();
			}
			LastItem = nullptr;
		}
	}
}

bool AChel::GoToServerOpenArea_Validate(bool IsStart)
{
	return true;
}

void AChel::UpdateTargetArrowPosition(AActor* TargetObj, UTargetArrow* ArrowWidget) {
	if (ArrowWidget && TargetObj) {
		APlayerController* CurPC = UGameplayStatics::GetPlayerController(World, 0);
		//ScreenPosObj = PofigKakNaz(TargetObj);
		PofigKakNaz(TargetObj);
		//CurPC->ProjectWorldLocationToScreen(TargetObj->GetActorLocation(), ScreenPosObj, true);
		int32 ScreenWidth = 0;
		int32 ScreenHeight = 0;
		CurPC->GetViewportSize(ScreenWidth, ScreenHeight);
		

		FVector temp = UKismetMathLibrary::InverseTransformLocation(CameraComp->GetComponentTransform(), TargetObj->GetActorLocation());
		FRotator CurRotation = UKismetMathLibrary::MakeRotFromX(temp);
		//UE_LOG(LogTemp, Warning, TEXT("Screen width %d"), ScreenWidth);
		//UE_LOG(LogTemp, Warning, TEXT("Screen height %d"), ScreenHeight);
		//UE_LOG(LogTemp, Warning, TEXT("ScreenPosObj.X %f"), ScreenPosObj.X);
		//UE_LOG(LogTemp, Warning, TEXT("ScreenPosObj.Y %f"), ScreenPosObj.Y);
		//UE_LOG(LogTemp, Warning, TEXT("You looking at Actor!"));
		if (0 < int32(ScreenPosObj.X) && int32(ScreenPosObj.X) <= ScreenWidth && 0 < int32(ScreenPosObj.Y) && int32(ScreenPosObj.Y) <= ScreenHeight) 
		{ //� ������
			UE_LOG(LogTemp, Warning, TEXT("You looking at Actor!"));
			ArrowWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		else 
		{ //�� � ������
			ArrowWidget->SetVisibility(ESlateVisibility::Visible);
			ArrowWidget->SetRenderTransformAngle(0);
			FVector2D NewPos; //����� ������� ������� �� ������
			if (0 <= int32(ScreenPosObj.X) && int32(ScreenPosObj.X) <= ScreenWidth) 
			{ //������ ��� �����
				float ArrowNewPosX = UKismetMathLibrary::FClamp((CurRotation.Yaw + 45.f) * ScreenWidth / 90.f, ScreenWidth * 0.05, ScreenWidth * 0.95 - 80.f);
				NewPos.X = ArrowNewPosX;
				if (CurRotation.Pitch < 0) 
				{ //�����
					NewPos.Y = ScreenHeight * 0.95 - 80;
					ArrowWidget->SetPositionInViewport(NewPos);
					ArrowWidget->SetRenderTransformAngle((CurRotation.Yaw * -1.f) + 180.f);
				}
				else 
				{ //������
					NewPos.Y = ScreenHeight * 0.05;
					ArrowWidget->SetPositionInViewport(NewPos);
					ArrowWidget->SetRenderTransformAngle(CurRotation.Yaw);
				}
			}
			else 
			{ //����� ��� ������
				float ArrowNewPosY = UKismetMathLibrary::FClamp((ScreenHeight / 2) -(((CurRotation.Pitch + 20.f) * ScreenHeight / 40.f) - (ScreenHeight/2)), ScreenHeight * 0.05, ScreenHeight * 0.95 - 80.f);
				NewPos.Y = ArrowNewPosY;
				if (CurRotation.Yaw > 0) 
				{ //������
					NewPos.X = ScreenWidth * 0.95 - 80.f;
					ArrowWidget->SetPositionInViewport(NewPos);
					ArrowWidget->SetRenderTransformAngle((CurRotation.Pitch * -1.f) + 90.f);
				}
				else 
				{ //�����
					NewPos.X = ScreenWidth * 0.05;
					ArrowWidget->SetPositionInViewport(NewPos);
					ArrowWidget->SetRenderTransformAngle(CurRotation.Pitch - 90.f);
				}
			}
		}
	}
	else if (!TargetObj && ArrowWidget)
	{
		RemoveTargetArrowDynamic(ArrowWidget);
	}

}

void AChel::AddTargetArrowStatic(AActor * TargetObj)
{
	UTargetArrow* Arrow = Cast<UTargetArrow>(CreateWidget(World, TargetArrowClass));
	if (Arrow) 
	{
		Arrow->AddToViewport();
		TargetItemsStatic.Add(TargetObj);
		TargetArrowsStatic.Add(Arrow);
		PlayArrowSound();
	}
}

void AChel::RemoveTargetArrowStatic(AActor * TargetObj)
{
	for (int i = 0; i < TargetItemsStatic.Num(); ++i) 
	{
		if (TargetItemsStatic[i] == TargetObj) 
		{
			TargetArrowsStatic[i]->RemoveFromViewport();
			TargetItemsStatic.RemoveAt(i);
			TargetArrowsStatic.RemoveAt(i);
		}
	}
}

void AChel::AddTargetArrowDynamic(AActor * TargetObj)
{
	UTargetArrow* Arrow = Cast<UTargetArrow>(CreateWidget(World, TargetArrowClass));
	if (Arrow)
	{
		Arrow->AddToViewport();
		TargetItemsDynamic.Add(TargetObj);
		TargetArrowsDynamic.Add(Arrow);
		PlayArrowSound();
	}
}

void AChel::RemoveTargetArrowDynamic() 
{
	if (TargetItemsDynamic.Num() != 0) {
		TargetArrowsDynamic[0]->RemoveFromViewport();
		TargetItemsDynamic.RemoveAt(0);
		TargetArrowsDynamic.RemoveAt(0);
	}
}

void AChel::RemoveTargetArrowDynamic(UTargetArrow* ArrowObj)
{
	for (int i = 0; i < TargetArrowsDynamic.Num(); ++i)
	{
		if (TargetArrowsDynamic[i] == ArrowObj)
		{
			TargetArrowsDynamic[i]->RemoveFromParent();
			TargetArrowsDynamic.RemoveAt(i);
			TargetItemsDynamic.RemoveAt(i);
			PlayArrowSound();
		}
	}
}

void AChel::DeleteStrelkaBadOutline_Client_Implementation(int32 ChelIndex)
{
	RemoveArrowBadOutline(ChelIndex);
}

void AChel::RemoveArrowBadOutline(int32 ChelIndex)
{
	for (int i = 0; i < TargetItemsDynamic.Num(); ++i)
	{
		AChel* BadChel = Cast<AChel>(TargetItemsDynamic[i]);
		if (BadChel && BadChel->Index == ChelIndex)
		{
			TargetArrowsDynamic[i]->RemoveFromParent();
			TargetItemsDynamic.RemoveAt(i);
			TargetArrowsDynamic.RemoveAt(i);
		}
	}
}

void AChel::ShowUIAfterTerminalAndGenerator_Implementation(int32 NewAreaType, bool DoesEnabled)
{
	if (DoesEnabled) {
		AreaCode = NewAreaType;
		if (!GS->AreaAvaliables[NewAreaType])
		{
			if (DoesHave[NewAreaType])
				UserView->HoldText->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		AreaCode = -1;
		UserView->HoldText->SetVisibility(ESlateVisibility::Hidden);
	}
}

void AChel::ResetCacheKeys()
{
	UserView->KeyLeft_Gold->SetText(FText::AsNumber(0));
	UserView->KeyLeft_Silver->SetText(FText::AsNumber(0));
	UserView->KeyLeft_Bronze->SetText(FText::AsNumber(0));
}