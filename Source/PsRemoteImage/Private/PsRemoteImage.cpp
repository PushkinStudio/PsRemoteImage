// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#include "PsRemoteImagePrivatePCH.h"
#include "PsRemoteImage.h"
#include "SImageWithThrobber.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/DateTime.h"
#include "Async/Async.h"
#include "HAL/PlatformFilemanager.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateDynamicImageBrush.h"

#define LOCTEXT_NAMESPACE "PsRemoteImageModule"

UPsRemoteImage::UPsRemoteImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowProgress = true;
	bWorking = false;
	bPendingWork = false;
}

#if WITH_EDITOR
const FText UPsRemoteImage::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}
#endif // WITH_EDITOR

void UPsRemoteImage::SetURL(FString InURL)
{
	URL = InURL;
	LoadImage(URL);
}

void UPsRemoteImage::SetShowProgress(bool bInShowProgress)
{
	bShowProgress = bInShowProgress;
}

TSharedRef<SWidget> UPsRemoteImage::RebuildWidget()
{
	SlateImage = SNew(SImageWithThrobber);
	return SlateImage.ToSharedRef();
}

void UPsRemoteImage::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	
	SlateImage.Reset();
}

void UPsRemoteImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	
	LoadImage(URL);
}

void UPsRemoteImage::LoadImage(FString InURL)
{
	if (InURL.IsEmpty())
	{
		LoadImageComplete();
		return;
	}
	
	if (bWorking)
	{
		bPendingWork = true;
		UE_LOG(LogPsRemoteImage, Warning, TEXT("%s work in progress, postponing image loading"), *VA_FUNC_LINE);
		return;
	}
	
	bWorking = true;
	
	if (SlateImage.IsValid())
	{
		SlateImage->SetShowProgress(bShowProgress && true);
	}
	
	const bool bImageCached = IsImageCached(InURL);
	if (bImageCached)
	{
		AsyncLoadCachedImage(InURL);
	}
	else
	{
		AsyncDownloadImage(InURL);
	}
}

void UPsRemoteImage::LoadImageComplete()
{
	ImageData.Empty();
	bWorking = false;
	
	if (SlateImage.IsValid())
	{
		SlateImage->SetShowProgress(false);
	}
	
	if (bPendingWork)
	{
		bPendingWork = false;
		LoadImage(URL);
	}
	else
	{
		OnImageLoaded.Broadcast();
	}
}

bool UPsRemoteImage::IsImageCached(const FString& InURL) const
{
	const bool bCached = !InURL.IsEmpty() && FPaths::FileExists(GetCacheFilename(InURL));
	
	return bCached;
}

FString UPsRemoteImage::GetCacheFilename(const FString& InURL) const
{
	if (InURL.IsEmpty())
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("GetCacheFilename empty URL"));
		return FString();
	}
	
	const FString Hash = FMD5::HashAnsiString(*InURL);
	const FString Filename = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir()), TEXT("PSRemoteImage"), TEXT("Cache"), FString::Printf(TEXT("%s.bin"), *Hash));
	return Filename;
}

void UPsRemoteImage::AsyncAddImageToCache(const FString &InURL, const TArray<uint8>& InImageData)
{
	Async<void>(EAsyncExecution::ThreadPool, [this, InURL, &InImageData]()
	{
		const FString CacheFilename = GetCacheFilename(InURL);
		FFileHelper::SaveArrayToFile(InImageData, *CacheFilename);
		
		AsyncTask(ENamedThreads::GameThread, [this, InURL]()
		  {
			  LoadImageComplete();
		  });
	});
}

FString UPsRemoteImage::GetCacheDate(const FString& InURL) const
{
	const FString CacheFilename = GetCacheFilename(InURL);
	const FDateTime ModificationDate = FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*CacheFilename);
	return ModificationDate.ToHttpDate();
}

void UPsRemoteImage::RemoveImageFromCache(const FString& InURL)
{
	const FString CacheFilename = GetCacheFilename(InURL);
	const bool bDeleted = FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*CacheFilename);
	
	if (!bDeleted)
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't delete cached image"), *VA_FUNC_LINE);
	}
}

void UPsRemoteImage::AsyncLoadCachedImage(const FString& InURL)
{
	Async<void>(EAsyncExecution::ThreadPool, [this, InURL]()
	{
		const FString CacheFilename = GetCacheFilename(InURL);
		TArray<uint8> ReadImageData;
		const bool bReadSuccess = FFileHelper::LoadFileToArray(ReadImageData, *CacheFilename);
		if (bReadSuccess)
		{
			ImageData = ReadImageData;
			
			AsyncTask(ENamedThreads::GameThread, [this, InURL]()
			  {
				  LoadCachedImageSuccess(InURL);
			  });
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, InURL]()
			  {
				  LoadCachedImageFailure(InURL);
			  });
		}
	});
}

void UPsRemoteImage::LoadCachedImageSuccess(const FString& InURL)
{
	const bool bLoadedImage = SetImageData(ImageData);
	
	if (bLoadedImage)
	{
		if (SlateImage.IsValid())
		{
			SlateImage->SetShowProgress(false);
		}
		
		AsyncDownloadImageCheckCache(InURL);
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't load cached image, trying to download it"), *VA_FUNC_LINE);
		RemoveImageFromCache(InURL);
		AsyncDownloadImage(InURL);
	}
}

void UPsRemoteImage::LoadCachedImageFailure(const FString& InURL)
{
	RemoveImageFromCache(InURL);
	AsyncDownloadImage(InURL);
}

void UPsRemoteImage::AsyncDownloadImage(const FString& InURL)
{
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPsRemoteImage::DownloadImage_HttpRequestComplete);
	HttpRequest->SetURL(InURL);
	HttpRequest->SetVerb(TEXT("GET"));
	
	HttpRequest->ProcessRequest();
}

void UPsRemoteImage::DownloadImage_HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid())
	{
		ImageData = Response->GetContent();
		
		const bool bLoadedImage = SetImageData(ImageData);
		if (bLoadedImage)
		{
			AsyncAddImageToCache(*Request->GetURL(), ImageData);
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s failed to download image"), *VA_FUNC_LINE);
		LoadImageComplete();
	}
}

void UPsRemoteImage::AsyncDownloadImageCheckCache(const FString& InURL)
{
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPsRemoteImage::DownloadImageCheckCache_HttpRequestComplete);
	HttpRequest->SetURL(InURL);
	HttpRequest->SetVerb(TEXT("GET"));
	
	const FString CacheFileDate = GetCacheDate(InURL);
	HttpRequest->SetHeader(TEXT("If-Modified-Since"), CacheFileDate);
	
	HttpRequest->ProcessRequest();
}

void UPsRemoteImage::DownloadImageCheckCache_HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (Response.IsValid())
	{
		if (Response->GetResponseCode() == 304)
		{
			LoadImageComplete();
			return;
		}
		else
		{
			DownloadImage_HttpRequestComplete(Request, Response, bWasSuccessful);
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s invalid response"), *VA_FUNC_LINE);
		LoadImageComplete();
	}
}

bool UPsRemoteImage::SetImageData(const TArray<uint8>& InImageData)
{
	if (SlateImage.IsValid())
	{
		SlateImage->SetImage(nullptr);
	}
	
	Brush = CreateBrush(InImageData);
	
	if (Brush.IsValid())
	{
		if (SlateImage.IsValid())
		{
			SlateImage->SetImage(Brush.Get());
			return true;
		}
		else
		{
			UE_LOG(LogPsRemoteImage, Error, TEXT("%s invalid SlateImage"), *VA_FUNC_LINE);
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s invalid Brush"), *VA_FUNC_LINE);
	}
	
	return false;
}

TSharedPtr<FSlateDynamicImageBrush> UPsRemoteImage::CreateBrush(const TArray<uint8>& InImageData) const
{
	TSharedPtr<FSlateDynamicImageBrush> BrushRet;
	FName ResourceName = FName(*FString::Printf(TEXT("PSRemoteImage_%s"), *FGuid::NewGuid().ToString()));
	
	TArray<uint8> DecodedImage;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	const EImageFormat ImageType = ImageWrapperModule.DetectImageFormat(InImageData.GetData(), InImageData.Num());
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageType);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("Resource '%s' is not a supported image type"), *ResourceName.ToString());
		return nullptr;
	}
	
	if (ImageWrapper->SetCompressed(InImageData.GetData(), InImageData.Num()))
	{
		const int32 BytesPerPixel = ImageWrapper->GetBitDepth();
		const int32 Width = ImageWrapper->GetWidth();
		const int32 Height = ImageWrapper->GetHeight();
		
		const TArray<uint8>* RawData = nullptr;
		
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, BytesPerPixel, RawData) && RawData)
		{
			DecodedImage = *RawData;
		}
		else
		{
			UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't get raw data"), *VA_FUNC_LINE);
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't load compressed data"), *VA_FUNC_LINE);
		return nullptr;
	}
	
	if (DecodedImage.Num() <= 0)
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s raw data is empty"), *VA_FUNC_LINE);
		return nullptr;
	}
	
	const bool bGeneratedResource = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(ResourceName, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), DecodedImage);
	
	if (bGeneratedResource)
	{
		BrushRet = MakeShareable(new FSlateDynamicImageBrush(ResourceName, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight())));
		return BrushRet;
	}
	else
	{
		UE_LOG(LogPsRemoteImage, Error, TEXT("%s can't generate resource"), *VA_FUNC_LINE);
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
