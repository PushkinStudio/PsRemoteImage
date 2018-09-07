// Copyright 2018 Mail.Ru Group. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class SCircularThrobber;

class SImageWithThrobber : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SImageWithThrobber)
	{}
	
	/** Image resource */
	SLATE_ATTRIBUTE(const FSlateBrush*, Image)
	
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	
	/** Set the Image attribute */
	void SetImage(TAttribute<const FSlateBrush*> InImage);
	
	/** Toggle progress indicator */
	void SetShowProgress(bool bShow);
	
private:
	/** Image object */
	TSharedPtr<SImage> Image;

	/** Progress indicator object */
	TSharedPtr<SCircularThrobber> Progress;
};
