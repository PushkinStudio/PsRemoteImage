// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#include "PsRemoteImagePrivatePCH.h"
#include "SImageWithThrobber.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"

void SImageWithThrobber::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
	 SNew(SOverlay)
	 
	 + SOverlay::Slot()
	 .HAlign(HAlign_Center)
	 .VAlign(VAlign_Center)
	 [
	  SAssignNew(Progress, SCircularThrobber)
	  ]
	 
	 + SOverlay::Slot()
	 [
	  SAssignNew(Image, SImage)
	  .Visibility(EVisibility::Collapsed)
	  ]
	 ];
}

void SImageWithThrobber::SetImage(TAttribute<const FSlateBrush*> InImage)
{
	Image->SetImage(InImage);
}

void SImageWithThrobber::SetShowProgress(bool bShow)
{
	Progress->SetVisibility(bShow ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	Image->SetVisibility(bShow ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
}
