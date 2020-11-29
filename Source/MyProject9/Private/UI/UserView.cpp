// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/UserView.h"

#include "Kismet/GameplayStatics.h"
#include "Chel.h"

void UUserView::NativeConstruct() {
	Super::NativeConstruct();
	IsAwake = true;
}

void UUserView::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	if (Animation == CanalizaciaAnim)
	{
		if (Player->IsSuccessOpening) {
			Player->StuffAvaliableUpdate(0);
			HoldText->SetVisibility(ESlateVisibility::Hidden);
			EscapeText->SetVisibility(ESlateVisibility::Visible);

			Player->PlayerOpenAreaUpdate(0);
		}
	}
	else if (Animation == ShelterAnim)
	{
		if (Player->IsSuccessOpening) {
			Player->StuffAvaliableUpdate(1);
			HoldText->SetVisibility(ESlateVisibility::Hidden);
			EscapeText->SetVisibility(ESlateVisibility::Visible);

			Player->PlayerOpenAreaUpdate(1);
		}
	}
	else if (Animation == VentilaciaAnim)
	{
		if (Player->IsSuccessOpening) {
			Player->StuffAvaliableUpdate(2);
			HoldText->SetVisibility(ESlateVisibility::Hidden);
			EscapeText->SetVisibility(ESlateVisibility::Visible);

			Player->PlayerOpenAreaUpdate(2);
		}
	}
	else if (Animation == Shading)
	{
		if (!IsAwake)
		{
			SetVisibility(ESlateVisibility::Visible);
		}
	}
	PB_Opening->SetVisibility(ESlateVisibility::Hidden);
	TimeLeft->SetVisibility(ESlateVisibility::Hidden);
}