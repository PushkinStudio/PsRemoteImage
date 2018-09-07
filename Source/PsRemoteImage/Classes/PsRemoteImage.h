// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Http.h"

#include "PsRemoteImage.generated.h"

class SImageWithThrobber;
struct FSlateDynamicImageBrush;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FImageLoaded);

UCLASS()
class PSREMOTEIMAGE_API UPsRemoteImage : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	
#if WITH_EDITOR
	/** Get the palette category of the widget */
	const FText GetPaletteCategory() override;
#endif
	
	/** Set image URL */
	UFUNCTION(BlueprintCallable, Category=PSRemoteImage)
	void SetURL(FString InURL);
	
	/** Set image URL */
	UFUNCTION(BlueprintCallable, Category=PSRemoteImage)
	void SetShowProgress(bool bInShowProgress);
	
	/** Image loaded delegate */
	UPROPERTY(BlueprintAssignable)
	FImageLoaded OnImageLoaded;
	
protected:
	/** Begin UWidget Interface */
	TSharedRef<SWidget> RebuildWidget() override;
	/** End UWidget Interface */
	
	/** Begin UVisual Interface */
	void ReleaseSlateResources(bool bReleaseChildren) override;
	/** End UVisual Interface */
	
	/** Apply properties to widget */
	void SynchronizeProperties() override;
	
protected:
	/** Initial image URL */
	UPROPERTY(EditAnywhere, Category=PSRemoteImage)
	FString URL;
	
	/** Toggle show progress indicator while loading */
	UPROPERTY(EditAnywhere, Category=PSRemoteImage)
	bool bShowProgress;
	
private:
	/** Load possibly cached image with URL */
	void LoadImage(FString InURL);
	
	/** Load image complete callback */
	void LoadImageComplete();
	
	/** Check whether image is cached */
	bool IsImageCached(const FString& InURL) const;
	
	/** Get cache file filename */
	FString GetCacheFilename(const FString& InURL) const;
	
	/** Add image data to cache */
	void AsyncAddImageToCache(const FString& InURL, const TArray<uint8>& InImageData);
	
	/** Get cache file creation date */
	FString GetCacheDate(const FString& InURL) const;
	
	/** Remove image from cache */
	void RemoveImageFromCache(const FString& InURL);
	
	/** Load image data from cache */
	void AsyncLoadCachedImage(const FString& InURL);
	
	/** Success callback of loading cached image */
	void LoadCachedImageSuccess(const FString& InURL);
	
	/** Success callback of loading cached image */
	void LoadCachedImageFailure(const FString& InURL);
	
	/** Download image from network */
	void AsyncDownloadImage(const FString& InURL);
	
	/** Callback of downloading image */
	void DownloadImage_HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	/** Download image from network if it is newer that cached file */
	void AsyncDownloadImageCheckCache(const FString& InURL);
	
	/** Callback of downloading image with cache check */
	void DownloadImageCheckCache_HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	/** Update Slate image with image data */
	bool SetImageData(const TArray<uint8>& InImageData);
	
	/** Create brush from image data */
	TSharedPtr<FSlateDynamicImageBrush> CreateBrush(const TArray<uint8>& InImageData) const;
	
private:
	/** Native Slate Widget */
	TSharedPtr<SImageWithThrobber> SlateImage;
	
	/** Brush to draw */
	TSharedPtr<FSlateDynamicImageBrush> Brush;
	
	/** Loading in progress */
	bool bWorking;
	
	/** URL has changed while working */
	bool bPendingWork;
	
	/** Image data buffer */
	TArray<uint8> ImageData;
};
